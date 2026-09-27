# Plano de implementação — motor v3 de ponteiro móvel (Low Latency)

> **Para agentes:** SUB-SKILL OBRIGATÓRIA: `superpowers:executing-plans` (execução inline nesta
> sessão) ou `superpowers:subagent-driven-development`. Os passos usam caixas (`- [ ]`) para
> acompanhamento.
>
> **Data:** 2026-09-01 · **Status:** ✅ **EXECUTADO** — a Etapa 6 foi concluída em 2026-09-02, e
> o que aconteceu ao executar está no
> [diário](execucao-do-plano.md#etapa-6--motor-v3-de-ponteiro-móvel-low-latency). As caixas
> abaixo ficam como foram escritas, sem marcar: este documento é o **previsto**, o diário é o
> **acontecido**, e a comparação entre os dois só existe enquanto nenhum dos dois for reescrito.
> **Branch:** `docs/antares-parity-and-low-latency` — todo o trabalho fica nela; **sem merge**.
> **Commits:** mensagens em **inglês**, uma por tarefa, **sem trailer de co-autoria**.

**Objetivo:** um segundo motor de síntese, o ponteiro móvel da patente US 5.973.252, selecionável
por um botão **Low Latency** no plugin e por `motor=`/`lowlat=` no CLI, com o TD-PSOLA intocado
como motor padrão e referência.

**Arquitetura:** `MotorPonteiro` (classe RT-safe em `dsp.h`) lê o áudio de um anel a velocidade
β e salta um período quando a distância entre leitura e escrita sai de `[margem, margem + T]`.
`AutotuneStream` ganha `StreamParams::motor`; com Ponteiro, `process()` chama o motor por amostra
com a decisão de `nFrame + look·nHop` amostras atrás, e não chama `avancarPsola()`. Detecção e
decisão não mudam.

**Stack:** C++17 header-only (`dsp.h`, `autotune_stream.h`), JUCE (plugin), bash (`baseline.sh`),
Python 3 + numpy + soundfile (`.venv/` do repositório) para a medição.

**Especificação:** [`docs/especificacao-v3-ponteiro.md`](especificacao-v3-ponteiro.md) — o plano
argumenta a partir dela; quem executa lê as duas.

## Restrições globais

- Os 30 casos existentes do `baseline.sh` têm de dar `IDENTICO`, e os 6 invariantes + 19
  legados `ok`, **depois de cada tarefa** que toca `src/` ou `baseline.sh`.
- Nada de alocação dentro de `MotorPonteiro::processar()`.
- Código, comentários e docs em **português**, no estilo didático do repositório (o *porquê* nos
  comentários é a norma). Identificadores em português.
- Sem dependência nova. `dr_wav.h` e JUCE continuam sendo as únicas.
- `tcc-texto/` **não é tocado**.
- `MARGEM = 8` amostras; interpolação Catmull-Rom de 4 pontos; crossfade `min(T/2, 64)`.
- Defasagem da correção no motor de ponteiro: exatamente `nFrame + look·nHop` amostras.
- Compilar com `clang++ -std=c++17 -O2 -I external` (o que o `baseline.sh` usa no macOS).

## Mapa de arquivos

| Arquivo | Ação | Responsabilidade |
|---|---|---|
| `src/core/dsp.h` | modificar (acrescentar no fim, antes de `#endif`) | `MotorPonteiro` |
| `src/tests/test_ponteiro.cpp` | criar | teste de unidade do motor isolado |
| `src/c1_streaming/autotune_stream.h` | modificar | `MotorSintese`, `StreamParams::motor`, `prepare()`, `process()`, `passoPonteiro()`, getters de medição |
| `src/c1_streaming/stream_test.cpp` | modificar | flags `motor=`, `lowlat=`; relatório |
| `baseline.sh` | modificar | 7 casos novos, 2 invariantes novos |
| `baseline/resumo.txt` | regravar via `./baseline.sh gravar` | referência com os casos novos |
| `plugin/PluginProcessor.h/.cpp` | modificar | parâmetro `lowlat`, `aplicarParametros()` |
| `plugin/PluginEditor.h/.cpp` | modificar | botão Low Latency, look-ahead desabilitado, rótulo do motor e latência |
| `python/medir_v3.py` | criar | medição PSOLA × Ponteiro × Low Latency |
| `docs/execucao-do-plano.md` | modificar | Etapa 6 |
| `docs/historico-e-decisoes.md` | modificar | Decisão 9 |
| `docs/modo-baixa-latencia.md`, `docs/analise-v1-v2-v3.md`, `docs/pesquisa-latencia-antares.md` | modificar (cabeçalho de status) | apontar para a spec e a Etapa 6 |
| `README.md`, `CLAUDE.md`, `docs/README.md`, `plugin/README.md`, `python/README.md` | modificar | flags, botão, verificação, índice |

---

### Tarefa 1: `MotorPonteiro` em `dsp.h`, com teste de unidade

**Arquivos:**
- Modificar: `src/core/dsp.h` (acrescentar antes de `#endif // DSP_H`, depois de `psolaSintetiza`)
- Criar: `src/tests/test_ponteiro.cpp`

**Interfaces:**
- Produz: `class MotorPonteiro { void prepare(int fsHz, double fminHz); void reset(); int latencia() const; float processar(float x, double f0Hz, double fAlvoHz); long long saltos() const; double distMedia() const; double distMax() const; static const int MARGEM = 8; }`

- [ ] **Passo 1: escrever o teste que falha**

`src/tests/test_ponteiro.cpp`:

```cpp
// ---------------------------------------------------------------------------
//  test_ponteiro.cpp — verificacao do motor v3 (ponteiro movel), isolado do
//  streaming (Etapa 6 do diario; spec em docs/especificacao-v3-ponteiro.md).
//
//  O que so um teste pega:
//    1) IDENTIDADE EXATA — com beta = 1 (ou sem voz) o motor tem de devolver a
//       entrada atrasada de MARGEM amostras BIT A BIT. E' a condicao para que
//       tol=600 e mix=0 continuem sendo os dois caminhos de identidade do
//       projeto tambem no motor novo.
//    2) A NOTA SAI CERTA — com beta = 1,05 a frequencia medida na saida tem de
//       subir 5 %. Isso verifica que o salto de um periodo nao "devolve" o
//       deslocamento (um erro classico: saltar de T e ler a T/beta).
//    3) SEM CLIQUE — o maior degrau amostra-a-amostra da saida nao pode passar
//       de 1,5x o da entrada. Em senoide o salto de um periodo e' invisivel;
//       se aparecer degrau, a emenda esta fora de fase.
//    4) DISTANCIA LIMITADA — a parte variavel da latencia fica em
//       [MARGEM-1, MARGEM+T+1]. Se passar disso, o teste de distancia esta errado.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_ponteiro.cpp -o test_ponteiro
//  Rodar:     ./test_ponteiro      (0 = tudo certo, 1 = falhou)
// ---------------------------------------------------------------------------
#include "../core/dsp.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>

static int falhas = 0;
static void checar(bool ok, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (!ok) ++falhas;
    std::printf("  %s  ", ok ? "ok  " : "FALHA");
    std::vprintf(fmt, ap); std::printf("\n");
    va_end(ap);
}

static const int FS = 44100;

static std::vector<float> senoide(double f, double seg, double amp = 0.5) {
    std::vector<float> x((size_t)(seg * FS));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float)(amp * std::sin(2 * PI * f * i / FS));
    return x;
}

// Frequencia media por cruzamentos de zero ascendentes em [ini, fim).
static double freqPorCruzamentos(const std::vector<float>& y, size_t ini, size_t fim) {
    long long n = 0; size_t primeiro = 0, ultimo = 0;
    for (size_t i = ini + 1; i < fim; ++i)
        if (y[i - 1] <= 0.0f && y[i] > 0.0f) { if (n == 0) primeiro = i; ultimo = i; ++n; }
    if (n < 2) return 0.0;
    return (double)(n - 1) * FS / (double)(ultimo - primeiro);
}

static double maiorDegrau(const std::vector<float>& y, size_t ini, size_t fim) {
    double m = 0; for (size_t i = ini + 1; i < fim; ++i) m = std::max(m, (double)std::fabs(y[i] - y[i - 1]));
    return m;
}

// Roda o motor com f0/fAlvo constantes (ou funcao do indice) e devolve a saida.
template <class F0, class FA>
static std::vector<float> rodar(MotorPonteiro& m, const std::vector<float>& x, F0 f0, FA fa) {
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = m.processar(x[i], f0(i), fa(i));
    return y;
}

int main() {
    const int M = MotorPonteiro::MARGEM;

    std::printf("== 1. beta = 1: identidade atrasada de MARGEM, bit a bit ==\n");
    {
        MotorPonteiro m; m.prepare(FS, 80.0);
        auto x = senoide(220.0, 1.0);
        auto y = rodar(m, x, [](size_t){ return 220.0; }, [](size_t){ return 220.0; });
        bool ok = true;
        for (size_t i = (size_t)M; i < x.size(); ++i) if (y[i] != x[i - M]) { ok = false; break; }
        checar(m.latencia() == M, "latencia() == MARGEM (%d)", M);
        checar(ok, "y[n] == x[n-%d] em todas as amostras", M);
        checar(m.saltos() == 0, "nenhum salto com beta = 1");
    }

    std::printf("== 2. sem voz (f0 = 0): identidade atrasada ==\n");
    {
        MotorPonteiro m; m.prepare(FS, 80.0);
        auto x = senoide(220.0, 1.0);
        auto y = rodar(m, x, [](size_t){ return 0.0; }, [](size_t){ return 0.0; });
        bool ok = true;
        for (size_t i = (size_t)M; i < x.size(); ++i) if (y[i] != x[i - M]) { ok = false; break; }
        checar(ok, "y[n] == x[n-%d] sem voz", M);
        checar(m.saltos() == 0, "nenhum salto sem voz");
    }

    auto casoBeta = [&](double f0, double beta, const char* nome) {
        std::printf("== %s: f0 = %.0f Hz, beta = %.2f ==\n", nome, f0, beta);
        MotorPonteiro m; m.prepare(FS, 80.0);
        auto x = senoide(f0, 2.0);
        auto y = rodar(m, x, [=](size_t){ return f0; }, [=](size_t){ return f0 * beta; });
        const size_t ini = FS / 4, fim = x.size() - FS / 4;   // regiao estavel
        const double fMed = freqPorCruzamentos(y, ini, fim), fEsp = f0 * beta;
        checar(std::fabs(fMed - fEsp) / fEsp < 0.005, "frequencia medida %.2f Hz, esperada %.2f (tol 0,5%%)", fMed, fEsp);
        checar(m.saltos() > 0, "houve saltos (%lld)", m.saltos());
        const double dIn = maiorDegrau(x, ini, fim), dOut = maiorDegrau(y, ini, fim);
        checar(dOut <= 1.5 * dIn, "maior degrau saida %.4f <= 1,5 x entrada %.4f (sem clique)", dOut, dIn);
        const double T = FS / f0;
        checar(m.distMax() <= M + T + 1.0, "distMax %.1f <= MARGEM + T + 1 = %.1f", m.distMax(), M + T + 1.0);
        checar(m.distMedia() >= M - 1.0, "distMedia %.1f >= MARGEM - 1", m.distMedia());
    };
    casoBeta(220.0, 1.05, "3. sobe");
    casoBeta(220.0, 0.95, "4. desce");

    std::printf("== 5. salto de nota no meio (220 -> 330 Hz), beta = 1,03 ==\n");
    {
        MotorPonteiro m; m.prepare(FS, 80.0);
        const size_t meio = FS;                      // 1 s de cada
        std::vector<float> x(2 * FS);
        for (size_t i = 0; i < x.size(); ++i) {
            double f = (i < meio) ? 220.0 : 330.0;
            // fase continua na troca, para que o degrau (se houver) seja do motor
            static double fase = 0; fase += 2 * PI * f / FS; x[i] = (float)(0.5 * std::sin(fase));
        }
        auto y = rodar(m, x, [=](size_t i){ return i < meio ? 220.0 : 330.0; },
                             [=](size_t i){ return (i < meio ? 220.0 : 330.0) * 1.03; });
        const double dIn = maiorDegrau(x, FS / 4, x.size() - FS / 4);
        const double dOut = maiorDegrau(y, FS / 4, x.size() - FS / 4);
        checar(dOut <= 1.5 * dIn, "maior degrau %.4f <= 1,5 x %.4f na troca de nota", dOut, dIn);
        checar(m.distMax() <= M + FS / 220.0 + 1.0, "distMax %.1f respeita o T maior (220 Hz)", m.distMax());
        const double f2 = freqPorCruzamentos(y, meio + FS / 4, x.size() - FS / 8);
        checar(std::fabs(f2 - 330.0 * 1.03) / (330.0 * 1.03) < 0.005, "segunda nota sai em %.2f Hz (esp. %.2f)", f2, 330.0 * 1.03);
    }

    std::printf("\n%s (%d falha(s))\n", falhas ? "FALHOU" : "TUDO CERTO", falhas);
    return falhas ? 1 : 0;
}
```

- [ ] **Passo 2: rodar e ver falhar**

```bash
clang++ -std=c++17 -O2 -I external src/tests/test_ponteiro.cpp -o /tmp/test_ponteiro
```
Esperado: erro de compilação, `MotorPonteiro` não declarado.

- [ ] **Passo 3: implementar o motor**

Acrescentar em `src/core/dsp.h`, imediatamente antes de `#endif // DSP_H`:

```cpp
// ----------------------------------------------------------------------------
//  MotorPonteiro — v3: correcao de altura por PONTEIRO DE LEITURA MOVEL, no
//  lugar do TD-PSOLA. E' o mecanismo da patente do Auto-Tune (US 5.973.252,
//  Hildebrand 1999) e o que o produto chama de "Classic Mode". Especificacao
//  completa em docs/especificacao-v3-ponteiro.md; a razao de existir esta em
//  docs/analise-v1-v2-v3.md: enquanto a sintese for PSOLA, a latencia e'
//  proporcional a fs/FMIN por construcao. Este motor tem latencia FIXA de
//  MARGEM amostras, mais uma parte VARIAVEL (0..T) que depende da nota cantada.
//
//  Como funciona, por amostra:
//    1. a entrada e' escrita num anel (ponteiro W, avanca 1 por amostra);
//    2. a saida e' LIDA do mesmo anel num ponteiro fracionario R que avanca
//       beta = fAlvo/f0 por amostra (beta > 1 = le mais rapido = sobe a nota);
//    3. como os dois andam a taxas diferentes, a distancia W - R muda. Quando
//       ela sai de [MARGEM, MARGEM + T], soma-se ou subtrai-se EXATAMENTE um
//       periodo T = fs/f0 de R: um ciclo e' repetido (subindo) ou descartado
//       (descendo). Como o salto e' de um periodo inteiro, as duas pontas da
//       emenda estao no mesmo ponto do ciclo -- e' a mesma ideia que sustenta
//       o PSOLA, sem quadros, sem marcas e sem janelas.
//
//  O que ele NAO faz: nao preserva formantes. Reamostrar desloca o envelope
//  espectral por |beta - 1|. Medido em docs/pesquisa-latencia-antares.md §10:
//  teto de 2,93 % em escala cromatica, abaixo do limiar tipico de 5 %.
//
//  Invariante que o teste verifica: com beta = 1 a saida e' a entrada
//  atrasada de MARGEM amostras, BIT A BIT. Depende de tres coisas: R nunca
//  sai da grade inteira (soma 1.0 exata), a interpolacao em fracao zero
//  devolve a amostra crua, e nenhum salto dispara.
//
//  RT-safe: o anel e' alocado em prepare(); processar() nao aloca.
// ----------------------------------------------------------------------------
class MotorPonteiro {
public:
    // Distancia minima entre leitura e escrita, em amostras. E' a latencia FIXA
    // declarada ao host. 8 porque a interpolacao cubica le ate i+2 e a distancia
    // cai no maximo (beta - 1) < 1 por amostra antes de o salto disparar: sobra
    // folga. (A Antares declara 37; o interpolador dela e' mais longo.)
    static const int MARGEM = 8;

    void prepare(int fsHz, double fminHz) {
        fs = fsHz;
        // Precisa caber: a margem, ate T atras (salto para tras), mais um T de
        // folga para o ponteiro velho do crossfade, mais os 2 pontos que a
        // interpolacao le a frente. Potencia de 2 para que o wrap seja um '&'.
        const double T = (double)fs / std::max(1.0, fminHz);
        long long precisa = MARGEM + (long long)std::ceil(2.0 * T) + 8;
        size_t n = 16; while ((long long)n < precisa) n <<= 1;
        anel.assign(n, 0.0f); mask = n - 1;
        reset();
    }

    void reset() {
        std::fill(anel.begin(), anel.end(), 0.0f);
        // W comeca em N (nao em 0) para que R = W - MARGEM seja >= 0 e o
        // priming leia zeros do anel, sem indice negativo.
        W = (long long)anel.size(); R = (double)W - MARGEM; Rvelho = R;
        xfResta = 0; xfLen = 1;
        nSaltos = 0; somaDist = 0.0; nDist = 0; maxDist = 0.0;
    }

    int latencia() const { return MARGEM; }

    // f0Hz <= 0: sem voz -> beta = 1, nenhum salto (a distancia fica onde esta).
    float processar(float x, double f0Hz, double fAlvoHz) {
        anel[(size_t)(W & (long long)mask)] = x; ++W;
        const double beta = (f0Hz > 0.0 && fAlvoHz > 0.0) ? fAlvoHz / f0Hz : 1.0;

        double y = ler(R);
        if (xfResta > 0) {
            // Crossfade linear entre a leitura ANTIGA (que continua andando a
            // beta) e a nova. Em sinal periodico as duas sao quase iguais e o
            // crossfade e' quase nulo; em consoante/ataque ele transforma o
            // degrau numa modulacao curta de amplitude.
            const double w = 1.0 - (double)xfResta / (double)xfLen;
            y = (1.0 - w) * ler(Rvelho) + w * y;
            Rvelho += beta; --xfResta;
            // O ponteiro velho de um salto para tras estava perto da escrita e
            // continua se aproximando: se ameacar ler o futuro, encerra antes.
            if ((double)W - Rvelho < 4.0) xfResta = 0;
        }
        R += beta;

        if (f0Hz > 0.0) {
            const double T = (double)fs / f0Hz;
            const double dist = (double)W - R;
            if      (dist < (double)MARGEM)     saltar(-T, T);   // repete um ciclo
            else if (dist > (double)MARGEM + T) saltar(+T, T);   // descarta um ciclo
            const double d2 = (double)W - R;
            somaDist += d2; ++nDist; if (d2 > maxDist) maxDist = d2;
        }
        return (float)y;
    }

    long long saltos()    const { return nSaltos; }
    double    distMedia() const { return nDist ? somaDist / (double)nDist : (double)MARGEM; }
    double    distMax()   const { return maxDist; }

private:
    // Catmull-Rom de 4 pontos em torno de floor(pos). Em fracao ZERO devolve a
    // amostra crua sem passar pela aritmetica -- e' o que garante a identidade
    // bit a bit com beta = 1.
    double ler(double pos) const {
        const long long i = (long long)std::floor(pos);
        const double f = pos - (double)i;
        const double x0 = anel[(size_t)(i & (long long)mask)];
        if (f == 0.0) return x0;
        const double xm1 = anel[(size_t)((i - 1) & (long long)mask)];
        const double x1  = anel[(size_t)((i + 1) & (long long)mask)];
        const double x2  = anel[(size_t)((i + 2) & (long long)mask)];
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    void saltar(double delta, double T) {
        Rvelho = R; R += delta;
        xfLen = std::max(1, std::min(64, (int)std::llround(T / 2.0)));
        xfResta = xfLen;
        ++nSaltos;
    }

    int fs = 44100;
    std::vector<float> anel; size_t mask = 0;
    long long W = 0;             // escrita (absoluto)
    double R = 0.0, Rvelho = 0.0;// leitura (absoluto, fracionario) e leitura antiga do crossfade
    int xfResta = 0, xfLen = 1;
    long long nSaltos = 0; double somaDist = 0.0; long long nDist = 0; double maxDist = 0.0;
};
```

- [ ] **Passo 4: rodar o teste até passar**

```bash
clang++ -std=c++17 -O2 -I external src/tests/test_ponteiro.cpp -o /tmp/test_ponteiro && /tmp/test_ponteiro
```
Esperado: `TUDO CERTO (0 falha(s))`. Se o item 3 falhar na frequência, o erro está no sinal
de `delta` em `saltar()`; se falhar no degrau, o erro está em `ler()` (índice `i−1`/`i+2`).

- [ ] **Passo 5: conferir que a linha de base não mudou** (o `baseline.sh` compila e roda os
  testes de `src/tests/` automaticamente, então ele passa a rodar este)

```bash
./baseline.sh conferir
```
Esperado: `ok    test_ponteiro` na seção de testes; `IDENTICO — nada mudou.`

- [ ] **Passo 6: commit**

```bash
git add src/core/dsp.h src/tests/test_ponteiro.cpp
git commit -m "feat(core): add the moving-pointer synthesis engine with unit test"
```

---

### Tarefa 2: seleção do motor em `AutotuneStream`

**Arquivos:**
- Modificar: `src/c1_streaming/autotune_stream.h` (`StreamParams`, `prepare()`, `reset()`, `process()`, membros)

**Interfaces:**
- Consome: `MotorPonteiro` da Tarefa 1.
- Produz: `enum class MotorSintese { PSOLA, Ponteiro };` · `StreamParams::motor` · `int getLatencySamples()` (já existe; passa a refletir o motor) · `long long getSaltosPonteiro() const` · `double getDistMediaPonteiro() const` · `double getDistMaxPonteiro() const` · `int getDefasagemCorrecao() const`.

- [ ] **Passo 1: o enum e o parâmetro**

Antes de `struct StreamParams`:

```cpp
// Etapa 6: qual motor executa o estagio 3 (a sintese). O estagio 1 (pYIN +
// Viterbi) e o 2 (CorretorAltura) sao os mesmos nos dois; o que muda e' COMO
// o esticamento e' feito, nao QUANTO. Ver docs/especificacao-v3-ponteiro.md.
enum class MotorSintese { PSOLA, Ponteiro };
```

Dentro de `StreamParams`, depois de `double fmax = FMAX;`:

```cpp
    // Etapa 6: PSOLA (padrao; a linha de base mede este) ou Ponteiro (v3, baixa
    // latencia). O plugin liga o Ponteiro pelo botao Low Latency, que tambem
    // forca look = 0; o CLI expoe os dois separadamente (motor= e lowlat=).
    MotorSintese motor = MotorSintese::PSOLA;
```

- [ ] **Passo 2: `prepare()` — latência por motor**

Trocar a linha `latSamples = p.nFrame + p.look * p.nHop + psolaGuard;` por:

```cpp
        // Orçamento de latência. Com PSOLA: quadro + look-ahead + guarda, os
        // tres termos de docs/modo-baixa-latencia.md §2. Com Ponteiro: so a
        // margem fixa do motor -- os outros dois termos nao somem, mudam de
        // moeda: viram DEFASAGEM DA CORRECAO (o beta aplicado agora foi
        // decidido 'defasagem' amostras atras). Ver especificacao §4.
        defasagem = p.nFrame + p.look * p.nHop;
        if (p.motor == MotorSintese::Ponteiro) {
            ponteiro.prepare(fs, FMIN);
            latSamples = ponteiro.latencia();
        } else {
            latSamples = p.nFrame + p.look * p.nHop + psolaGuard;
        }
```

- [ ] **Passo 3: `reset()`** — acrescentar no fim de `reset()`:

```cpp
        // Etapa 6: o motor de ponteiro zera os ponteiros e contadores sem
        // realocar (o anel foi alocado em prepare()).
        ponteiro.reset();
```

- [ ] **Passo 4: `process()` — o desvio por amostra e a saída direta**

Dentro do laço `for (int i = 0; i < n; ++i)`, logo depois do `while (buffered >= p.nFrame && ...) { ... }`, acrescentar:

```cpp
            // Etapa 6: no motor de ponteiro a saida desta amostra e' produzida
            // AQUI, no mesmo indice do host -- nao passa por outBuf/lida, porque
            // o atraso (MARGEM) ja esta dentro do motor. Fica depois do disparo
            // de quadros para que a decisao lida seja funcao so do que ja
            // chegou: e' o que torna a saida invariante ao tamanho de bloco.
            if (p.motor == MotorSintese::Ponteiro) out[i] = passoPonteiro(in[i]);
```

Logo depois do laço (antes de `avancarPsola();`):

```cpp
        if (p.motor == MotorSintese::Ponteiro) { lida += n; return; }
```

E acrescentar o método privado, depois de `emitirAmostras()`:

```cpp
    // -------------------------------------------------------------------
    //  passoPonteiro() — Etapa 6: uma amostra pelo motor de ponteiro.
    //
    //  A decisao usada e' a da amostra 'defasagem' atras (nFrame + look*nHop):
    //  e' a ultima que existe com certeza quando esta amostra chega, e usar um
    //  indice FIXO (em vez de f0samp.back()) da duas coisas: a trajetoria do
    //  Retune Speed chega ao motor amostra a amostra (nao em degraus de nHop),
    //  e a defasagem vira um numero exato, citavel: "o beta aplicado agora foi
    //  decidido ha nFrame + look*nHop amostras".
    //
    //  O seco do mix e' a entrada atrasada de latSamples (= MARGEM), pelo mesmo
    //  indice absoluto -- a regra da Etapa 2. Com beta = 1 o motor devolve
    //  exatamente xAll[a - MARGEM], entao mix=0 e tol=600 continuam bit-identicos.
    // -------------------------------------------------------------------
    float passoPonteiro(float x) {
        const long long a = (long long)xAll.size() - 1;      // indice absoluto desta amostra
        const long long d = a - (long long)defasagem;         // decisao que a governa
        const bool temDecisao = (d >= 0 && d < (long long)f0samp.size());
        const double f0 = temDecisao ? (double)f0samp[(size_t)d]   : 0.0;
        const double fo = temDecisao ? (double)foutSamp[(size_t)d] : 0.0;
        float molhado = ponteiro.processar(x, f0, fo);
        if (p.corr.vibAmp > 0.0 && temDecisao) molhado *= ganhoSamp[(size_t)d];
        const long long src = a - (long long)latSamples;
        const float seco = (src >= 0) ? xAll[(size_t)src] : 0.0f;
        return misturar(seco, molhado, p.mix);
    }
```

- [ ] **Passo 5: getters de medição** — na seção pública, depois de `getFoutSamp()`:

```cpp
    // Etapa 6: medicao do motor de ponteiro (fora do caminho de audio).
    long long getSaltosPonteiro()    const { return ponteiro.saltos(); }
    double    getDistMediaPonteiro() const { return ponteiro.distMedia(); }
    double    getDistMaxPonteiro()   const { return ponteiro.distMax(); }
    int       getDefasagemCorrecao() const { return defasagem; }
```

- [ ] **Passo 6: membros** — depois de `int psolaGuard = 0;`:

```cpp
    int defasagem = 0;     // Etapa 6: nFrame + look*nHop (defasagem da correcao no ponteiro)
    MotorPonteiro ponteiro; // Etapa 6: motor v3 (so usado com p.motor == Ponteiro)
```

- [ ] **Passo 7: compilar e conferir a linha de base**

```bash
./baseline.sh conferir
```
Esperado: `IDENTICO — nada mudou.` (o motor padrão continua PSOLA; nenhum caminho existente
foi tocado).

- [ ] **Passo 8: commit**

```bash
git add src/c1_streaming/autotune_stream.h
git commit -m "feat(streaming): select the synthesis engine, wire the moving pointer"
```

---

### Tarefa 3: flags do `stream_test`, casos e invariantes no `baseline.sh`

**Arquivos:**
- Modificar: `src/c1_streaming/stream_test.cpp`
- Modificar: `baseline.sh`
- Regravar: `baseline/resumo.txt`

**Interfaces:**
- Consome: `StreamParams::motor`, `MotorSintese`, getters da Tarefa 2.
- Produz: flags `motor=psola|ponteiro`, `lowlat=1`; linha de log `motor=... | saltos=... | dist media=... | dist max=...`.

- [ ] **Passo 1: flags** — no laço de argumentos de `stream_test.cpp`, depois de `dumpbeta=`:

```cpp
        else if (a.rfind("motor=",0)==0) {
            std::string m = a.c_str()+6;
            p.motor = (m == "ponteiro" || m == "v3") ? MotorSintese::Ponteiro : MotorSintese::PSOLA;
        }
        // lowlat=1 reproduz o botao do plugin: motor de ponteiro E look = 0.
        else if (a.rfind("lowlat=",0)==0 && std::atoi(a.c_str()+7) != 0) {
            p.motor = MotorSintese::Ponteiro; p.look = 0;
        }
```
⚠️ `lowlat=` tem de vencer um `look=` que venha antes **e** depois: para isso, guardar um
`bool lowlat=false` no laço e aplicar `if (lowlat) { p.motor = Ponteiro; p.look = 0; }` **depois**
do laço, antes da sanitização. Atualizar a linha `Uso:` do cabeçalho e do `printf` com
`[motor=psola|ponteiro] [lowlat=1]`.

- [ ] **Passo 2: relatório** — trocar o `printf` de `stream_test: ...` por:

```cpp
    const bool ponteiro = (p.motor == MotorSintese::Ponteiro);
    std::printf("stream_test: %.2fs | fs=%u | block=%d | look=%d | frame=%d | FMIN=%.0f | motor=%s | lat=%d amostras\n",
                (double)N/fs, taxa, block, p.look, p.nFrame, FMIN, ponteiro ? "ponteiro" : "psola",
                eng.getLatencySamples());
    if (ponteiro)
        std::printf("ponteiro: defasagem da correcao=%d amostras | saltos=%lld | dist media=%.1f am (%.2f ms) | dist max=%.1f am (%.2f ms)\n",
                    eng.getDefasagemCorrecao(), eng.getSaltosPonteiro(),
                    eng.getDistMediaPonteiro(), 1000.0 * eng.getDistMediaPonteiro() / fs,
                    eng.getDistMaxPonteiro(),   1000.0 * eng.getDistMaxPonteiro() / fs);
```
(Os decimais são filtrados pelo hash do log no `baseline.sh`; `saltos` e `defasagem` são
inteiros determinísticos e ficam.)

- [ ] **Passo 3: casos no `baseline.sh`** — depois do bloco `gold_humanize1`, dentro do `{ ... } | tee`:

```bash
  # Etapa 6: motor v3 (ponteiro movel). lowlat=1 == motor=ponteiro look=0, que
  # e' exatamente o botao do plugin. Os pares mix0/tol600 e block64/block512 sao
  # invariantes (abaixo); mix1, natural e ponteiro_look4 fixam o comportamento.
  rodar st_lowlat_mix1     "$BIN/stream_test" "$WAV" st_lowlat_mix1.wav     1.0 crom lowlat=1
  rodar st_lowlat_mix0     "$BIN/stream_test" "$WAV" st_lowlat_mix0.wav     0.0 crom lowlat=1
  rodar st_lowlat_tol600   "$BIN/stream_test" "$WAV" st_lowlat_tol600.wav   1.0 crom tol=600 lowlat=1
  rodar st_lowlat_natural  "$BIN/stream_test" "$WAV" st_lowlat_natural.wav  1.0 crom tol=15 retune=25 lowlat=1
  rodar st_lowlat_block64  "$BIN/stream_test" "$WAV" st_lowlat_block64.wav  1.0 crom lowlat=1 block=64
  rodar st_lowlat_block512 "$BIN/stream_test" "$WAV" st_lowlat_block512.wav 1.0 crom lowlat=1 block=512
  rodar st_ponteiro_look4  "$BIN/stream_test" "$WAV" st_ponteiro_look4.wav  1.0 crom motor=ponteiro look=4
```

Invariantes, depois de `par "humanize=0 == retune25 puro" ...`:

```bash
# Etapa 6: o motor de ponteiro tem os MESMOS dois caminhos de identidade do
# PSOLA, e eles tem de concordar: se divergirem, a interpolacao deixou de ser
# exata em fracao zero ou um salto disparou com beta = 1.
par "ponteiro em identidade (beta=1) == bypass  [lowlat]" st_lowlat_tol600.wav st_lowlat_mix0.wav
par "invariancia ao bloco no ponteiro: 64 == 512"        st_lowlat_block64.wav st_lowlat_block512.wav
```

Atualizar o comentário do cabeçalho do script (`17 casos` → `37 casos`) e a mensagem de erro
de invariante (`significa que o PSOLA deixou de ser identidade em beta=1` → `significa que um
motor deixou de ser identidade em beta=1`).

- [ ] **Passo 4: conferir, depois gravar**

```bash
./baseline.sh conferir
```
Esperado: os 8 invariantes `ok`, os 19 legados `ok`, e **`DIFERENTE`** só com linhas `+` dos 7
casos novos no diff (nenhuma linha `-`, nenhuma mudança nos 30 antigos). Confirmado isso:

```bash
./baseline.sh gravar && ./baseline.sh conferir
```
Esperado: `REFERENCIA GRAVADA` e depois `IDENTICO — nada mudou.`

Conferir também, à mão, que o log do caso novo faz sentido:
```bash
grep -h "ponteiro:" /dev/null; ./baseline.sh conferir >/dev/null; # ou rode direto:
clang++ -std=c++17 -O2 -I external src/c1_streaming/stream_test.cpp -o /tmp/st && /tmp/st exemplo-antes.wav /tmp/o.wav 1.0 crom tol=15 retune=25 lowlat=1
```
Esperado: `lat=8 amostras`, `defasagem da correcao=1024`, `dist max` ≤ 8 + 44100/FMIN.

- [ ] **Passo 5: commit**

```bash
git add src/c1_streaming/stream_test.cpp baseline.sh baseline/resumo.txt
git commit -m "test: cover the moving-pointer engine in stream_test and the baseline"
```

---

### Tarefa 4: botão Low Latency no plugin

**Arquivos:**
- Modificar: `plugin/PluginProcessor.h` (ponteiro `pLowLat`), `plugin/PluginProcessor.cpp` (id, parâmetro, listener, `aplicarParametros`)
- Modificar: `plugin/PluginEditor.h` (botão + attachment), `plugin/PluginEditor.cpp` (construtor, `paint`, `resized`)

**Interfaces:**
- Consome: `MotorSintese`, `StreamParams::motor`.
- Produz: parâmetro APVTS `"lowlat"` (bool, padrão `false`), estrutural.

- [ ] **Passo 1: processor**

`PluginProcessor.cpp`, em `namespace ids`:
```cpp
    // Etapa 6: o botao Low Latency. Liga o motor de ponteiro E forca look = 0.
    // Estrutural: muda a latencia declarada ao host.
    static constexpr const char* lowlat = "lowlat";
```
Em `criarParametros()`, depois de `look`:
```cpp
    // Low Latency (Etapa 6): troca o TD-PSOLA pelo motor de ponteiro movel (v3)
    // e forca look = 0. Desligado por padrao: a instalacao nova soa como antes e
    // a linha de base mede o motor padrao. Ver docs/especificacao-v3-ponteiro.md.
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID{ ids::lowlat, 1 }, "Low Latency", false));
```
No construtor: `pLowLat = apvts.getRawParameterValue(ids::lowlat);` e
`apvts.addParameterListener(ids::lowlat, this);`; no destrutor o `remove` correspondente.
Em `aplicarParametros()`, trocar `p.look = ...` por:
```cpp
    const bool lowlat = pLowLat && pLowLat->load() > 0.5f;
    p.look  = lowlat ? 0 : (pLook ? (int) pLook->load() : 4);
    p.motor = lowlat ? MotorSintese::Ponteiro : MotorSintese::PSOLA;
```
`PluginProcessor.h`: `std::atomic<float>* pLowLat = nullptr;` ao lado de `pLook`, e um getter
público `bool lowLatencyLigado() const { return pLowLat && pLowLat->load() > 0.5f; }`.

- [ ] **Passo 2: editor — membros** (`PluginEditor.h`, ao lado de `lookSlider`):
```cpp
    juce::ToggleButton lowlatButton { "Low Latency" };
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> lowlatAttach;
```

- [ ] **Passo 3: editor — construtor** (depois de `configurarSlider(mixSlider, ...)`):
```cpp
    // Etapa 6: o botao Low Latency. Ligado, o slider de look-ahead fica visivel
    // e DESABILITADO mostrando o valor que o motor usa (0) -- opcao B da spec
    // do modo de baixa latencia: a interface mostra o que o modo mudou, em vez
    // de esconder. O valor salvo de 'look' nao e' tocado; desligar devolve tudo.
    addAndMakeVisible(lowlatButton);
    lowlatButton.onStateChange = [this] {
        const bool on = lowlatButton.getToggleState();
        lookSlider.setEnabled(!on);
        lookSlider.setAlpha(on ? 0.45f : 1.0f);
        repaint();
        // A latencia nova so' existe depois do re-prepare no proximo bloco de
        // audio; repinta de novo um pouco depois para o rodape acompanhar.
        juce::Component::SafePointer<TccAutotuneEditor> sp(this);
        juce::Timer::callAfterDelay(250, [sp] { if (sp != nullptr) sp->repaint(); });
    };
```
Depois dos outros attachments: `lowlatAttach = std::make_unique<ButtonAttachment>(apvts, "lowlat", lowlatButton);`
e, logo após, `lowlatButton.onStateChange();` para aplicar o estado restaurado do projeto.

Em `formatar(lookSlider, 0);` trocar por uma versão que mostra 0 quando o botão está ligado:
```cpp
    lookSlider.textFromValueFunction = [this](double v) {
        return juce::String(lowlatButton.getToggleState() ? 0.0 : v, 0);
    };
    lookSlider.updateText();
```
(e chamar `lookSlider.updateText()` dentro do `onStateChange`).

- [ ] **Passo 4: editor — `paint()`**: trocar o texto fixo `"pYIN  ->  TD-PSOLA"` por
```cpp
    g.drawText(processorRef.lowLatencyLigado() ? "pYIN  ->  PONTEIRO MOVEL (v3)" : "pYIN  ->  TD-PSOLA",
               faixaTitulo, juce::Justification::centredRight);
```
O rodapé `LATENCIA  %.1f ms` já lê `getLatencySamples()` e passa a mostrar `0.2 ms`. Trocar o
formato para `%.2f` para que 8 amostras não virem `0.2`.

- [ ] **Passo 5: editor — `resized()`**, no bloco MOTOR, antes de `linha(lookLabel, lookSlider);`:
```cpp
        lowlatButton.setBounds(dentro.removeFromTop(22));
        dentro.removeFromTop(6);
```
Cabe: o grupo tem 168 px de altura e passa a usar 16 + 28 + 46 + 46 + 20 = 156.

- [ ] **Passo 6: compilar e validar**

```bash
cd plugin && ./build.sh && cd ..
```
Esperado: build sem erro; `pluginval` sem falha. Abrir o Standalone, ligar Low Latency: o
rodapé mostra `LATENCIA  0.18 ms`, o look-ahead fica esmaecido em 0, o título diz PONTEIRO MOVEL.

- [ ] **Passo 7: commit**

```bash
git add plugin/PluginProcessor.h plugin/PluginProcessor.cpp plugin/PluginEditor.h plugin/PluginEditor.cpp
git commit -m "feat(plugin): add the Low Latency switch to the moving-pointer engine"
```

---

### Tarefa 5: `python/medir_v3.py` — a medição que compara os dois motores

**Arquivos:**
- Criar: `python/medir_v3.py`

**Interfaces:**
- Consome: `stream_test` (`dumpbeta=`, log `lat=`, linha `ponteiro:`), `autotune_rt` (`dumpf0=`).
- Produz: tabela em Markdown no stdout (colar no diário).

- [ ] **Passo 1: escrever o script**

```python
#!/usr/bin/env python3
# =============================================================================
#  medir_v3.py — compara o TD-PSOLA com o motor de ponteiro movel (v3).
#
#  Responde, com numero, as perguntas da spec (docs/especificacao-v3-ponteiro.md
#  §7): a nota de saida e' a mesma nos dois motores? Quanto e' o erro de ataque
#  do ponteiro (o beta chega 'defasagem' amostras atrasado)? Quanto e' a
#  latencia variavel? Algum motor introduz degraus (cliques)?
#
#  COMO MEDE O ERRO DE AFINACAO DA SAIDA
#  --------------------------------------
#  O alvo por amostra (fout) vem do proprio stream_test (dumpbeta=), decimado
#  por hop. O F0 da SAIDA e' medido com o autotune_rt (dumpf0=, look=8) sobre o
#  WAV gerado. As duas trilhas sao alinhadas pela latencia declarada de cada
#  motor. O erro e' |1200*log2(f0_saida / fout)| em cents, por quadro vozeado.
#    * "estavel": quadros a partir de 50 ms depois do inicio de cada regiao
#       vozeada -- mede se a nota certa saiu;
#    * "ataque": quadros nos primeiros 30 ms de cada regiao -- mede o preco da
#       defasagem da correcao no ponteiro.
#  O detector tem quadro de 1024 (23 ms), entao a janela de ataque e' GROSSA:
#  os numeros de ataque sao comparaveis ENTRE motores, nao absolutos.
#
#  USO:  .venv/bin/python python/medir_v3.py [--wav exemplo-antes.wav]
# =============================================================================
import argparse, os, re, subprocess, sys, tempfile
import numpy as np
import soundfile as sf

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESET = ["1.0", "crom", "tol=15", "retune=25"]
HOP = 256
CONFIGS = [  # (nome, flags extras do stream_test)
    ("PSOLA, look=4",       ["look=4"]),
    ("Ponteiro, look=4",    ["motor=ponteiro", "look=4"]),
    ("Low Latency (look=0)", ["lowlat=1"]),
]

def compilar(dir_bin):
    cxx = "clang++" if subprocess.run(["which", "clang++"], capture_output=True).returncode == 0 else "g++"
    exes = {}
    for fonte, exe in [("src/c1_streaming/stream_test.cpp", "stream_test"),
                       ("src/offline_causal/autotune_rt.cpp", "autotune_rt")]:
        dst = os.path.join(dir_bin, exe)
        r = subprocess.run([cxx, "-std=c++17", "-O2", "-I", os.path.join(RAIZ, "external"),
                            os.path.join(RAIZ, fonte), "-o", dst], capture_output=True, text=True)
        if r.returncode: sys.exit(r.stderr)
        exes[exe] = dst
    return exes

def rodar(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: sys.exit(r.stdout + r.stderr)
    return r.stdout

def cents(a, b): return 1200.0 * np.log2(a / b)

def regioes(vozeado):
    """Lista de (ini, fim) de quadros vozeados consecutivos."""
    out, ini = [], None
    for i, v in enumerate(vozeado):
        if v and ini is None: ini = i
        if not v and ini is not None: out.append((ini, i)); ini = None
    if ini is not None: out.append((ini, len(vozeado)))
    return out

def medir(nome, flags, exes, wav, tmp):
    saida = os.path.join(tmp, "saida.wav"); beta = os.path.join(tmp, "beta.txt"); f0s = os.path.join(tmp, "f0.txt")
    log = rodar([exes["stream_test"], wav, saida] + PRESET + flags + [f"dumpbeta={beta}"])
    lat = int(re.search(r"lat=(\d+)", log).group(1))
    fs = int(re.search(r"fs=(\d+)", log).group(1))
    dist = re.search(r"dist media=([\d.]+).*dist max=([\d.]+)", log)
    rodar([exes["autotune_rt"], saida, os.path.join(tmp, "lixo.wav"), "1.0", "crom", "tol=600",
           "look=8", f"hop={HOP}", f"dumpf0={f0s}"])
    alvo = np.loadtxt(beta)           # colunas: f0_entrada fout, uma linha por hop
    f0_out = np.loadtxt(f0s)          # um F0 por quadro da saida (hop=HOP)
    # alinhamento: quadro k da saida cobre a entrada deslocada de -lat amostras
    desl = int(round(lat / HOP))
    n = min(len(f0_out) - desl, len(alvo))
    fo = alvo[:n, 1]; fi = alvo[:n, 0]; fs_out = f0_out[desl:desl + n]
    voz = (fi > 0) & (fo > 0) & (fs_out > 0)
    err = np.full(n, np.nan); err[voz] = np.abs(cents(fs_out[voz], fo[voz]))
    est, atq = [], []
    q50, q30 = int(0.050 * fs / HOP), int(0.030 * fs / HOP)
    for ini, fim in regioes(fi > 0):
        est += [e for e in err[ini + q50:fim] if not np.isnan(e)]
        atq += [e for e in err[ini:min(fim, ini + q30)] if not np.isnan(e)]
    y, _ = sf.read(saida); y = y if y.ndim == 1 else y.mean(axis=1)
    degraus = int(np.sum(np.abs(np.diff(y)) > 0.25))
    return dict(nome=nome, lat_ms=1000 * lat / fs,
                dist_med=float(dist.group(1)) * 1000 / fs if dist else 0.0,
                dist_max=float(dist.group(2)) * 1000 / fs if dist else 0.0,
                est_med=np.median(est) if est else np.nan, est_p95=np.percentile(est, 95) if est else np.nan,
                atq_med=np.median(atq) if atq else np.nan, degraus=degraus)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--wav", default=os.path.join(RAIZ, "exemplo-antes.wav"))
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        exes = compilar(tmp)
        x, _ = sf.read(a.wav); x = x if x.ndim == 1 else x.mean(axis=1)
        print(f"degraus |d| > 0,25 na ENTRADA: {int(np.sum(np.abs(np.diff(x)) > 0.25))}\n")
        print("| Motor | Latência fixa | dist média | dist máx | erro estável (med / p95, ct) | erro de ataque (med, ct) | degraus |")
        print("|---|---:|---:|---:|---:|---:|---:|")
        for nome, flags in CONFIGS:
            r = medir(nome, flags, exes, a.wav, tmp)
            print(f"| {r['nome']} | {r['lat_ms']:.2f} ms | {r['dist_med']:.2f} ms | {r['dist_max']:.2f} ms | "
                  f"{r['est_med']:.1f} / {r['est_p95']:.1f} | {r['atq_med']:.1f} | {r['degraus']} |")

if __name__ == "__main__":
    main()
```

- [ ] **Passo 2: rodar**

```bash
.venv/bin/python python/medir_v3.py
```
Esperado: tabela com três linhas. Critérios de sanidade: PSOLA e Ponteiro com o mesmo `look`
têm erro estável parecido (mesma nota, é a tese da spec §2); o Ponteiro tem `lat` de 0,18 ms;
`degraus` de todos abaixo do da entrada. Se o erro estável do Ponteiro for muito maior que o do
PSOLA, o alinhamento (`desl`) está errado ou a defasagem não está sendo compensada — o erro
de afinação **não** deve conter a defasagem, porque o alvo é comparado ao que saiu.

- [ ] **Passo 3: commit**

```bash
git add python/medir_v3.py
git commit -m "feat(analysis): measure PSOLA against the moving-pointer engine"
```

---

### Tarefa 6: documentação — diário, decisão, status e READMEs

**Arquivos:**
- Modificar: `docs/execucao-do-plano.md` (tabela de status no topo + seção nova **Etapa 6** antes de "Pendências abertas")
- Modificar: `docs/historico-e-decisoes.md` (**Decisão 9** depois da Decisão 8)
- Modificar: `docs/modo-baixa-latencia.md`, `docs/analise-v1-v2-v3.md`, `docs/pesquisa-latencia-antares.md` (cabeçalho de status)
- Modificar: `docs/README.md`, `README.md`, `CLAUDE.md`, `plugin/README.md`, `python/README.md`

- [ ] **Passo 1: Etapa 6 no diário** — seção com esta estrutura, preenchida com os números reais
  da Tarefa 3 (log) e da Tarefa 5 (tabela):

```markdown
## Etapa 6 — motor v3 de ponteiro móvel (Low Latency)

**Concluída em 2026-09-__.** Especificação: [especificacao-v3-ponteiro.md](especificacao-v3-ponteiro.md).
Decisão de escopo: [Decisão 9](historico-e-decisoes.md#decisão-9--motor-v3-de-ponteiro-móvel-como-motor-paralelo-2026-09-01).

### O que foi feito
- `MotorPonteiro` em `dsp.h` (…linhas), RT-safe; `MotorSintese` e `StreamParams::motor` no streaming;
  `motor=`/`lowlat=` no `stream_test`; botão **Low Latency** no plugin; `test_ponteiro.cpp`; 7 casos e
  2 invariantes no `baseline.sh`; `python/medir_v3.py`.
- O que **não** mudou: pYIN, Viterbi, `CorretorAltura`, `psolaSintetiza`, `avancarPsola`.

### Verificação
- `./baseline.sh conferir`: 30 casos antigos `IDENTICO`; 8 invariantes `ok`; 19 legados `ok`; 37 casos no total.
- `test_ponteiro`: __ verificações, todas `ok`.
- `pluginval`: __.
- Latência declarada com Low Latency: **8 amostras = 0,18 ms** (era 2552 = 57,9 ms no contralto).

### Medição — `python/medir_v3.py`
(tabela colada)

### O que a medição diz, e o que não diz
- (interpretar: a nota sai igual? o erro de ataque é maior no ponteiro? quanto? a latência
  variável medida bate com a projeção T/2 da análise §4?)
- Ressalva: janela de ataque de 30 ms com detector de 23 ms — comparável entre motores, não absoluto.

### Pendências que esta etapa abre
- Teste de escuta (agora responde também ao erro de ataque).
- L6, recentragem em silêncio, formante em maior/menor — ver spec §8.
```

Atualizar a tabela de status do topo do diário com a linha `**6 — motor v3 (Low Latency)** | ✅ concluída | data`,
e a nota "O plano acabou" para dizer que a Etapa 6 veio de um plano próprio
(`plano-v3-ponteiro.md`).

- [ ] **Passo 2: Decisão 9 no histórico** — depois da Decisão 8:

```markdown
### Decisão 9 — motor v3 de ponteiro móvel como motor paralelo (2026-09-01)

**O que muda:** um segundo motor de síntese (ponteiro móvel, patente US 5.973.252) selecionável
por um botão **Low Latency** no plugin e por `motor=`/`lowlat=` no CLI. O TD-PSOLA fica, intocado,
como padrão e referência.

**Por quê:** a [análise de 31/08](analise-v1-v2-v3.md) mostrou que v1 e v2 têm um piso de
`fs/FMIN` enquanto a síntese for PSOLA (12,5 ms com voz grave, na fronteira de coloração). Só a
troca do motor atravessa esse piso. O objetivo fixado em 31/08 — contribuição acadêmica **e** uso
ao vivo — elimina parar na v1 e deixa a v2 na fronteira.

**Três escolhas, decididas com o autor em 2026-09-01:**
1. **Só o ponteiro, sem L6.** Argumento da análise §8: na v3 o L6 resolve um problema que a v3
   dissolve. Volta se o erro de ataque medido for inaceitável.
2. **Qualquer escala.** O teto de formante (2,93 % cromática / 5,95 % maior-menor) é documentado,
   não imposto. Mais útil para medir.
3. **Botão Low Latency = ponteiro + look = 0**, com o slider de look-ahead visível e desabilitado
   (opção B da spec do modo de baixa latência). O CLI mantém `motor=` e `look=` independentes para
   a varredura latência × robustez.

**Consequências registradas:** a linha de base antiga mede o PSOLA e continua `IDENTICO`; a
latência declarada com o botão ligado é 8 amostras (parte fixa) e a parte variável (0..T da nota
cantada) **não** é declarada ao host — o texto do TCC tem de citar as duas. Supersede a
[Decisão 5](#decisão-5--modo-de-baixa-latência), que previa um modo por parâmetros.
```

- [ ] **Passo 3: cabeçalhos de status** — no topo de `modo-baixa-latencia.md`,
  `analise-v1-v2-v3.md` e `pesquisa-latencia-antares.md`, acrescentar uma linha ao bloco de
  citação inicial:
  `> **Atualização 2026-09-__:** a v3 foi especificada em [especificacao-v3-ponteiro.md](especificacao-v3-ponteiro.md) e implementada — Etapa 6 do [diário](execucao-do-plano.md), Decisão 9 do [histórico](historico-e-decisoes.md). Os números deste documento continuam valendo como projeção; os medidos estão na Etapa 6.`

- [ ] **Passo 4: READMEs e CLAUDE.md**
  - `docs/README.md`: duas linhas novas na tabela (`especificacao-v3-ponteiro.md`,
    `plano-v3-ponteiro.md`), o mapa rápido, e a tabela "Estado atual" (`Modo de baixa latência` →
    `✅ v3 implementada (Etapa 6)`).
  - `README.md`: na seção `stream_test`/uso, as flags `motor=` e `lowlat=`; em "Como funciona",
    um parágrafo "6-bis. Motor alternativo (v3)"; em "Testes de validação", as linhas
    `Identidade no ponteiro (lowlat)` e `Invariância ao bloco no ponteiro`; em "Próximos passos",
    Latência → `✅ v3 implementada; falta a escuta`; o `Estado atual` no topo.
  - `CLAUDE.md`: pipeline no "O que é" (`… → TD-PSOLA **ou** ponteiro móvel (v3)`); linha na
    tabela "Onde ler" (`Mexer no **motor v3 / Low Latency** → especificacao-v3-ponteiro.md +
    Etapa 6`); invariantes 1 e 4 passam a valer "nos dois motores"; `17 casos` → `37 casos`;
    "Problemas em aberto" → Latência `✅ 0,18 ms fixos com Low Latency (+ 0..T variável); falta a
    escuta`; armadilha nova: "**A latência declarada com Low Latency é só a parte fixa.** A parte
    variável (distância entre ponteiros, 0..T) não entra no `setLatencySamples`."
  - `plugin/README.md`: o parâmetro `lowlat`, o botão, o que ele força, o que mostra.
  - `python/README.md`: `medir_v3.py`.

- [ ] **Passo 5: conferir links e commit**

```bash
grep -rn "especificacao-v3-ponteiro\|plano-v3-ponteiro" docs README.md CLAUDE.md plugin/README.md | wc -l
./baseline.sh conferir | tail -1
git add docs README.md CLAUDE.md plugin/README.md python/README.md
git commit -m "docs: record step 6, the moving-pointer engine, and decision 9"
```

---

## Auto-revisão do plano (feita ao escrever)

- **Cobertura da spec:** §3 → Tarefa 1; §4 → Tarefa 2; §5.1 → Tarefa 3; §5.2 → Tarefa 4; §6 →
  Tarefas 1 e 3 (+ pluginval na 4); §7 → Tarefa 5; §8 e documentação → Tarefa 6.
- **Nomes consistentes entre tarefas:** `MotorPonteiro::{prepare, reset, latencia, processar,
  saltos, distMedia, distMax, MARGEM}`; `MotorSintese::{PSOLA, Ponteiro}`; `StreamParams::motor`;
  `AutotuneStream::{passoPonteiro, getSaltosPonteiro, getDistMediaPonteiro, getDistMaxPonteiro,
  getDefasagemCorrecao, defasagem, ponteiro}`; APVTS id `"lowlat"`; flags `motor=`, `lowlat=`.
- **Ordem obrigatória:** 1 → 2 → 3 → 4 → 5 → 6. A Tarefa 3 exige `gravar` uma única vez, e só
  depois de o `diff` mostrar apenas linhas `+`.
- **Risco conhecido:** o item 5 do `test_ponteiro` usa `static double fase` dentro de um laço —
  é intencional (fase contínua na troca de nota), mas o `static` faz o teste não ser reentrante.
  Aceitável num executável de teste que roda uma vez.
