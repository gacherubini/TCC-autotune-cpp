# TCC Autotune — protótipo em C++

Protótipo de **correção automática de afinação vocal (autotune)** escrito do zero em C++.
Programa de console offline: recebe um WAV, detecta o pitch, encosta cada nota na
afinação correta e grava um WAV corrigido. Parte prática do TCC (PUCRS, 2026).

> Projeto irmão (parte experimental em Python, comparação de algoritmos de pitch):
> `../TCC_autotune`.

---

## Como compilar

```bat
.\compilar.bat
```

Gera `autotune.exe`. O `.bat` já força o PATH do g++ (MinGW via scoop).
Toolchain: g++ 15.2 + CMake 4.3. Flags: `-std=c++17 -O2 -I external`.

## Como usar

```bat
.\autotune.exe <entrada.wav> [saida.wav] [forca 0..1] [escala] [tol=cents] [glide=ms]
```

- **forca**: `0` = sem correção (idêntico à entrada) · `0.7` = natural · `1.0` = autotune "duro".
- **escala**: `crom` (padrão, cromática) · `C`, `G`, `F#`... (maior) · `Am`, `C#m`... (menor).
- **tol=** (cents): zona morta — desvios menores que isso **não** são corrigidos, preservando
  vibrato e micro-afinação (soa natural). Padrão `0` (corrige tudo). Sugerido `10–20`.
- **glide=** (ms): suavização temporal da afinação (portamento) — a correção desliza entre
  notas em vez de "pular", tirando o efeito robô. Reinicia no ataque de cada nota.
  Padrão `0` (snap imediato). Sugerido `30–60`.
- **forca negativa** (`-1`) = modo cópia (só converte pra mono, sem processar — diagnóstico).

Exemplos:
```bat
.\autotune.exe audioteste.wav corrigido.wav 0.7                  REM natural simples
.\autotune.exe audioteste.wav corrigido.wav 1.0 Am              REM duro, Lá menor
.\autotune.exe audioteste.wav corrigido.wav 1.0 crom tol=15 glide=40   REM preset NATURAL
```

### Versão tempo real (Caminho B) — `autotune_rt.exe`

Mesmo áudio, mas com detecção de pitch **causal** (Viterbi de lag fixo) e relatório de
**latência (ms)** e **fator de tempo real (xRT)**:

```bat
.\autotune_rt.exe <in.wav> [out.wav] [forca] [escala] [tol=] [glide=] [look=L] [block=N] [frame=] [hop=] [voz=] [fmin=] [fmax=] [dumpf0=]
```

- **look=** : quadros de look-ahead do Viterbi causal. `0` = guloso (menor latência, menor
  qualidade); maior = mais perto do offline, mais latência. Padrão `4`. Ponto ideal ~`8`.
- **block=** : tamanho do bloco de áudio (entra na conta de latência). Padrão `256`.
- **frame= / hop=** : tamanho do quadro de análise e o passo. Frame menor = menos latência,
  mas detecta menos graves (mín. detectável = `fs/(frame/2)`). Padrão `1024`/`256`.
- **voz=** : preset de tessitura, igual ao *Vocal Range* do Auto-Tune. Define a faixa de busca
  `[FMIN, FMAX]` pela classificação vocal padrão (Fach/SATB):
  `baixo` (E2–E4), `baritono` (G2–G4), `tenor` (C3–C5), `contralto` (F3–F5), `mezzo` (A3–A5),
  `soprano` (C4–C6); além dos agrupamentos `lowmale` (baixo+barítono) e `altotenor`, e `instrumento` (faixa ampla).
- **fmin= / fmax=** : faixa de busca em Hz (sobrescreve o `voz=`). **`fmin` domina o piso de
  latência** (termo PSOLA = `fs/fmin`): subir `fmin` baixa a latência, mas para de corrigir
  notas abaixo dele.
- **dumpf0=** : grava o F0 detectado por quadro num `.txt` (só análise; não afeta a saída).

```bat
.\autotune_rt.exe audioteste.wav rt.wav 1.0 crom tol=15 glide=40 look=8 voz=contralto
```

---

## Como funciona (pipeline)

