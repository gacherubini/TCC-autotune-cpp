// ---------------------------------------------------------------------------
//  test_custo_bloco.cpp — o custo por bloco do motor de sintese, e a
//  CONTINUIDADE da forma de onda que qualquer correcao desse custo tem de
//  preservar.
//
//  Este arquivo nasceu medindo so o custo, e essa versao dele deixou passar o
//  defeito que ele existia para vigiar. Vale contar por que, porque a licao e'
//  reaproveitavel: o sinal de teste era uma nota de 220 Hz exata, e com
//  tolCents = 0 o alvo de 220 Hz e' o proprio 220 Hz, entao beta = 1 e o
//  TD-PSOLA rodava em IDENTIDADE. As tres secoes cronometravam e comparavam o
//  unico caminho em que a sintese nao desloca nada. Um teste verde sobre o
//  caminho de identidade nao diz coisa alguma sobre o caminho que o usuario ouve.
//
//  Agora o sinal e' DESAFINADO de proposito (45 cents acima de A3, dentro do meio
//  semitom para que o alvo continue sendo A3), entao beta != 1 e o PSOLA
//  realmente reempilha graos.
//
//  ---------------------------------------------------------------------------
//  AS DUAS COISAS QUE ESTE ARQUIVO MEDE, E POR QUE ELAS ANDAM JUNTAS
//
//  1) CONTINUIDADE (secao 1) -- ASERCAO. A sintese incremental re-sintetiza uma
//     janela deslizante e comete so o miolo novo. Se a ANCORA dessa janela mudar
//     de uma chamada para a outra, a grade de graos se desloca e a forma de onda
//     salta de fase na fronteira do commit. Isso e' um estalo, e um estalo por
//     bloco e' pior que o defeito de custo que se estaria tentando corrigir.
//
//  2) CUSTO POR BLOCO (secao 2) -- DIAGNOSTICO, nunca criterio de falha. E' a
//     Causa 3 do spec (docs/spec-encaixe-e-estabilidade.md §2), e ela esta
//     ABERTA: a janela recua ate o inicio da regiao vozeada, entao o custo por
//     bloco cresce com a duracao da nota. Os numeros sao impressos porque sao a
//     evidencia de que o defeito existe e porque o texto do TCC os cita.
//
//     ⚠️ VERDE AQUI NAO QUER DIZER "CONSERTADO". A secao 2 nao reprova nada. Se
//     ela reprovasse, o baseline.sh abortaria a suite inteira todo dia por causa
//     de um defeito conhecido e documentado, e uma suite que falha sempre e' uma
//     suite que ninguem le.
//
//  As duas andam juntas porque a tentacao e' resolver (2) as custas de (1). Foi
//  o que aconteceu em 03/09/2026, medido neste mesmo arquivo:
//
//    | janela de re-sintese    | desc. >30x a mediana | maior |delta| |
//    |-------------------------|----------------------|--------------|
//    | sem teto (hoje)         | 0                    | 0,116        |
//    | com teto de 12 periodos | 17                   | 0,535        |
//
//    O pico do sinal era 0,298 -- ou seja, o salto era QUASE O DOBRO DO PROPRIO
//    SINAL, e as 17 descontinuidades caiam TODAS em i % nHop == 0. (Medido com
//    a nota de 3 s deste arquivo; numa nota de 4 s dao 30, na mesma proporcao.)
//
//  A CAUSA, que fecha a questao para quem vier tentar de novo: psolaSintetiza()
//  ancora a grade de sintese em cum[0] = 0, na PRIMEIRA marca da janela. A
//  invariancia a truncamento conquistada no commit e1ffd1d vale para o FIM da
//  regiao, nao para o INICIO. Um teto faz 'winStart' avancar nHop a cada commit
//  assim que a nota passa do teto, entao a ancora muda a cada commit e o
//  deslocamento da grade nao e' multiplo do espacamento de sintese. Preservar a
//  fase com um inicio movel exige carregar a contagem acumulada de beta ENTRE
//  chamadas -- a cadeia de marcas incremental, que e' hoje o unico caminho que
//  sobra para a Causa 3.
//
//  Entao o trabalho deste arquivo e': documentar a Causa 3 com numeros, e
//  REPROVAR qualquer correcao que troque custo por estalo.
//
//  ---------------------------------------------------------------------------
//  ESTE E' O UNICO TESTE DA SUITE SENSIVEL A MAQUINA, e o desenho leva isso em
//  conta:
//
//   - A parte cronometrada nao assevera nada. Nenhum 'checar' olha para
//     milissegundos, entao uma maquina lenta ou um CI carregado nao produzem
//     falha espuria.
//   - A parte que assevera (continuidade) e' puramente aritmetica sobre a saida:
//     mesma resposta em qualquer maquina.
//   - A estatistica de custo e' PERCENTIL, nao maximo. So metade dos blocos faz
//     trabalho (com nHop = 256 e bloco = 128, a sintese avanca a cada dois
//     blocos), entao a mediana cairia em cima dessa fronteira e oscilaria. O
//     percentil 90 fica solidamente entre os blocos que trabalham.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_custo_bloco.cpp -o test_custo_bloco
//  Rodar:     ./test_custo_bloco      (0 = tudo certo, 1 = falhou)
// ---------------------------------------------------------------------------
#define DR_WAV_IMPLEMENTATION
#include "../c1_streaming/autotune_stream.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>

