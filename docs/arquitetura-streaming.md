# Arquitetura de tempo real (streaming)

Como o algoritmo de lote — que enxergava o sinal inteiro — foi transformado num motor causal
que processa blocos de tamanho arbitrário dentro do `processBlock()` de um plugin.

> **Índice da documentação:** [`docs/README.md`](README.md)
> **Análise aprofundada e diagnóstico:** [`documentacao-tecnica.md`](documentacao-tecnica.md)

---

## Caminho C — arquitetura de tempo real (streaming) — **CONCLUÍDO**

> ✅ **Status:** o núcleo de streaming (`AutotuneStream`) está **implementado e verificado**
> (`src/c1_streaming/`) e o **plugin VST3/Standalone (JUCE)** foi **compilado e testado no
> Ableton Live** (`plugin/`). Esta seção descreve a arquitetura de tempo real: o **mesmo
> algoritmo** do `dsp.h` roda bloco a bloco dentro do `processBlock()` do plugin, em vez de
> processar o WAV inteiro de uma vez.

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
- **C2** — casca **VST3/Standalone** (JUCE via CMake `FetchContent`, build **MSVC**),
  `setLatencySamples`, GUI (forca/tol/glide/look/voz/escala).
  ✅ **FEITO:** plugin compilado e **testado no Ableton Live** (VST3 + Standalone). Ver `plugin/README.md`.

### Roteiro de estudo (fundamentação do TCC)

- **Transversal:** amostragem/Nyquist, `f=fs/τ`, conversões amostra↔tempo↔Hz, 12-TET/cents/MIDI, formantes. 📖 *DAFX* (Zölzer).
- **Tempo real (passo 0):** modelo de audio callback, **real-time safety** (nada de `malloc`/lock/IO no callback), latência algorítmica vs round-trip, PDC, ASIO×WASAPI.
- **Buffering (1–2):** **ring buffer** (ponteiros, wrap, aritmética modular), frame/hop/overlap, janelamento.
- **Pitch (3):** autocorrelação/AMDF → **YIN** (📖 de Cheveigné & Kawahara 2002) → **pYIN** (📖 Mauch & Dixon 2014); erros de oitava; vozeado/não-vozeado.
- **Viterbi (4):** cadeias de Markov/**HMM**, **programação dinâmica**, algoritmo de Viterbi (📖 Rabiner 1989), log-probabilidades, **Viterbi online / fixed-lag**, guloso×global.
- **Correção (5):** quantização 12-TET, zona morta/soft-knee, **filtro de 1 polo** (`α=e^(−1/(τ·fs))`), onset.
- **PSOLA (6):** **overlap-add**/condição **COLA**, janela de Hann, **TD-PSOLA** (📖 Moulines & Charpentier 1990), reamostragem/preservação de duração, preservação de formantes; alternativas: phase vocoder, WSOLA.
- **Plugin (C2):** JUCE (`AudioProcessor`, `processBlock`, `AudioProcessorValueTreeState`), VST3 SDK, CMake/MSVC, pluginval.