1. **Ler WAV → mono** (`dr_wav.h`, header-only).
2. **Detecção de pitch (pYIN)**: YIN (CMNDF) com multi-limiar Beta(2,18) → matriz de
   observação → **HMM/Viterbi** sobre uma grade de pitch de 20 cents. Resultado: F0
   por quadro, estável e com decisão de vozeado/não-vozeado.
   - Parâmetros (topo do `src/core/dsp.h`): `N_FRAME=1024`, `N_HOP=256`, `FMIN=80`, `FMAX=1000`.
3. **Suavização do vozeamento**: tampa buracos curtos e remove ilhas curtas.
4. **Nota-alvo**: encosta na nota mais próxima da escala, com intensidade `forca` e
   **zona morta** `tol` (não corrige desvios pequenos → vibrato/expressão preservados).
5. **Trajetória com glide**: a afinação-alvo é suavizada no tempo (filtro de 1 polo, com
   reset no ataque de cada nota) → portamento natural entre notas, sem efeito robô.
6. **Correção (TD-PSOLA)**: marcas de análise por período (alinhadas por correlação),
   síntese por overlap-add no novo período, reconstrução por cobertura. Como copia grãos
   no tempo e só muda o espaçamento, **preserva os formantes** (timbre).
7. **Gravar WAV** 16-bit PCM mono.

---

## Estrutura

```
src/core/dsp.h               DSP compartilhada (CMNDF, nota-alvo, TD-PSOLA, WAV...) — usada por TUDO
src/offline_causal/main.cpp        autotune OFFLINE (referência/gold) -> autotune.exe
src/offline_causal/autotune_rt.cpp autotune CAUSAL (Viterbi lag fixo) -> autotune_rt.exe
src/c1_streaming/autotune_stream.h núcleo C1 de STREAMING (header-only, usado pelo plugin)
src/c1_streaming/stream_test.cpp   driver headless do C1            -> stream_test.exe
plugin/                       Caminho C2: plugin VST3/Standalone (JUCE) em volta do C1
external/dr_wav.h             leitor/gravador WAV (header-only)
compilar.bat                  build rápido com g++ (compila os 3 exes)
audioteste.wav                gravação de teste (entrada)
formantes.py                  verifica preservação dos formantes (entrada vs saída)
bench_stream.py / bench_pitch.py / bench_frames.py  validam C1 vs gold
bench_latencia.py             varre look-ahead: latência × qualidade × xRT (resultado do TCC)
bench_nframe.py               varre N_FRAME: piso de latência × fmin detectável × qualidade
bench_fmin.py                 varre presets de tessitura (FMIN): latência × % notas perdidas × qualidade
```

Os testes em Python usam o venv do projeto irmão:
`..\TCC_autotune\.venv\Scripts\python.exe`.

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

## Testes de regressão

Se mexer na síntese e voltar a estalar, rode:

```bat
REM 1) forca=0 tem que ser IDENTIDADE (lag 0 e std 0 em todas as janelas)
.\autotune.exe audioteste.wav _t0.wav 0
..\TCC_autotune\.venv\Scripts\python.exe teste_fase.py audioteste.wav _t0.wav

REM 2) saida nao pode introduzir saltos/cliques (comparar com o input)
.\autotune.exe audioteste.wav _t1.wav 1
..\TCC_autotune\.venv\Scripts\python.exe ab_clicks.py audioteste.wav _t1.wav
```

---

## Próximos passos (TODO)

- [x] **Força em cents** — zona morta `tol` com soft knee. ✓ 2026-06-09
- [x] **Suavização da trajetória de pitch** — `glide` (portamento). ✓ 2026-06-09
- [x] **Formantes** — verificado que são preservados (`formantes.py`). ✓ 2026-06-09
- [x] **Caminho B: motor causal/streaming + medição de latência** ✓ 2026-06-09 (ver abaixo).
- [~] Caminho C: plugin JUCE (VST3) por cima do mesmo núcleo causal. **C1 (núcleo streaming) = FEITO e verificado** (2026-06-11); **C2 (casca VST3) = esqueleto criado em `plugin/`**, falta compilar com MSVC + testar no Ableton. (Ver seção "Caminho C" abaixo + `plugin/README.md` + spec em `docs/superpowers/specs/`.)
- [x] `frame=`/`hop=` configuráveis no `autotune_rt.exe` (experimento de piso de latência). ✓ 2026-06-09
- [x] `FMIN`/`FMAX` por flag + **presets de tessitura** (`voz=`, estilo Auto-Tune); reduz o termo PSOLA da latência. ✓ 2026-06-09
- [ ] `voz=auto`: detectar a tessitura automaticamente (como o "Auto Detect" do Auto-Tune) a partir do F0.
- [ ] Adicionar `voz=`/`fmin=`/`fmax=` também ao `autotune.exe` (offline) para uso prático.
- [ ] Estimar formantes por **LPC** (mais robusto que cepstro em pitch alto) — opcional.

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