static int falhas = 0;

static void checar(bool ok, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (!ok) ++falhas;
    std::printf("  %s  ", ok ? "ok  " : "FALHA");
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}

// Diagnostico: sai no relatorio, nao entra em nenhuma asercao.
static void diag(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    std::printf("        . ");
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}

// ---------------------------------------------------------------------------
//  A altura do sinal de teste.
//
//  45 cents acima de A3. Dois requisitos que so este valor atende ao mesmo
//  tempo: precisa estar DENTRO do meio semitom, para que a nota-alvo continue
//  sendo A3 (a 51 cents o alvo viraria A#3 e o teste mediria outra coisa), e
//  precisa estar LONGE de zero, para que beta = 220/225,79 fique claramente
//  diferente de 1 e o PSOLA reempilhe graos de verdade.
// ---------------------------------------------------------------------------
static double f0Desafinado() { return 220.0 * std::pow(2.0, 45.0 / 1200.0); }

// ---------------------------------------------------------------------------
//  Sinal de teste: silencio, uma nota LONGA sustentada, silencio.
//
//  A nota longa e' o ponto inteiro do arquivo. O custo do PSOLA incremental so
//  cresce DENTRO de uma regiao vozeada -- ele volta ao normal quando a nota
//  acaba, porque a busca da ancora para no primeiro quadro nao-vozeado. Um
//  sinal de notas curtas nao exibiria o defeito, e um teste montado sobre ele
//  passaria com o motor quebrado.
//
//  Perfil harmonico e envelope sao os mesmos de test_deteccao.cpp, pela mesma
//  razao: senoide pura nao produz a cadeia de marcas que o PSOLA percorre.
// ---------------------------------------------------------------------------
static std::vector<float> sinalNotaLonga(int fs, double segNota, double f0,
                                         double segSilencio = 0.25) {
    const int nSil  = (int)(segSilencio * fs);
    const int nNota = (int)(segNota * fs);
    std::vector<float> x((size_t)(2 * nSil + nNota), 0.0f);
    const int nHarm = std::min(24, std::max(1, (int)((0.45 * fs) / f0)));
    for (int i = 0; i < nNota; ++i) {
        const double t = (double)i / fs;
        double s = 0.0, norm = 0.0;
        for (int h = 1; h <= nHarm; ++h) {
            const double a = 1.0 / std::pow((double)h, 1.2);
            s += a * std::sin(2.0 * PI * h * f0 * t);
            norm += a;
        }
        const double ataque = 1.0 - std::exp(-t / 0.020);
        const double queda  = 1.0 - std::exp(-(segNota - t) / 0.020);
        x[(size_t)(nSil + i)] = (float)(0.7 * (s / norm) * ataque * std::max(0.0, queda));
    }
    return x;
}

// Um bloco cronometrado: quando ele entrou (em segundos de sinal) e quanto
// custou (em ms de parede).
struct Bloco { double t; double ms; };

static std::vector<Bloco> cronometrar(const std::vector<float>& x, int fs,
                                      MotorSintese motor, int block,
                                      std::vector<float>* saida = nullptr,
                                      int* latOut = nullptr) {
    StreamParams p;
    p.motor = motor;
    if (motor == MotorSintese::Ponteiro) p.look = 0;
    AutotuneStream eng; eng.prepare(fs, p);
    if (latOut) *latOut = eng.getLatencySamples();

    std::vector<float> out(x.size(), 0.0f);
    std::vector<Bloco> blocos;
    blocos.reserve(x.size() / (size_t)block + 1);
    for (size_t off = 0; off < x.size(); off += (size_t)block) {
        const int nb = (int)std::min((size_t)block, x.size() - off);
        const auto t0 = std::chrono::steady_clock::now();
        eng.process(&x[off], &out[off], nb);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        blocos.push_back({ (double)off / fs, ms });
    }
    if (saida) *saida = out;
    return blocos;
}

