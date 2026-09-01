# Motor v3 — ponteiro móvel no lugar do TD-PSOLA — especificação

> **Data:** 2026-09-01
> **Status:** 📐 **ESPECIFICAÇÃO APROVADA — implementação em curso.** O diário da implementação
> está em [execucao-do-plano.md](execucao-do-plano.md), Etapa 6. A decisão de escopo está
> registrada como [Decisão 9](historico-e-decisoes.md#decisão-9--motor-v3-de-ponteiro-móvel-como-motor-paralelo-2026-09-01).
> **Leia antes:** [analise-v1-v2-v3.md](analise-v1-v2-v3.md) (o enquadramento por estágios e
> os números corrigidos) · [pesquisa-latencia-antares.md](pesquisa-latencia-antares.md) §3.2
> (a mecânica da patente) · [modo-baixa-latencia.md](modo-baixa-latencia.md) (o que este
> documento substitui em parte).
>
> ### O que este documento decide
>
> A [especificação de 26/08](modo-baixa-latencia.md) descrevia um modo de baixa latência por
> **parâmetros** (v1) e por **troca do detector** (v2). A [análise de 31/08](analise-v1-v2-v3.md)
> mostrou que os dois têm um piso: enquanto a síntese for PSOLA, a latência é `fs/FMIN` por
> construção. Este documento especifica o caminho que atravessa esse piso — a **v3**, que troca
> o **motor de síntese** — e fixa as três escolhas que estavam em aberto:
>
> 1. **Só o ponteiro.** O detector (pYIN + Viterbi) **não muda**. O L6 (CMNDF recursivo) fica
>    fora, pelo argumento da [análise §8](analise-v1-v2-v3.md): na v3 o atraso da detecção sai
>    do caminho do áudio, e o L6 passaria a resolver um problema que a v3 dissolve por outro
>    caminho. Se o erro de ataque medido for inaceitável, o L6 volta como trabalho futuro.
> 2. **Qualquer escala.** O motor fica disponível em cromática, maior e menor. O teto de
>    deslocamento de formante por escala (2,93 % / 5,95 %) é documentado, não imposto.
> 3. **Um botão.** No plugin, um controle booleano **Low Latency** que liga o motor de ponteiro
>    **e** força `look = 0`. O slider de look-ahead fica visível e desabilitado, mostrando o
>    zero — a opção B da [spec §7](modo-baixa-latencia.md), transparente.

---

## 1. Em uma frase

Um **segundo motor de síntese, selecionável**, que corrige a altura lendo o áudio de um buffer
circular a uma velocidade β ≠ 1 e saltando exatamente um período quando a leitura ameaça
alcançar (ou perder de vista) a escrita. O PSOLA continua existindo, intocado, como motor
padrão e como referência de medição.

## 2. O que NÃO muda

Este é o ponto que decide o tamanho do trabalho, e vale repetir o que a
[análise §1](analise-v1-v2-v3.md) já estabeleceu:

| Estágio | Componente | Situação |
|---|---|---|
| 1 · Detectar | pYIN (CMNDF + Beta(2,18)) + Viterbi de lag fixo | **inalterado** — continua por quadro, com `look` |
| 2 · Decidir | `notaAlvo()`, `CorretorAltura` (tolerância, Retune Speed, Natural Vibrato, Humanize, Create Vibrato) | **inalterado** |
| 2 → 3 | `β = foutSamp / f0samp` | **o mesmo número**, lido do mesmo lugar |
| 3 · Executar | TD-PSOLA (`psolaSintetiza` + `avancarPsola`) | **inalterado**; deixa de ser chamado quando o motor selecionado é o ponteiro |
| — | Mix seco/molhado | inalterado em forma; o seco passa a ser atrasado da latência **do motor selecionado** |
| — | Linha de base (`baseline.sh`) | os 30 casos existentes continuam medindo o PSOLA e têm de dar `IDENTICO` |

Consequência: a **nota** de saída é a mesma nos dois motores. O que difere é *como* o
esticamento é feito, e três efeitos colaterais — formantes, latência e o tipo de artefato.

## 3. O motor — `MotorPonteiro` em `src/core/dsp.h`

Entra em `dsp.h`, ao lado de `psolaSintetiza()`, pela regra do projeto ("DSP novo entra em
`dsp.h` ou em `autotune_stream.h`"). É uma classe autocontida, testável sem o streaming.

### 3.1 Interface

```cpp
class MotorPonteiro {
public:
    void  prepare(int fsHz, double fminHz);   // aloca o anel (uma vez, fora do tempo real)
    void  reset();                            // zera ponteiros e contadores; não realoca
    int   latencia() const;                   // parte FIXA da latência, em amostras (= margem)
    float processar(float entrada, double f0Hz, double fAlvoHz);   // UMA amostra
    // medição (leitura fora do caminho de áudio):
    long long saltos()   const;               // quantos ciclos foram repetidos ou descartados
    double    distMedia() const;              // média de (W − R) nas amostras vozeadas
    double    distMax()   const;              // máximo de (W − R)
};
```

Não há `std::vector::push_back` em `processar()`. O anel é alocado em `prepare()` e nunca
cresce. É a única parte do caminho de áudio que a v3 acrescenta, e ela é RT-safe.

### 3.2 Estado

| Membro | O que é |
|---|---|
| `anel` | `std::vector<float>` de tamanho `N`, potência de 2, `N ≥ margem + 2·fs/fmin + 8`. Potência de 2 para que o *wrap* seja um `&`. |
| `W` | ponteiro de **escrita**, inteiro absoluto (amostras recebidas desde o `reset`). Avança 1 por amostra. |
| `R` | ponteiro de **leitura**, `double` absoluto. Avança `β` por amostra. |
| `margem` | distância mínima entre `R` e `W`. **8 amostras.** É a latência fixa declarada ao host. |
| `xfResta`, `xfLen`, `Rvelho` | estado do *crossfade* após um salto (§3.4). |
| `nSaltos`, `somaDist`, `nDist`, `maxDist` | contadores de medição. |

### 3.3 O passo por amostra

Para cada amostra de entrada `x`, com `f0` (Hz, 0 = não-vozeado) e `fAlvo` (Hz) **da última
decisão disponível**:

```
1. anel[W & (N−1)] = x;  W += 1
2. β = (f0 > 0) ? fAlvo / f0 : 1.0
3. y = interpola(R)                         // cúbica de 4 pontos em torno de floor(R)
   (se em crossfade: y = (1−w)·interpola(Rvelho) + w·y; Rvelho += β; w avança)
4. R += β;  Rvelho += β
5. dist = W − R
   se f0 > 0:
       T = fs / f0
       se dist < margem:        R −= T   (repete um ciclo — a leitura estava alcançando a escrita)
       se dist > margem + T:    R += T   (descarta um ciclo — a leitura ficou para trás)
       em qualquer salto: inicia crossfade de comprimento min(T/2, 64) a partir de Rvelho = R antes do salto
6. devolve y
```

Propriedades que seguem daí, e que o teste de unidade verifica:

- **β = 1 sempre ⇒ identidade atrasada de `margem`.** `R` avança 1 por amostra, nunca sai da
  grade inteira, a interpolação em fração zero devolve a amostra exata, e nenhum salto é
  disparado. Saída == entrada deslocada de 8 amostras, **bit a bit**. É o que faz `tol = 600`
  e `mix = 0` continuarem sendo os dois caminhos de identidade do projeto.
- **`dist` fica em `[margem − 1, margem + T + 1]`.** A parte variável da latência é essa
  distância: média ≈ `T/2`, máximo ≈ `T`. Depende da nota **cantada**, não do FMIN.
- **Sem voz nada acontece.** `f0 = 0` ⇒ `β = 1` e nenhum salto. A distância que estava, fica.
  Não há recentragem em silêncio — pendência registrada na §8.
- **O salto é de um período inteiro.** As duas pontas da emenda estão no mesmo ponto do ciclo.
  Em sinal periódico a emenda é invisível; em sinal não periódico (consoante, ataque) o
  crossfade de meio período reduz o degrau a uma modulação de amplitude curta.

### 3.4 Interpolação e crossfade

- **Interpolação cúbica de 4 pontos (Catmull-Rom)** sobre `anel[i−1..i+2]`, `i = floor(R)`.
  Exige `dist ≥ 3`, garantido por `margem = 8` (a distância cai no máximo `β − 1 < 1` por
  amostra antes do salto). Em fração zero devolve `anel[i]` exatamente — condição da
  identidade.
- **Crossfade** linear, `min(T/2, 64)` amostras, entre a leitura antiga (`Rvelho`, que continua
  avançando a `β`) e a nova. Um salto durante um crossfade em curso o encerra e começa outro.
  Custo: uma segunda interpolação por amostra só durante o crossfade.

> A patente do Auto-Tune lê em `Output_addr − 5` com interpolação simples, e o produto declara
> 37 amostras. Aqui a margem é de 8 porque o interpolador é curto. **O número declarado ao host
> pela v3 deste projeto será 8 amostras (0,18 ms a 44,1 kHz)**, e não 37 — é a parte fixa, e o
> texto do TCC tem de dizer sempre que a parte variável (0 a T) não entra nesse número.

## 4. Fiação no streaming — `autotune_stream.h`

```cpp
enum class MotorSintese { PSOLA, Ponteiro };
struct StreamParams { ...; MotorSintese motor = MotorSintese::PSOLA; };
```

| Ponto | Com PSOLA (hoje) | Com Ponteiro |
|---|---|---|
| `prepare()` | `latSamples = nFrame + look·nHop + psolaGuard` | `latSamples = ponteiro.latencia()` (= 8) |
| `process()`, por amostra | acumula em `xAll`, dispara quadros | idem; **e** chama `ponteiro.processar(in[i], f0, fout)` com a última decisão emitida (`f0samp.back()`, `foutSamp.back()`), guardando o resultado em `outBuf[a]` |
| `process()`, fim do bloco | `avancarPsola()` | **não chama** `avancarPsola()` |
| mix | `seco = xAll[lida − latSamples]` | igual — `latSamples` já é o do ponteiro |
| ganho do Create Vibrato | `ganhoSamp[src]` | `ganhoSamp.back()` no momento da síntese (mesma defasagem do β) |

A chamada ao ponteiro acontece **dentro do laço por amostra, depois do disparo de quadros**.
Assim a decisão que ele lê em cada amostra é função só do que já chegou, e a saída é idêntica
para qualquer tamanho de bloco do host — pela mesma razão estrutural que já vale para o disparo
de quadros.

**A defasagem da correção.** Na amostra `a`, a última decisão disponível corresponde a uma
amostra `nFrame + look·nHop` atrás (mais o resto do hop). É o "β decidido com o pitch de antes"
da [análise §2](analise-v1-v2-v3.md). Não é latência de áudio: é erro de ataque, e a §7 diz como
medi-lo.

**O que continua alocando.** `xAll`, `f0samp`, `foutSamp` e `ganhoSamp` continuam crescendo por
amostra nos dois motores (Achado 2 do doc técnico §8.3, anterior a este trabalho). A v3 não
piora nem resolve isso; o que ela garante é que **o motor de síntese** não aloca.

## 5. Controles

### 5.1 CLI (`stream_test`)

| Flag | Efeito |
|---|---|
| `motor=psola` (padrão) · `motor=ponteiro` | escolhe o motor; `look`, `frame`, `hop` continuam livres — é isso que permite a varredura latência × robustez com os dois motores |
| `lowlat=1` | **atalho que reproduz o botão do plugin**: `motor=ponteiro` **e** `look=0` |

O relatório de fim de execução passa a imprimir, no motor de ponteiro: `saltos`, `dist média`
e `dist máx` (em amostras e ms). São os números da parte variável da latência.

### 5.2 Plugin

- Parâmetro novo `lowlat` (`AudioParameterBool`, id estável `"lowlat"`, padrão **desligado**).
  **Estrutural**: muda a latência declarada, então dispara re-prepare, pelo caminho de
  `precisaReprepare` já existente.
- `aplicarParametros()`: `p.motor = lowlat ? Ponteiro : PSOLA; p.look = lowlat ? 0 : look`.
- `setLatencySamples(core.getLatencySamples())` no mesmo re-prepare — o Ableton passa a mostrar
  8 em vez de 2552.
- Editor: um `ToggleButton` **"Low Latency"** no grupo *Motor*. Ligado, o slider de look-ahead
  é desabilitado e exibe 0; desligado, volta ao valor salvo do parâmetro `look`. O valor de
  `look` **não é sobrescrito** no APVTS — só é ignorado enquanto o botão estiver ligado. Assim
  desligar devolve exatamente o estado anterior.

Padrão desligado por dois motivos: a instalação nova tem de soar como antes, e a linha de base
mede o motor padrão.

## 6. Verificação

| O quê | Como | Critério |
|---|---|---|
| Motor isolado | `src/tests/test_ponteiro.cpp` (entra no `baseline.sh` como os outros testes) | ver lista abaixo |
| Nada mudou no PSOLA | `./baseline.sh conferir` | os 30 casos existentes `IDENTICO`; os 6 invariantes e os 19 legados `ok` |
| Identidade no motor novo | casos novos `st_lowlat_mix0` e `st_lowlat_tol600` | **bit-idênticos entre si** (invariante novo) |
| Invariância ao bloco no motor novo | `st_lowlat_block64` vs `st_lowlat_block512` | bit-idênticos (invariante novo) |
| Comportamento fixado | `st_lowlat_mix1`, `st_lowlat_natural` (`tol=15 retune=25`), `st_ponteiro_look4` | checksums gravados com `./baseline.sh gravar` **uma vez**, depois `conferir` |
| Plugin compila e valida | `cd plugin && ./build.sh` | `pluginval` sem falha |

`test_ponteiro.cpp`, sobre senoides sintéticas a 44,1 kHz:

1. **β = 1** (f0 = fAlvo = 220 Hz): saída == entrada atrasada de `latencia()`, bit a bit.
2. **f0 = 0** o tempo todo: idem.
3. **β = 1,05** (220 → 231 Hz): frequência medida na saída por cruzamentos de zero dentro de
   0,5 % de 231 Hz; `saltos() > 0`; maior degrau `|y[n] − y[n−1]|` ≤ 1,5 × o maior degrau da
   entrada (sem clique).
4. **β = 0,95**: idem, 209 Hz.
5. **`dist` limitada**: `distMax() ≤ margem + T + 2` em todos os casos acima.
6. **Salto de nota** (220 Hz → 330 Hz no meio, β = 1,03): sem degrau acima do critério do
   item 3, `distMax()` respeita o `T` maior.

## 7. Medição — `python/medir_v3.py`

O que a implementação tem de responder, e que só a medição responde:

| Pergunta | Medida | Como |
|---|---|---|
| Latência de áudio | declarada, por motor | do log do `stream_test` |
| Latência variável | `dist` média e máx, por motor de ponteiro | do log |
| A nota de saída é a mesma? | erro em cents entre o F0 da **saída** e o alvo `fout`, por motor, mediana nas regiões vozeadas estáveis | F0 da saída pelo `autotune_rt … dumpf0=` (look = 8); alvo pelo `dumpbeta=` do próprio `stream_test`; alinhamento pela latência declarada |
| Erro de ataque | o mesmo erro, só nos primeiros 25 ms de cada região vozeada | idem, janela no início de cada região |
| Artefato | contagem de degraus `\|Δ\| > 0,25` (o critério absoluto do Achado B), por motor | direto na onda |

Roda sobre `exemplo-antes.wav` com o preset natural (`tol=15 retune=25`), para PSOLA, Ponteiro
com `look=4` e Low Latency (`look=0`). O resultado vai para a Etapa 6 do diário e é a tabela
que compara as duas arquiteturas no texto do TCC.

## 8. Fora do escopo, registrado

| Item | Por que fica de fora | Onde volta |
|---|---|---|
| L6 — CMNDF recursivo | análise §8: na v3 pode ser trabalho perdido; decidir depois de medir o erro de ataque | backlog; depende da §7 |
| Correção de formante na v3 | teto de 2,93 % em cromática está abaixo do limiar típico; em maior/menor (5,95 %) é ressalva documentada, não bloqueio | backlog, se a escuta reclamar |
| Recentragem do ponteiro em silêncio | em `f0 = 0` a distância fica onde estava; um retorno lento à margem reduziria a latência média entre frases | backlog |
| Buffers de acumulação RT-safe (`xAll` etc.) | anterior a este trabalho (Achado 2) | backlog §8.3 |
| Teste de escuta | é o mesmo teste que as Etapas 3–5 esperam; agora responde também "o erro de ataque é aceitável?" | pendência 1 do diário |

## 9. Grau de evidência

- §2, §3.3 (β = 1 ⇒ identidade), §4: **fortes** — seguem da leitura do código existente e da
  construção do algoritmo; o teste de unidade e os invariantes do `baseline.sh` os verificam.
- §3.3 (limites de `dist`), §3.4 (crossfade reduz o degrau): **fortes em sinal periódico**,
  **inferência** em voz real. A §7 mede.
- Latência declarada de 8 amostras: **forte** por construção; comparação com as 37 da Antares
  é **inferência** (a decomposição das 37 não é publicada).