## Caminho C — arquitetura de tempo real (streaming) — **EM DESIGN**

> ⚠️ **Status:** esta seção descreve o **design** do núcleo streaming (`AutotuneStream`),
> ainda **não implementado**. Hoje existe a versão em **lote** (`autotune_rt.exe`), que é
> causal *no algoritmo* mas processa o WAV inteiro de uma vez. O Caminho C reescreve o
> **mesmo algoritmo** para rodar bloco a bloco dentro de um `processBlock()` de plugin
> (VST3 via JUCE), reaproveitando ao máximo as primitivas do `dsp.h`. Design completo em
> `docs/superpowers/specs/2026-06-10-caminho-c-streaming-design.md`.

### Diferença essencial: lote vs. streaming

A versão de lote **conhece o sinal inteiro de antemão** (Viterbi sobre todos os quadros,
PSOLA sobre todo o sinal). Um plugin **não tem o futuro**: a DAW entrega blocos pequenos
(`n` amostras, ex.: 128) por vez, em tamanho escolhido pelo **host** (não por nós), e exige
resposta imediata. O streaming roda peça por peça, guardando **estado entre chamadas**.

### Modelo mental: 3 fronteiras que andam pra frente

- **① entrada** — até onde já chegou áudio.
- **② decisão** — até onde o pitch já foi decidido (fica `look` quadros atrás de ①).
- **③ síntese** — até onde o PSOLA já gerou saída (fica ~1 período atrás de ②).

**Latência = distância entre ① e ③.** Cada `process()` só move as 3 fronteiras um pouco.

### O pipeline, passo a passo

| # | passo | o que faz | fonte de latência |
|---|---|---|---|
| **0** | `prepare(fs)` | aloca ring buffers + estados; calcula a latência uma vez (sem alocar no `process`) | — |
| **1** | `process(in,out,n)` | estoca as `n` novas amostras no **ring de entrada**; `desdeUltimo += n` | — |
| **2** | disparo de quadro | `while (buffered≥frame && desdeUltimo≥hop)`: fatia as últimas `frame` amostras (juntando a volta do anel) e `desdeUltimo -= hop` | ⏳ `frame` |
| **3** | análise de pitch | YIN/CMNDF + multi-limiar **Beta(2,18)** → mapa de probabilidade `obs[bin]` + `pUnv` (idêntico ao offline, 1 quadro) | — |
| **4** | **Viterbi de lag fixo** | 1 passo do HMM (emissão+transição); guarda só `look+1` colunas de `psi`; **emite o quadro `t−look`** por backtrack curto | ⏳ `look·hop` |
| **5** | nota-alvo + glide | F0 detectado → encosta na escala (`forca`, zona morta `tol`) + portamento (`glide`, filtro de 1 polo com reset no ataque) | — |
| **6** | **PSOLA online** | re-sintetiza em janela deslizante (reusa `psolaSintetiza`) e comete o miolo já estável; look-ahead de **2 períodos** | ⏳ `2·fs/FMIN` |
| **7** | pull da saída | copia `n` amostras já prontas pro `out[]` (no início, silêncio = priming) | — |
| **8** | repete | cada chamada move as 3 fronteiras; tudo que era "global" no offline vira uma frente com look-ahead limitado | — |

### Orçamento de latência (o número que vai pro Ableton)

```
latência = frame + look·hop + 2·fs/FMIN
```