// Percentil (0..1) dos custos de uma janela de tempo [t0, t1) do sinal.
static double percentilNaJanela(const std::vector<Bloco>& b, double t0, double t1,
                                double q, double* maxOut = nullptr) {
    std::vector<double> v;
    double mx = 0.0;
    for (const auto& e : b)
        if (e.t >= t0 && e.t < t1) { v.push_back(e.ms); mx = std::max(mx, e.ms); }
    if (maxOut) *maxOut = mx;
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t i = (size_t)(q * (double)(v.size() - 1));
    return v[i];
}

static constexpr double SEG_NOTA = 3.0;    // duracao da nota sustentada
static constexpr double SEG_SIL  = 0.25;   // silencio antes e depois

// ---------------------------------------------------------------------------
//  SECAO 1 — CONTINUIDADE. A asercao que faltava.
//
//  Como se mede um estalo sem inventar um limiar arbitrario. A ideia e' comparar
//  o MAIOR degrau amostra-a-amostra com o degrau tipico do proprio sinal, e nao
//  com um numero absoluto:
//
//    - num sinal periodico e suave, |delta| tem distribuicao apertada. O maior
//      degrau legitimo e' o da inclinacao maxima da forma de onda, e fica a uma
//      distancia pequena do percentil 99,9;
//    - um salto de fase e' um OUTLIER: ele nao pertence aquela distribuicao.
//
//  Por isso o criterio e' `max|delta| <= K * p99,9(|delta|)`. Medido:
//
//    sem teto : max 0,1157 / p99,9 0,1086  -> razao 1,07x
//    com teto : max 0,5353 / p99,9 0,1166  -> razao 4,59x
//
//  K = 2,5 cai no meio da lacuna, com folga dos dois lados. Duas alternativas
//  foram descartadas: um limiar ABSOLUTO em |delta| mediria a amplitude do sinal
//  de teste em vez do defeito; e um multiplo da MEDIANA (o criterio que o resto
//  do projeto usa) tem margem pequena demais aqui -- o maior degrau LEGITIMO ja
//  e' 30x a mediana, exatamente em cima do limiar, entao um sinal ligeiramente
//  diferente daria falso positivo.
//
//  A contagem de ">30x a mediana" continua sendo impressa como diagnostico,
//  porque e' o criterio que o resto do projeto usa e que o texto cita.
// ---------------------------------------------------------------------------
static constexpr double K_CONTINUIDADE = 2.5;

static void secaoContinuidade(int fs) {
    std::printf("== 1. continuidade da forma de onda (beta != 1) ==\n");
    const int block = 128;
    const double f0 = f0Desafinado();
    const auto x = sinalNotaLonga(fs, SEG_NOTA, f0, SEG_SIL);

    std::vector<float> out; int lat = 0;
    cronometrar(x, fs, MotorSintese::PSOLA, block, &out, &lat);

    // Mede so o REGIME: pula o priming (latencia) e as bordas do envelope, que
    // tem inclinacao propria e nada tem a ver com a emenda das janelas.
    const size_t ini = (size_t)(SEG_SIL * fs) + (size_t)lat + (size_t)(0.30 * fs);
    const size_t fim = (size_t)((SEG_SIL + SEG_NOTA) * fs) - (size_t)(0.30 * fs);
    if (ini + 1000 >= fim || fim > out.size()) { checar(false, "janela de medicao invalida"); return; }

    std::vector<double> d;
    d.reserve(fim - ini);
    double pico = 0.0;
    for (size_t i = ini; i < fim; ++i) {
        d.push_back(std::fabs((double)out[i] - (double)out[i - 1]));
        pico = std::max(pico, std::fabs((double)out[i]));
    }
    std::vector<double> ord = d;
    std::sort(ord.begin(), ord.end());
    const double mediana = ord[ord.size() / 2];
    const double p999    = ord[(size_t)(0.999 * (double)(ord.size() - 1))];
    const double maxD    = ord.back();
    const double razao   = (p999 > 0.0) ? maxD / p999 : 0.0;

    checar(razao <= K_CONTINUIDADE,
           "maior degrau %.4f vs p99,9 %.4f -> razao %.2fx (teto %.1fx)",
           maxD, p999, razao, K_CONTINUIDADE);

    // ---- diagnostico ----
    long long n30 = 0;
    std::vector<int> posHop;
    for (size_t k = 0; k < d.size(); ++k)
        if (d[k] > 30.0 * mediana) { ++n30; posHop.push_back((int)((ini + k) % (size_t)N_HOP)); }
    diag("pico da saida %.4f | mediana |delta| %.5f | descontinuidades >30x a mediana: %lld",
         pico, mediana, n30);
    if (!posHop.empty()) {
        // A assinatura do defeito de ancora: tudo na fronteira do commit. Se
        // aparecer, e' esta linha que diz QUAL defeito e', nao so que ha um.
        long long naFronteira = 0;
        for (int m : posHop) if (m == 0 || m == N_HOP - 1) ++naFronteira;
        diag("delas em i %% nHop em {0, %d}: %lld de %zu  <-- assinatura de ancora movel",
             N_HOP - 1, naFronteira, posHop.size());
    }
}

