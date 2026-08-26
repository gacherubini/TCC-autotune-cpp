# Histórico de desenvolvimento e decisões de arquitetura

Registro cronológico do que foi construído, dos bugs que foram caçados e das decisões de
arquitetura tomadas durante o desenvolvimento do protótipo. Preservado como material de
apoio para o texto do TCC.

> **Índice da documentação:** [`docs/README.md`](README.md)

---

## Histórico / o que já foi feito

- **Pipeline completo funcionando** ponta a ponta (ler → pYIN → nota-alvo → PSOLA → WAV).
- Reproduziu o resultado-chave do TCC: pYIN ~16× mais estável que YIN (σ de estabilidade
  169 → 10 cents).
- Suporte a **escalas** (cromática / maior / menor) e **força** de correção.
- Saída em **16-bit PCM** (compatível com qualquer player).

### Bug "pipoca" (cliques) — RESOLVIDO (2026-06-09)

Eram **duas causas-raiz na síntese** (a entrada, o formato e o pYIN estavam corretos):

1. **Offset de fase por região.** O cursor de síntese andava amostra-a-amostra pelos
   silêncios e reentrava em cada nota numa fase arbitrária → cada nota saía deslocada no
   tempo vs o sinal seco; a mistura nas bordas virava comb filtering / cancelamento.
   *Sintoma decisivo: em `forca=0` a saída deveria ser idêntica à entrada e não era.*
   **Correção:** reancorar o cursor numa marca de análise real ao sair de cada silêncio
   (offset de fase = 0).

2. **Corte seco no piso de normalização** (`wsum > 0.2`): saltava no instante da troca.
   **Correção:** crossfade por **cobertura** — `out = w·(y/wsum) + (1−w)·x`, com
   `w = min(wsum, 1)`, usando a própria rampa Hann dos grãos (COLA 50% overlap).

Resultado verificado:

| Métrica | Antes | Depois | Referência (input) |
|---|---|---|---|
| `forca=0` é identidade? | ❌ (lag std 41.85) | ✅ (std 0.00) | — |
| `forca=1` saltos > 30× mediana | 28 | **1** | 0 |
| pior salto de 1 amostra | 0.379 | **0.139** | 0.118 |

### Naturalidade: tolerância, glide e formantes (2026-06-09)

- **Força em cents (zona morta / `tol`)**: desvios menores que `tol` cents não são
  corrigidos, com *soft knee* (sem degrau). Preserva vibrato e micro-afinação.
  Verificado: com `tol=15`, um desvio de −14 ct fica praticamente intacto.
- **Glide (`glide`)**: a afinação-alvo é suavizada no tempo (1 polo, reset no ataque) →
  portamento entre notas, sem o "pulo" robótico. `glide=0` = comportamento antigo.
- **Formantes**: TD-PSOLA já preserva o envelope espectral por construção (copia grãos no
  tempo, só muda o espaçamento). Confirmado com `formantes.py`: nos trechos de pitch
  médio/grave os formantes batem dentro do ruído do estimador (ex.: 1997→1997 Hz,
  3235→3273 Hz). Em pitch alto (>450 Hz) o envelope cepstral resolve mal o F1 (poucos
  harmônicos) — limitação de *medição*, não deslocamento real. Nenhum código novo foi
  preciso; a propriedade vem da arquitetura. (Importa de verdade em shifts grandes, p.ex.
  um futuro harmonizador.)
- Regressão mantida: tudo isso é retrocompatível (`tol=0 glide=0` = saída anterior); os
  testes de fase e de cliques continuam passando.

**Preset natural recomendado:** `forca 1.0  tol=15  glide=40`.

### "Pipoco residual" — RESOLVIDO de vez (2026-06-09)

Ainda sobrava um pouco de pipoco. Investigação fundo achou DOIS bugs encadeados:

1. **Compressão temporal por região (síntese).** O loop antigo andava `s += Tout`
   e pegava a marca mais próxima → quando `beta≠1`, a marca-fonte corria à frente
   da posição de saída e cada nota era *comprimida no tempo* por `beta`; o drift
   ressincronizava com tranco na borda (pop), e o `glide` piorava. *Sintoma: num
   ponto afinado (β≈1) a saída não batia com a entrada.* **Correção:** TD-PSOLA com
   **preservação de duração** — por região, as marcas de síntese são uma reamostragem
   das de análise no MESMO intervalo de tempo, com densidade local = `beta` (duplica
   grãos ao subir, pula ao descer), distribuída uniformemente.