Exemplo (fs=44100, frame=1024, hop=256, look=2, voz=contralto → FMIN=175):
`1024 + 512 + 504` = **2040 amostras = 46,3 ms**, reportado ao host via
`setLatencySamples(2040)` (aparece na barra do Ableton). Os três termos são as três
esperas ⏳ da tabela. **Nota:** PDC alinha faixas gravadas, mas **não** remove o atraso que
se *ouve* ao monitorar ao vivo — por isso `look` pequeno (0–4) e ASIO para cantar.

> **Por que 2·fs/FMIN (e não 1)?** O termo do PSOLA é a folga (look-ahead) entre a
> fronteira de **decisão** e a de **síntese**. A re-síntese em janela reusa o
> `psolaSintetiza` do lote, cujo espaçamento de grãos foi tornado **invariante a
> truncamento** (passos inteiros de β ancorados no início da nota — antes dependia de
> *onde a nota terminava*, o que causava **drift de fase** no streaming). Sobra um
> efeito de borda: o grão da **última** marca da janela tem largura estimada pela marca
> anterior (a próxima ainda não chegou). 2 períodos de folga garantem que esse grão
> instável nunca alcança a região já finalizada. Com isso a saída do streaming
> reproduz a do lote: **idêntica em `forca=0`** e correlação **~0,997** em `forca=1`
> (resíduo = jitter de fase de <3 amostras por nota, >0,999 por região).

### Decomposição do trabalho

- **C1** — núcleo streaming `AutotuneStream` (C++ puro, **sem JUCE**) + **verificação headless**.
  ✅ **FEITO (2026-06-11):** `src/c1_streaming/autotune_stream.h` + `src/c1_streaming/stream_test.cpp`. Identidade
  **bit-perfeita** em `forca=0`; **0 pipoco**; invariante ao tamanho de bloco; trilha de F0
  100% vs gold; saída vs gold **0,997** em `forca=1` (drift do PSOLA online eliminado por
  espaçamento de grãos invariante a truncamento; resíduo = jitter de fase <3 amostras/nota,
  >0,999 por região). *(o valor técnico mora aqui)*
- **C2** — casca **VST3** (JUCE via CMake `FetchContent`, build **MSVC**), `setLatencySamples`,
  GUI mínima (forca/tol/glide/look/voz/escala), testado no Ableton + `validator` do VST3 SDK.
  🚧 **esqueleto criado em `plugin/`** (`CMakeLists.txt` + `PluginProcessor.h/.cpp` + `build.bat`);
  falta compilar (depende do MSVC Build Tools) e testar no Ableton. Ver `plugin/README.md`.

### Roteiro de estudo (fundamentação do TCC)

- **Transversal:** amostragem/Nyquist, `f=fs/τ`, conversões amostra↔tempo↔Hz, 12-TET/cents/MIDI, formantes. 📖 *DAFX* (Zölzer).
- **Tempo real (passo 0):** modelo de audio callback, **real-time safety** (nada de `malloc`/lock/IO no callback), latência algorítmica vs round-trip, PDC, ASIO×WASAPI.
- **Buffering (1–2):** **ring buffer** (ponteiros, wrap, aritmética modular), frame/hop/overlap, janelamento.
- **Pitch (3):** autocorrelação/AMDF → **YIN** (📖 de Cheveigné & Kawahara 2002) → **pYIN** (📖 Mauch & Dixon 2014); erros de oitava; vozeado/não-vozeado.
- **Viterbi (4):** cadeias de Markov/**HMM**, **programação dinâmica**, algoritmo de Viterbi (📖 Rabiner 1989), log-probabilidades, **Viterbi online / fixed-lag**, guloso×global.
- **Correção (5):** quantização 12-TET, zona morta/soft-knee, **filtro de 1 polo** (`α=e^(−1/(τ·fs))`), onset.
- **PSOLA (6):** **overlap-add**/condição **COLA**, janela de Hann, **TD-PSOLA** (📖 Moulines & Charpentier 1990), reamostragem/preservação de duração, preservação de formantes; alternativas: phase vocoder, WSOLA.
- **Plugin (C2):** JUCE (`AudioProcessor`, `processBlock`, `AudioProcessorValueTreeState`), VST3 SDK, CMake/MSVC, pluginval.