// ---------------------------------------------------------------------------
//  SECAO 2 — o custo por bloco. DIAGNOSTICO, e defeito ABERTO.
//
//  Nada aqui reprova. Ver o cabecalho do arquivo: a Causa 3 esta aberta, e um
//  teste que falhasse por causa dela abortaria o baseline.sh todo dia.
//
//  O que estes numeros sustentam: que a Causa 3 EXISTE e tem tamanho. A janela
//  de re-sintese recua ate o inicio da regiao vozeada, entao numa nota de 3 s o
//  motor refaz 3 s de marcas e graos para entregar 128 amostras.
// ---------------------------------------------------------------------------
static void secaoCusto(int fs) {
    std::printf("\n== 2. custo por bloco do TD-PSOLA -- DIAGNOSTICO (Causa 3, defeito ABERTO) ==\n");
    const int block = 128;
    const double orcamentoMs = 1000.0 * block / fs;   // 2,90 ms a 44,1 kHz

    const auto x = sinalNotaLonga(fs, SEG_NOTA, f0Desafinado(), SEG_SIL);
    const auto b = cronometrar(x, fs, MotorSintese::PSOLA, block);

    // Duas janelas dentro da MESMA nota: uma logo depois de o pipeline encher,
    // outra no fim. A razao entre elas E' o crescimento.
    const double iniA = 0.80, fimA = 1.30;
    const double iniB = SEG_SIL + SEG_NOTA - 0.50, fimB = SEG_SIL + SEG_NOTA;
    double maxA = 0.0, maxB = 0.0;
    const double p90A = percentilNaJanela(b, iniA, fimA, 0.90, &maxA);
    const double p90B = percentilNaJanela(b, iniB, fimB, 0.90, &maxB);

    double maxGeral = 0.0; long long estouros = 0, total = 0;
    for (const auto& e : b) {
        if (e.t < SEG_SIL) continue;            // priming nao conta
        ++total; maxGeral = std::max(maxGeral, e.ms);
        if (e.ms > orcamentoMs) ++estouros;
    }

    std::printf("        > CRESCIMENTO: p90 %.3f ms no inicio da nota -> %.3f ms no fim"
                "  (%.2fx)\n", p90A, p90B, p90A > 0.0 ? p90B / p90A : 0.0);
    std::printf("        > PIOR BLOCO:  %.3f ms contra %.2f ms de orcamento  (%.1fx)\n",
                maxGeral, orcamentoMs, maxGeral / orcamentoMs);
    std::printf("        > ESTOUROS:    %lld de %lld blocos (%.1f%%) -- cada um e' um dropout\n",
                estouros, total, total ? 100.0 * (double)estouros / (double)total : 0.0);
    diag("maximos por janela: %.3f / %.3f ms", maxA, maxB);
    diag("perfil (pior bloco por meio segundo):");
    for (double t = 0.0; t < SEG_SIL + SEG_NOTA + SEG_SIL; t += 0.5) {
        double mx = 0.0;
        percentilNaJanela(b, t, t + 0.5, 0.5, &mx);
        std::printf("            t=%4.2f s  %6.3f ms\n", t, mx);
    }
    diag("NAO ha asercao nesta secao. Verde aqui nao quer dizer consertado.");
}