2. **Marcas grudadas no fim da nota (detecção).** Quando o encadeamento parava no fim
   da nota, o laço externo avançava 1 amostra por vez e cravava uma marca em CADA
   amostra do final → grãos minúsculos isolados → spikes. **Correção:** ao terminar
   uma nota, pular o resto do trecho vozeado.

Verificado: spikes (>30× mediana) em `forca=1` natural caíram de 14 → **0**; o drift
sumiu (pontos afinados reconstroem idênticos à entrada); identidade em `forca=0`
mantida. Testes: `diag_periodo.py` (quebras/drift) + os de regressão.

---


---

## Decisões e experimentos de tempo real

### Decisões de arquitetura — tempo real (2026-06-09)

- **Caminho B antes do C.** O entregável do TCC é a **latência**, e latência mora no
  núcleo **causal** (processamento em blocos, com look-ahead limitado), não no plugin.
  O wrapper JUCE é mecânico e seria custo de toolchain (build, DAW/host) sem render
  texto. O núcleo causal é **o mesmo** para B e C → fazer B não é retrabalho; o C depois
  só chama o núcleo no `processBlock()`.
- **O pipeline atual é offline/não-causal** e precisa virar causal em 3 pontos:
  (1) **Viterbi do pYIN** → versão online com look-ahead limitado;
  (2) **suavização de vozeamento** → janela causal curta;
  (3) **marcas/PSOLA** → ring buffer com look-ahead de ~1–2 períodos.
- **Bônus de TCC:** a versão causal perde um pouco de qualidade vs a offline
  (Viterbi sem o "futuro", PSOLA com menos contexto). Esse **trade-off latência × qualidade**
  vira resultado de discussão.

### Resultados do Caminho B (2026-06-09)

Implementado: `src/offline_causal/autotune_rt.cpp` (`autotune_rt.exe`) com **Viterbi de lag
fixo** (decisão do quadro `t` olhando no máx. `look` quadros à frente). O resto (marcas +
PSOLA + cobertura) foi extraído para `src/core/dsp.h` e é **o mesmo** do offline (é local → causal).
A suavização de vozeamento não-causal foi **removida** no causal (confiamos no Viterbi).

Latência algorítmica = `look·HOP + N_FRAME` (pitch) + `~1 período` (PSOLA) + `bloco`.
Sweep com `bench_latencia.py` (entrada `audioteste.wav`, forca 1.0, tol 15, glide 40):

| look | latência | xRT | sim. offline | pipoco |
|---:|---:|---:|---:|---:|
| 0  | 41.5 ms | 0.025 | 0.562 | 0 |
| 2  | 53.1 ms | 0.025 | 0.560 | 0 |
| 4  | 64.7 ms | 0.025 | 0.654 | 0 |
| 8  | 88.0 ms | 0.025 | 0.740 | 0 |
| 16 | 134 ms  | 0.025 | 0.740 | 0 |
| 32 | 227 ms  | 0.025 | 0.740 | 0 |

**Leitura:** qualidade (semelhança com a saída offline "ouro") sobe com o look-ahead até
`look≈8` e **satura** em ~0.74; a latência cresce linear; o **xRT fica em 0.025** (≈40× mais
rápido que tempo real → sobra CPU). O **ponto ideal é `look≈8` (~88 ms)**: além disso paga-se
latência sem ganhar qualidade. O **piso de latência (~41 ms, `look=0`)** é dominado pelo
`N_FRAME=1024` (23 ms) + 1 período do PSOLA (12,5 ms) — usável em estúdio, no limite para
monitoração ao vivo. Identidade em `forca=0` e ausência de pipoco (0 spikes) mantidas em
todos os `look`.

### Experimento: reduzir `N_FRAME` para baixar o piso (2026-06-09)

`frame=`/`hop=` viraram configuráveis no `autotune_rt.exe`. O `N_FRAME` é o maior
pedaço do piso de latência (23 ms), mas tem custo físico: a janela do YIN é `frame/2`,
então a **frequência mínima detectável = `fs/(frame/2)`** sobe quando o frame encolhe, e
as notas graves passam a não ser detectadas. Sweep com `bench_nframe.py` (`look=0`):