// ---------------------------------------------------------------------------
//  SECAO 3 — a invariancia ao tamanho de bloco, agora COM beta != 1.
//
//  A saida tem de ser identica para qualquer tamanho de bloco do host. O
//  baseline.sh ja compara 64 == 512 == 1024 sobre o take real; aqui a varredura
//  vai de 1 a 4096 sobre uma nota LONGA e DESAFINADA -- que e' o caso em que a
//  janela de re-sintese trabalha mais e em que qualquer dependencia do corte do
//  host apareceria.
//
//  Rodar isto em beta = 1 (como a versao anterior fazia) testava o caminho em
//  que psolaSintetiza devolve a entrada, onde a invariancia e' trivial.
// ---------------------------------------------------------------------------
static void secaoInvariancia(int fs) {
    std::printf("\n== 3. invariancia ao tamanho de bloco (beta != 1) ==\n");
    // 1,6 s de nota: longa o bastante para a janela crescer, curta o bastante
    // para o teste continuar barato mesmo com block = 1.
    const auto x = sinalNotaLonga(fs, 1.60, f0Desafinado(), 0.20);

    std::vector<float> ref;
    cronometrar(x, fs, MotorSintese::PSOLA, 128, &ref);

    for (int block : { 1, 3, 64, 256, 512, 1024, 4096 }) {
        std::vector<float> y;
        cronometrar(x, fs, MotorSintese::PSOLA, block, &y);
        long long difs = 0; size_t primeira = 0;
        for (size_t i = 0; i < ref.size(); ++i)
            if (y[i] != ref[i]) { if (!difs) primeira = i; ++difs; }
        checar(difs == 0, "PSOLA  block=%-5d == block=128  (%lld divergencias%s)",
               block, difs, difs ? "" : ", bit a bit");
        if (difs) diag("primeira divergencia em i=%zu", primeira);
    }

    // O motor de ponteiro nao passa por avancarPsola, mas a varredura custa
    // pouco e o ticket pede explicitamente que ele nao regrida.
    std::vector<float> refP;
    cronometrar(x, fs, MotorSintese::Ponteiro, 128, &refP);
    for (int block : { 1, 64, 512, 4096 }) {
        std::vector<float> y;
        cronometrar(x, fs, MotorSintese::Ponteiro, block, &y);
        long long difs = 0;
        for (size_t i = 0; i < refP.size(); ++i) if (y[i] != refP[i]) ++difs;
        checar(difs == 0, "ponteiro block=%-5d == block=128  (%lld divergencias%s)",
               block, difs, difs ? "" : ", bit a bit");
    }
}

// ---------------------------------------------------------------------------
//  SECAO 4 — o motor de ponteiro nao regride.
//
//  Ele e' O(1) por amostra (anel, interpolacao de 4 pontos, sem janelas nem
//  marcas) e ja media 0 % de estouros. Esta secao existe para que uma mudanca
//  futura em avancarPsola que por engano afete o caminho comum apareca aqui, e
//  nao no ouvido de quem toca. Aqui a asercao sobre tempo E' feita, e pode ser:
//  o custo do ponteiro nao depende da duracao da nota por CONSTRUCAO, entao a
//  razao e' ~1,0 em qualquer maquina.
// ---------------------------------------------------------------------------
static constexpr double FATOR_MAX_PONTEIRO = 1.6;

static void secaoPonteiro(int fs) {
    std::printf("\n== 4. o motor de ponteiro continua plano ==\n");
    const int block = 128;
    const auto x = sinalNotaLonga(fs, SEG_NOTA, f0Desafinado(), SEG_SIL);
    const auto b = cronometrar(x, fs, MotorSintese::Ponteiro, block);

    double maxA = 0.0, maxB = 0.0;
    const double p90A = percentilNaJanela(b, 0.80, 1.30, 0.90, &maxA);
    const double p90B = percentilNaJanela(b, SEG_SIL + SEG_NOTA - 0.50,
                                             SEG_SIL + SEG_NOTA, 0.90, &maxB);
    checar(p90A > 0.0 && p90B <= p90A * FATOR_MAX_PONTEIRO,
           "p90 do bloco: inicio %.3f ms -> fim %.3f ms (razao %.2fx)",
           p90A, p90B, p90A > 0.0 ? p90B / p90A : 0.0);

    double maxGeral = 0.0;
    for (const auto& e : b) if (e.t >= SEG_SIL) maxGeral = std::max(maxGeral, e.ms);
    diag("pior bloco: %.3f ms (%.1fx o orcamento de %.2f ms)",
         maxGeral, maxGeral / (1000.0 * block / fs), 1000.0 * block / fs);
}

int main() {
    definirEscala("crom");
    const int fs = 44100;
    secaoContinuidade(fs);
    secaoCusto(fs);
    secaoInvariancia(fs);
    secaoPonteiro(fs);
    std::printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK",
                falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