| frame | latência | fmin detectável | xRT | sim. offline | pipoco |
|---:|---:|---:|---:|---:|---:|
| 1024 | 41.5 ms | 86 Hz  | 0.025 | 0.562 | 0 |
| 768  | 35.7 ms | 115 Hz | 0.020 | 0.483 | 0 |
| 512  | 29.9 ms | 172 Hz | 0.016 | 0.434 | 0 |
| 384  | 27.0 ms | 230 Hz | 0.011 | 0.234 | 0 |
| 256  | 24.1 ms | 345 Hz | 0.004 | 0.122 | 0 |
| 128  | 21.2 ms | 689 Hz | 0.005 | 0.122 | 0 |

**Leitura:** `frame=768` é **ganho de graça** para esta voz (35.7 ms vs 41.5 ms, qualidade
quase igual, pois a voz raramente desce abaixo de 115 Hz). De `512` para baixo a qualidade
**despenca**: a `fmin` ultrapassa as notas cantadas (~177–250 Hz), os graves viram "sem nota"
e ficam sem correção. **Conclusão:** o `N_FRAME` deve ser o **menor que ainda cubra a nota
mais grave esperada** (`frame ≈ fs / fmin_voz · 2`). Para baixar o piso ainda mais, o próximo
gargalo é o termo do **PSOLA (~12,5 ms = `fs/FMIN`)** — reduzir esse exige subir o `FMIN`.

### Experimento: `FMIN` / presets de tessitura (2026-06-09)

Atacando o **outro** termo do piso, o do PSOLA = `fs/FMIN` (período mais longo que um grão
pode ter). Subir o `FMIN` baixa a latência, mas o sistema **para de corrigir notas abaixo
dele**. Em vez de números soltos, expusemos isso como **presets de tessitura** (`voz=`), iguais
ao *Vocal Range / Input Type* do Auto-Tune, com as faixas da **classificação vocal padrão
(Fach/SATB)**: Bass E2–E4, Baritone G2–G4, Tenor C3–C5, Alto F3–F5, Mezzo A3–A5, Soprano C4–C6.

`bench_fmin.py` descobre a nota mais grave **realmente cantada** na gravação (via `dumpf0=`,
reaproveitando o detector real) e varre os presets. Para `audioteste.wav` (uma **contralto**:
grave ~G3 = 193 Hz, mediana ~A♯3 = 230 Hz, agudo ~C4), em `look=0`:

| preset | FMIN | latência | lat. PSOLA | % notas perdidas | xRT | sim. offline | pipoco |
|---:|---:|---:|---:|---:|---:|---:|---:|
| baixo     |  82 | 41.2 ms | 12.2 ms |   0.0% | 0.024 | 0.162 | 0 |
| baritono  |  98 | 39.2 ms | 10.2 ms |   0.0% | 0.025 | 0.184 | 0 |
| tenor     | 131 | 36.7 ms |  7.6 ms |   0.0% | 0.024 | 0.220 | 0 |
| **contralto** | **175** | **34.7 ms** | **5.7 ms** | **0.0%** | 0.025 | **0.236** | 0 |
| mezzo     | 220 | 33.6 ms |  4.5 ms |  25.9% | 0.023 | 0.144 | 0 |
| soprano   | 262 | 32.8 ms |  3.8 ms |  98.4% | 0.015 | 0.122 | 0 |

**Leitura:** o preset ideal é o de **maior `FMIN` que ainda não perde nenhuma nota**. Para esta
voz é **`contralto`** (FMIN = 175 Hz, logo abaixo do grave cantado, G3 ≈ 193 Hz): dá ao
mesmo tempo a **menor latência sem perda (34,7 ms)** e a **melhor qualidade (0.236)** — restringir
a grade de pitch à faixa da voz reduz erros de oitava do Viterbi causal. Subir além disso
(mezzo/soprano) baixa a latência mais 1–2 ms, mas **descarta 26%/98% das notas** (caem abaixo do
`FMIN` → "sem nota" → sem correção). **Conclusão geral:** a latência mínima do sistema é limitada
pela **nota mais grave que se quer corrigir** — casar o preset com a tessitura da voz é o que
minimiza latência *e* maximiza qualidade ao mesmo tempo.

---

