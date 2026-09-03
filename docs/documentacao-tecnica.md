# Documentação técnica — protótipo de autotune

**TCC · PUCRS · Desenvolvimento de um Protótipo Gratuito de Correção Automática de Afinação Vocal**
Gabriel Abreu Cherubini · Orientador: Dr. Marco Mangan

Documentação técnica consolidada dos dois repositórios do trabalho, do diagnóstico dos
problemas encontrados no teste de usuário e das alternativas de solução para o TCC 2.

**Documento complementar:** [`teste-de-usuario.md`](teste-de-usuario.md) —
registro do teste de usuário, sem soluções.
**Texto do TCC:** [`gacherubini/TCC-TEXT`](https://github.com/gacherubini/TCC-TEXT) —
fonte LaTeX, em repositório separado.

### Posição no cronograma

Este documento é a entrega da **Sprint 11** (24/08 – 06/09) do cronograma do TCC 2:
*"Consolidação do feedback — análise das sessões; identificação de funcionalidades ausentes
e de problemas de usabilidade; **priorização das melhorias**. Entrega: feedback
sistematizado e **backlog de melhorias priorizado**."*

O backlog priorizado está na Seção 10.

---

## ⚠️ Errata — leia antes de citar este documento

Este documento foi escrito **antes** da revisão bibliográfica de 2026-08-26. Oito afirmações
dele foram corrigidas desde então, entre elas:

- a §8.2 afirma que o protótipo implementou "o mecanismo errado" — **não implementou**;
- a §9.2 apresenta o **C1** como contribuição original — ele está na patente do Auto-Tune de 1997;
- a §9.1 descreve o mecanismo do **L6** como redução de janela — **erra o mecanismo**;
- a §10 coloca o L6 no item 17 do backlog — ele deveria estar **no topo**;
- a meta de latência do **RNF01** (≤ 20 ms) não tem respaldo revisado por pares.

**O texto original foi mantido de propósito**, como registro do que se acreditava antes da
revisão — apagá-lo destruiria o rastro que o TCC precisa mostrar.

👉 **Lista completa, com o que mudou e por quê:**
[historico-e-decisoes.md § Errata](historico-e-decisoes.md#errata--afirmações-corrigidas-pela-pesquisa-bibliográfica-2026-08-26)
👉 **Fontes:** [pesquisa-bibliografica.md](pesquisa-bibliografica.md)
👉 **Decisões tomadas depois:** [comparacao-antares.md](comparacao-antares.md) e
[modo-baixa-latencia.md](modo-baixa-latencia.md)

---

## Sumário

1. [Os dois repositórios](#1-os-dois-repositórios)
2. [A trilha A → B → C1 → C2](#2-a-trilha-a--b--c1--c2)
3. [Repositório Python: benchmark de detectores de pitch](#3-repositório-python-benchmark-de-detectores-de-pitch)
4. [Repositório C++: o pipeline em 7 estágios](#4-repositório-c-o-pipeline-em-7-estágios)
5. [De lote a streaming: as três fronteiras](#5-de-lote-a-streaming-as-três-fronteiras)
6. [O plugin VST3](#6-o-plugin-vst3)
7. [O que já está medido](#7-o-que-já-está-medido)
8. [Diagnóstico dos problemas](#8-diagnóstico-dos-problemas)
9. [Soluções possíveis](#9-soluções-possíveis)
10. [Backlog priorizado e plano por sprint](#10-backlog-priorizado-e-plano-por-sprint)
11. [Glossário técnico](#11-glossário-técnico)
12. [Mapa de arquivos e referências](#12-mapa-de-arquivos-e-referências)

---

## 1. Os dois repositórios

O trabalho está partido em duas metades que respondem a perguntas diferentes.

| | `TCC-autotune-python` | `TCC-autotune-cpp` |
|---|---|---|
| **Pergunta** | Qual algoritmo de detecção de pitch serve para autotune ao vivo? | Dá para construir um autotune gratuito que rode ao vivo numa DAW? |
| **Papel no TCC** | Fundamentação (Caps. `chap:algoritmos_pitch`, `chap:estudo_comparativo`, `chap:resultados`) | Artefato (Cap. `chap:implementacao`) |
| **Conteúdo** | 4 algoritmos, 9 sinais sintéticos, 40 gravações do Vocadito, 6 métricas, 3 suavizadores | 3 executáveis + plugin VST3, pipeline pYIN + TD-PSOLA próprio |
| **Período** | mai–jun/2026 · 9 commits | jun/2026 · 5 commits |

A ligação entre os dois é um número: o benchmark concluiu que **o pYIN é ~23× mais estável
que o YIN** sobre voz real (σ de estabilidade 29,4 vs. 671,6 cents no Vocadito), e foi o
pYIN que virou C++.

> **Lacuna de reprodutibilidade.** No repositório Python, `results/` e `data/` estão no
> `.gitignore`. Os CSVs de resultado, as figuras e o Vocadito não estão versionados — nenhum
> número do benchmark é reproduzível a partir do que está commitado sem baixar o dataset e
> rodar tudo de novo. Versionar os CSVs (são pequenos) é praticamente de graça e fecha um
> buraco de método antes da defesa.

---

## 2. A trilha A → B → C1 → C2

O repositório C++ foi construído em quatro etapas nomeadas, cada uma com uma justificativa
metodológica. Entender essa sequência é entender por que o código está organizado assim.

### Caminho A — offline
`src/offline_causal/main.cpp` → `autotune.exe`

Lê o WAV inteiro, conhece o futuro, roda o Viterbi global. É o **padrão-ouro**: a melhor
qualidade que este algoritmo consegue produzir. Todo o resto é medido contra ele.

### Caminho B — causal
`src/offline_causal/autotune_rt.cpp` → `autotune_rt.exe`

Mesmo áudio, mas a decisão de pitch do quadro *t* só pode olhar `look` quadros à frente
(**Viterbi de lag fixo**). Ainda processa arquivo, mas **reporta latência algorítmica e
xRT**. A decisão de arquitetura registrada no repositório: *o entregável do TCC é a
latência, e latência mora no núcleo causal, não no plugin* — por isso B veio antes de C.

### Caminho C1 — streaming
`src/c1_streaming/autotune_stream.h`

A classe `AutotuneStream`: C++ puro, sem JUCE, processando blocos de tamanho arbitrário com
estado entre chamadas. Verificado headless contra o gold. **É aqui que mora o valor
técnico** — transformar um algoritmo que via o sinal inteiro num que vê 256 amostras por vez.

### Caminho C2 — plugin
`plugin/` → `TCC Autotune.vst3`

Casca JUCE em volta do C1: parâmetros (APVTS), `processBlock()`, `setLatencySamples()`, GUI
com afinador. Compilado e testado no Ableton Live. **Zero DSP novo** — é adaptador de host.

> **Por que isso importa para o TCC 2:** essa separação é o que permite atacar a latência sem
> quebrar nada. Qualquer mudança entra no `dsp.h` ou no `autotune_stream.h`, e as três suítes
> de verificação (`bench_pitch.py`, `bench_frames.py`, `bench_stream.py`) dizem imediatamente
> se o streaming ainda reproduz o gold.

---

## 3. Repositório Python: benchmark de detectores de pitch

### 3.1 Os quatro algoritmos

**Autocorrelação** — `algorithms/reference.py` · causal
Janela Hann, `librosa.autocorrelate`, normaliza por `acf[0]`, procura o pico maior que
`voicing_threshold = 0.3` na faixa de lags `[fs/fmax, fs/fmin]`. Sofre de **erro de
oitava**: o pico em 2τ compete com o pico em τ.

**YIN** (de Cheveigné & Kawahara, 2002) — causal
Troca autocorrelação por **função de diferença** e normaliza cumulativamente (CMNDF). Isso
mata a preferência estrutural por lags longos. Escolhe o *primeiro* mínimo abaixo de um
limiar — não o menor. Rápido, mas o limiar único faz a curva de F0 tremer.

**pYIN** (Mauch & Dixon, 2014) — não-causal
YIN probabilístico: em vez de *um* limiar, roda **100 limiares** ponderados por uma
distribuição Beta e trata os candidatos como uma distribuição de probabilidade. Depois um
**HMM + Viterbi** escolhe a trajetória globalmente mais provável.

**SWIPE′** (Camacho, 2008) — `algorithms/swipe.py` · implementação própria · causal
Método espectral: compara `√|X(f)|` com um **kernel de peneira harmônica** — lobos `cos²`
centrados em cada harmônico *k·p* com peso `1/√k`. A grade de candidatos é uniforme em
**ERB** (passo 1/48), não em Hz, porque a percepção de altura é logarítmica. Sem Viterbi →
puramente frame-a-frame → causal por construção.

> **Detalhe fino da implementação do SWIPE′.** O kernel é **zero-mean sobre o suporte ativo**
> (de `p/2` até o último harmônico `+ p/2`). Sem isso, candidatos de *p* pequeno acumulam
> mais lobos dentro da banda e o `argmax` fica enviesado para `fmax` sob ruído. Com a remoção
> da média, `Σ kernel ≈ 0` → ruído branco produz força ≈ 0 para qualquer candidato.

### 3.2 Os sinais de teste

**Sintéticos** (`synth.py`) — o ground truth é exato porque o sinal foi gerado a partir dele:
senoide 220/440/880 Hz, harmônico com 5 parciais em `1/k`, ruidoso a SNR 20 dB e 10 dB,
**vibrato** (5 Hz, ±50 cents, fase por soma cumulativa da frequência instantânea) e
**glissando** 220→440 Hz.

**Real** — **Vocadito** (Zenodo 5578807, CC-BY-4.0): 40 gravações de voz cantada a cappella,
44,1 kHz mono, com anotação de F0 por quadro em CSV. O `align_f0_to_times()` reamostra a
anotação para a grade temporal de cada algoritmo antes de comparar — cada algoritmo tem seu
próprio `hop`, então comparar sem alinhar seria comparar tempos diferentes.

### 3.3 As seis métricas

| Métrica | O que mede | Critério | Direção |
|---|---|---|---|
| RPA | Raw Pitch Accuracy | % de quadros vozeados com erro ≤ 50 cents | maior |
| RCA | Raw Chroma Accuracy | igual à RPA, módulo 1200 cents (ignora oitava) | maior |
| GPE | Gross Pitch Error | % com `\|f_pred/f_true − 1\| > 0,2` | menor |
| σ_estab | estabilidade temporal | σ das diferenças quadro-a-quadro, em cents | menor |
| Tempo | custo de CPU | média de N execuções (`perf_counter`) | menor |
| Mem | pico de alocação | `tracemalloc` | menor |

A métrica que decide o autotune é a **estabilidade**, não a RPA. Um detector pode acertar a
nota (RPA alta) e ainda assim tremer ±40 cents entre quadros — e esse tremor vai direto para
a saída via `beta` do PSOLA. Essa constatação é registrada no TCC como a **primeira
contribuição do trabalho**.

### 3.4 Suavização causal e a contabilidade de look-ahead

O `smoothing.py` opera sempre **em cents**, não em Hz — suavizar em Hz enviesa o resultado,
porque a mesma diferença em Hz vale intervalos musicais diferentes em regiões graves e
agudas. Três famílias, com o custo de latência declarado:

| Suavizador | Janela | Look-ahead | Nota |
|---|---|---|---|
| `causal_median(k)` | `[i−k+1, i]` | 0 quadros | só passado — grátis em latência |
| `centered_median(k)` | `[i−k/2, i+k/2]` | (k−1)/2 quadros | paga latência para ganhar estabilidade |
| `ema(α)` | recursiva | 0 quadros | `y[i] = αx[i] + (1−α)y[i−1]` |

O `scripts/06_realtime_benchmark.py` junta tudo: mede tempo **por quadro** (não por áudio
inteiro), varre `frame_length ∈ {512, 1024, 2048}`, e calcula
`latência = frame_length/sr + look_ahead_do_suavizador`. O comentário dele já cravava a meta:

> *"autotune ao vivo tolera no máximo ~20–30 ms antes de incomodar o cantor"*

**Amarração fina:** `_safe_fmin()` existe porque `librosa.yin` exige `frame_length ≥ sr/fmin`.
Ao varrer `frame_length` para baixo, o `fmin` pedido (65 Hz) fica ilegal e o script sobe o
`fmin` automaticamente. **É exatamente o mesmo trade-off que reaparece no C++** (§8.1):
janela menor = menos latência, mas para de enxergar os graves.

---

## 4. Repositório C++: o pipeline em 7 estágios

Do WAV ao WAV. Cada estágio abaixo é código real em `src/core/dsp.h` — 309 linhas
compartilhadas, sem cópia, pelo offline, pelo causal, pelo streaming e pelo plugin.

```
entrada → CMNDF → 100 limiares Beta → HMM/Viterbi → nota-alvo+glide → TD-PSOLA → saída
           ⏳                              ⏳                              ⏳
        1 quadro                    look quadros                    2 períodos
        23,2 ms                        23,2 ms                        11,4 ms
                    latência total = 2552 amostras = 57,9 ms @ 44,1 kHz
```

**Só três dos sete estágios custam latência** — e os três custam quase a mesma coisa. Todo o
resto é aritmética local, de custo desprezível.

### Estágio 1 — Leitura e conversão para mono

`dr_wav.h` (header-only, sem dependência externa) lê o arquivo em `float32`. Os canais viram
mono pela **média**. No plugin, o mesmo acontece bloco a bloco em `PluginProcessor.cpp:186`,
e a saída mono é espalhada de volta em todos os canais.

### Estágio 2 — CMNDF: o coração do YIN

Para um quadro que começa em `ini`, com janela `W = frame/2`:

```
d(τ)  = Σⱼ (x[ini+j] − x[ini+j+τ])²          // j = 0 .. W−1
d′(τ) = d(τ) · τ / Σ_{k≤τ} d(k)              // normalização cumulativa
```

A normalização cumulativa é a ideia central do YIN: sem ela, `d(τ)` tende a crescer com τ e o
algoritmo prefere lags longos → **erro de oitava para baixo**. Com ela, `d′(τ) ≈ 1` para lags
"sem sentido" e mergulha perto de zero no período verdadeiro.

O `candidato()` percorre τ de `tauMin = fs/FMAX` até `tauMax = W`, pega o **primeiro** τ com
`d′(τ) < s`, desce até o fundo do vale local, e refina com **interpolação parabólica**:
`τ̂ = τ + (s₀−s₂)/(2(s₀−2s₁+s₂))`. Sem esse refino, a resolução seria de 1 amostra de período
— em 440 Hz a 44,1 kHz isso são ~39 cents de erro de quantização.

> **Consequência direta de `W = frame/2`:** como `tauMax = W`, a menor frequência detectável é
> `fs/(frame/2)`. Com `frame = 1024` a 44,1 kHz: **86 Hz**. Esse único fato governa metade do
> orçamento de latência.

### Estágio 3 — Multi-limiar Beta

O YIN clássico usa *um* limiar e vive ou morre com ele. O pYIN roda `K = 100` limiares
`s_k = (k+0,5)/100` e pondera cada resultado por `w_k ∝ s(1−s)¹⁷` — o núcleo de uma
distribuição **Beta(2,18)**, que concentra massa em torno de s ≈ 0,1 sem descartar os
extremos.

```
for k in 0..99:
    f = candidato(d′, τmin, τmax, s_k)
    if f > 0:  obs[binDe(f)] += w_k;  massa += w_k
pUnv = 1 − massa      // probabilidade de "não-vozeado" = quanto ninguém votou
```

O resultado é um **histograma de probabilidade sobre a grade de pitch** mais uma massa
residual de "não é voz". Nenhuma decisão foi tomada ainda — a decisão fica para o Viterbi.

### Estágio 4 — HMM e Viterbi

A grade de pitch é logarítmica com resolução `RES_CENTS = 20`:

```
bin(f) = round( 1200 · log₂(f / FMIN) / 20 )
f(bin) = FMIN · 2^(bin · 20 / 1200)
```

Estados: um por bin de pitch, mais um estado **UV** (não-vozeado).

| Transição | Custo em log | Leitura musical |
|---|---|---|
| vozeado b₁ → vozeado b₂ | `log(0,99) − ½((b₂−b₁)/2)²` | gaussiana de σ = 2 bins = **40 cents**, janela ±12 bins = ±240 cents |
| UV → vozeado | `log(0,01)` | ataque de nota é raro por quadro |
| vozeado → UV | `log(0,01)` | fim de nota é raro por quadro |
| UV → UV | `log(0,99)` | silêncio tende a continuar |
| emissão | `log(obs[b] + ε)` ou `log(pUnv + ε)` | o que os 100 limiares votaram |

**No offline**, guarda-se a matriz `psi` inteira e faz-se o backtrack a partir do último
quadro: a trajetória é ótima *globalmente*. Depois, `suavizarVozeamento()` tampa buracos de
até 4 quadros e remove ilhas de menos de 4 quadros — operação inerentemente não-causal, e por
isso **removida na versão causal**.

> **Armadilha registrada:** `W_TRANS = 12` e `SIGMA_TRANS = 2` estão em **bins**, não em
> cents. Mudar `RES_CENTS` sem reescalar essas duas constantes **muda o modelo probabilístico**
> — ver §9.2.1.

### Estágio 5 — Nota-alvo: quantização com zona morta

```
midi   = 69 + 12·log₂(f/440)              // 12-TET, A4 = 440 Hz
alvo   = nota mais próxima permitida pela escala (busca ±7 semitons)
errCt  = (alvo − midi) · 100              // desvio em cents
mov    = |errCt| ≤ tol ? 0 : sign(errCt)·(|errCt| − tol)   // soft knee
corr   = midi + forca · mov / 100
```

Três controles em três linhas. `forca` é a fração do desvio que se corrige. `tol` é a **zona
morta** em cents: desvios menores passam intactos, e a subtração de `tol` fora dela garante
continuidade — sem degrau na fronteira. `g_permitida[12]` define quais classes de nota são
alvos válidos (cromática, maior ou menor natural).

### Estágio 6 — Glide: portamento por filtro de 1 polo

```
α = exp(−1 / (τ · fs))                                        // τ = glide_ms/1000
estado = tinhaNota ? α·estado + (1−α)·alvoCents : alvoCents    // ← reset
fout   = FMIN · 2^(estado/1200)
```

**O detalhe que importa:** `tinhaNota` só volta a `false` quando o quadro é *não-vozeado*.
Dentro de uma frase legato, o glide funciona e desliza entre notas; mas em todo ataque de
nota depois de uma respiração, o `else` dispara e a afinação de saída **já nasce exatamente
em cima do alvo**. Ver §8.2.

### Estágio 7 — TD-PSOLA

*Time-Domain Pitch-Synchronous Overlap-Add.* A ideia física: um som vozeado é uma sequência
de pulsos glotais espaçados de `T = fs/F0`, filtrados pelo trato vocal. Se você **copiar** os
pulsos e apenas mudar o espaçamento entre eles, muda a frequência fundamental e **não** o
filtro — o timbre fica.

```
ANÁLISE   |----T----|----T----|----T----|----T----|      marcas por período
SÍNTESE   |--T/β--|--T/β--|--T/β--|--T/β--|--T/β--|      mesmo intervalo de tempo,
                                                          β = f_alvo / f_real
```

Com β > 1 (subir a afinação) cabem mais grãos no mesmo intervalo de tempo, então alguns são
*reusados*. Como cada grão é um recorte literal da entrada — nunca reamostrado — o envelope
espectral (os formantes) atravessa o processo intacto.

**7a · Marcas de análise.** Em cada região vozeada: acha-se o pico de `|x|` no primeiro
período como âncora, e a partir dele encadeia-se marca a marca. Cada próxima marca é
procurada em `m + T ± T/4`, escolhendo a posição de **correlação cruzada máxima** com a marca
anterior (janela ±T/2). Isso mantém as marcas coerentes em fase — sem isso, o overlap-add
produz cancelamento.

**7b · Marcas de síntese com preservação de duração.** Define-se
`β_k = f_alvo(marca_k) / f_real(marca_k)` e acumula-se por trapézio:
`cum[k] = cum[k−1] + ½(β_{k−1} + β_k)`. Os grãos de saída são colocados nos pontos onde `cum`
atinge valores **inteiros** (0, 1, 2, …), ancorados no *início* da região.

> **Invariância a truncamento — a chave do streaming.** Como cada grão *j* depende só das
> marcas **até ali** — nunca de `cum[M−1]`, o total da região — re-sintetizar a mesma região
> com um trecho maior ou menor **não desloca os grãos já posicionados**. A versão anterior
> usava `tc = j·total/(K−1)`, o que amarrava todo grão ao *fim* da região; no streaming, a
> região é truncada a cada bloco, então o fim mudava a cada chamada → **drift de fase**.
> Trocar isso foi o que fez o streaming reproduzir o lote com correlação 0,997.

**7c · Reconstrução por cobertura.**

```
wet  = wsum[i] > ε ? y[i]/wsum[i] : x[i]
w    = clamp(wsum[i], 0, 1)
out  = w·wet + (1−w)·x[i]        // blend com o seco pela própria rampa Hann
```

Onde a cobertura dos grãos é total (`wsum ≥ 1`), a saída é 100% processada; nas bordas, ela
desliza suavemente de volta para o sinal seco. Foi essa mudança — junto com reancorar o
cursor de síntese numa marca real ao sair de cada silêncio — que eliminou o bug de "pipoca"
(cliques).

---

## 5. De lote a streaming: as três fronteiras

O algoritmo de lote conhece o sinal inteiro. Um plugin recebe 256 amostras e tem que
responder agora. O modelo mental que resolve isso — e que define a latência — são três
fronteiras que andam para frente.

```
passado ──────────────────────────────────────────────────────────► agora
        ...processado...  │  ...decidido...  │  ...só recebido...
                          ③                  ②                    ①
                       síntese             decisão             entrada
                          └──── 2·fs/FMIN ───┴─ frame+look·hop ──┘
                          └────────── latência = ① − ③ ──────────┘
                                    2552 amostras = 57,9 ms
```

- **① entrada** — até onde já chegou áudio.
- **② decisão** — até onde o pitch já foi decidido (fica `look` quadros atrás de ①).
- **③ síntese** — até onde o PSOLA já gerou saída (fica ~2 períodos atrás de ②).

**A latência não é um número escolhido — é a distância entre a amostra que entra e a amostra
que sai.** Cada chamada de `process()` empurra as três fronteiras um pouco para a direita; os
espaçamentos entre elas ficam constantes, e a soma deles é o que o cantor ouve.

### 5.1 Como cada peça virou causal

| Peça | No lote | No streaming | Custo |
|---|---|---|---|
| Buffer | vetor com o sinal inteiro | **ring buffer** de `frame+8` amostras | — |
| Disparo de quadro | `for q in 0..numQ` | `while (buffered≥frame && desdeUltimo≥hop)` | frame |
| Viterbi | backtrack do último quadro | **lag fixo**: anel de `look+1` colunas psi | look·hop |
| Vozeamento | tampa buracos ±4 quadros | *removido* | — |
| PSOLA | uma passada no sinal todo | **overlap-save** em janela deslizante | 2·fs/FMIN |

### 5.2 O `while` que torna a saída independente do host

A DAW escolhe o tamanho do bloco — 64, 128, 256, 512 — e o algoritmo não pode depender disso.
Por isso o disparo de quadros usa `while`, não `if`: se o bloco do host for maior que o `hop`,
vários quadros ficam prontos dentro do processamento de uma única amostra, e todos são
disparados antes de seguir. E `desdeUltimo -= nHop` (em vez de `= 0`) preserva o "troco".
Resultado verificado: **saída idêntica para blocos de 64, 128, 256 e 512**.

### 5.3 Viterbi de lag fixo

Em vez de guardar as `numQ` colunas de `psi`, guarda-se um anel de `look+1`. A cada novo
quadro, parte-se do melhor estado da coluna mais recente e retrocede-se `look` passos —
emitindo a decisão do quadro `t − look`. Com `look = 0` o decodificador vira **guloso**
(decide só com o presente); com `look → ∞` converge para o Viterbi global.

### 5.4 PSOLA online por overlap-save

A jogada aqui foi *não* reescrever a detecção de marcas em modo online. Em vez disso,
re-sintetiza-se uma **janela** do sinal acumulado usando a mesma `psolaSintetiza()` já
validada, e comete-se só o miolo novo `[synthFront, alvo)`. Para que a fase bata com a do
lote, a janela recua até o **início da região vozeada** — a mesma âncora que o lote usaria:

```cpp
while (winStart > 0 && f0samp[winStart−1] > 0.0f) --winStart;   // autotune_stream.h:481
```

> **Marque esta linha.** Ela é a razão da correção soar correta — e é também um dos achados de
> performance da §8.3.

### 5.5 Por que 2 períodos de guarda, e não 1

O grão da **última** marca da janela tem largura estimada pela marca *anterior*, porque a
próxima ainda não chegou. Esse grão é instável. Dois períodos do tom mais grave garantem que
ele nunca alcance a região já finalizada. Custo: `2·fs/FMIN` amostras — 11,4 ms para
FMIN = 175 Hz.

---

## 6. O plugin VST3

232 linhas de `PluginProcessor.cpp` e nenhuma linha de DSP nova.

### 6.1 Parâmetros "ao vivo" vs. "estruturais"

A distinção é de segurança em tempo real, e está codificada explicitamente:

| Parâmetro | Faixa | Padrão | Tipo | Por quê |
|---|---|---|---|---|
| Forca | 0 – 1 | 1,0 | ao vivo | escalar lido por `notaAlvo()` |
| Tolerancia | 0 – 50 ct | 15 | ao vivo | idem |
| Glide | 0 – 200 ms | 40 | ao vivo | idem |
| Look-ahead | 0 – 16 quadros | 4 | **estrutural** | redimensiona `psiRing` e a latência |
| Voz | 7 presets | Contralto | **estrutural** | muda FMIN/FMAX → grade do HMM e guarda do PSOLA |
| Escala | 7 opções | Cromática | **estrutural** | re-prepara por simplicidade |

Mudar um estrutural marca `precisaReprepare` (um `std::atomic<bool>`) e o próximo
`processBlock()` chama `aplicarParametros()` → `core.prepare()` → `setLatencySamples()`. O
próprio comentário do código admite que num plugin de produção isso iria para o thread de
mensagens com `suspendProcessing`.

### 6.2 Presets de tessitura

Copiados do *Vocal Range / Input Type* do Auto-Tune, com fronteiras na classificação vocal
padrão (Fach/SATB). Não é cosmético: **FMIN é o parâmetro que mais mexe na latência**.

| Preset | FMIN | FMAX | Faixa | Guarda PSOLA (2·fs/FMIN) |
|---|---|---|---|---|
| Baixo | 82 | 330 | E2 – E4 | 24,4 ms |
| Baritono | 98 | 392 | G2 – G4 | 20,4 ms |
| Tenor | 131 | 523 | C3 – C5 | 15,3 ms |
| **Contralto** | **175** | **698** | **F3 – F5** | **11,4 ms** |
| Mezzo | 220 | 880 | A3 – A5 | 9,1 ms |
| Soprano | 262 | 1047 | C4 – C6 | 7,7 ms |
| Instrumento | 50 | 2000 | ampla | 40,0 ms |

### 6.3 A GUI

`TunerDisplay` roda a 30 Hz e lê dois átomos publicados pelo `processBlock`: o F0 detectado e
o F0 corrigido. Desenha o nome da nota-alvo e um medidor de ±50 cents com **duas agulhas** —
"antes" e "depois". Paleta âmbar sobre grafite via um `LookAndFeel` próprio.

> **PDC não é o que parece.** `setLatencySamples()` faz o Ableton alinhar a *faixa gravada*
> com as outras. Ele **não remove o atraso que o cantor ouve no fone** ao monitorar. O que o
> cantor ouve é `driver_in + bloco_host + núcleo + driver_out`.
>
> **Correção necessária no texto do TCC:** a Tabela `tab:requisitos_atingidos` diz, sobre o
> RNF01, que a latência é *"parametrizável e compensada pelo host"*. A segunda metade dessa
> afirmação está incorreta para o caso de uso principal do trabalho (correção em tempo real
> durante a gravação, conforme o levantamento de requisitos). Reescrever.

---

## 7. O que já está medido

### 7.1 Benchmark de detectores — sinais sintéticos (9 sinais)

| Algoritmo | RPA ↑ | RCA ↑ | GPE ↓ | σ_estab (cents) ↓ | Tempo (s) ↓ | Mem (MB) ↓ |
|---|---|---|---|---|---|---|
| autocorr | 1,000 ± 0,000 | 1,000 ± 0,000 | 0,000 | 5,14 ± 7,97 | 0,008 | 0,83 |
| yin | 0,999 ± 0,004 | 1,000 ± 0,000 | 0,001 | 22,96 ± 56,76 | 0,004 | 3,58 |
| **pyin** | **1,000 ± 0,000** | 1,000 ± 0,000 | 0,000 | **3,19 ± 5,51** | 0,203 | 23,24 |
| swipe | 0,888 ± 0,212 | 0,888 ± 0,212 | 0,000 | 24,84 ± 38,83 | 0,533 | 10,88 |

### 7.2 Benchmark de detectores — Vocadito (40 gravações)

| Algoritmo | RPA ↑ | RCA ↑ | GPE ↓ | σ_estab (cents) ↓ | Tempo (s) ↓ | Mem (MB) ↓ |
|---|---|---|---|---|---|---|
| autocorr | 0,983 ± 0,019 | 0,989 | 0,010 | 332,92 ± 113,21 | 0,142 | 13,92 |
| yin | 0,977 ± 0,032 | 0,987 | 0,013 | 671,58 ± 147,23 | 0,103 | 72,21 |
| **pyin** | **0,983 ± 0,016** | 0,984 | **0,001** | **29,43 ± 6,09** | 3,150 | 82,50 |
| swipe | 0,863 ± 0,151 | 0,879 | 0,121 | 581,06 ± 147,55 | 1,266 | 23,96 |

**A leitura decisiva:** em RPA os três métodos temporais empatam (0,977–0,983). O que separa
o pYIN é a **estabilidade**: 29,4 cents contra 671,6 do YIN — ~23× melhor. Como o
deslocamento de pitch amplifica oscilações entre quadros em artefatos audíveis, essa é a
métrica que decide.

### 7.3 Verificação do núcleo de streaming (C1 vs. gold)

| Verificação | Resultado |
|---|---|
| Identidade em `forca = 0` | **bit-perfect** |
| Correlação com o gold, `forca = 1` | **0,997** |
| Correlação por região vozeada | **> 0,999** |
| Trilha de F0 vs. gold | **100 %** |
| Cliques ("pipoco") | **0** em todos os testes |
| Invariância ao block size (64/128/256/512) | **confirmada** |

O resíduo de 0,003 é jitter de fase de menos de 3 amostras (≈ 0,07 ms) por nota, inerente à
re-síntese em janela ancorada por região — não é erro acumulado.

### 7.4 Varredura de look-ahead — latência × qualidade × xRT

| look | latência | xRT | sim. offline | pipoco |
|---|---|---|---|---|
| 0 | 41,5 ms | 0,025 | 0,562 | 0 |
| 2 | 53,1 ms | 0,025 | 0,560 | 0 |
| 4 | 64,7 ms | 0,025 | 0,654 | 0 |
| **8** | 88,0 ms | 0,025 | **0,740** | 0 |
| 16 | 134 ms | 0,025 | 0,740 | 0 |
| 32 | 227 ms | 0,025 | 0,740 | 0 |

A qualidade **satura em look ≈ 8** enquanto a latência cresce linear. O xRT fica cravado em
0,025 — ~40× mais rápido que tempo real, ou seja, **sobra CPU**. A latência aqui *não* é
falta de processamento, é espera algorítmica.

### 7.5 Varredura de N_FRAME — o piso de latência vs. o grave detectável

| frame | latência | fmin detectável | sim. offline |
|---|---|---|---|
| 1024 | 41,5 ms | 86 Hz | 0,562 |
| **768** | 35,7 ms | 115 Hz | 0,483 |
| 512 | 29,9 ms | 172 Hz | 0,434 |
| 384 | 27,0 ms | 230 Hz | 0,234 |
| 256 | 24,1 ms | 345 Hz | 0,122 |
| 128 | 21,2 ms | 689 Hz | 0,122 |

De 512 para baixo a qualidade despenca — a `fmin` ultrapassa as notas efetivamente cantadas
(~177–250 Hz na gravação de teste), os graves viram "sem nota" e ficam sem correção nenhuma.

### 7.6 Varredura de tessitura — o resultado mais elegante do trabalho

| preset | FMIN | latência | lat. PSOLA | % notas perdidas | sim. offline |
|---|---|---|---|---|---|
| baixo | 82 | 41,2 ms | 12,2 ms | 0,0 % | 0,162 |
| baritono | 98 | 39,2 ms | 10,2 ms | 0,0 % | 0,184 |
| tenor | 131 | 36,7 ms | 7,6 ms | 0,0 % | 0,220 |
| **contralto** | **175** | **34,7 ms** | **5,7 ms** | **0,0 %** | **0,236** |
| mezzo | 220 | 33,6 ms | 4,5 ms | 25,9 % | 0,144 |
| soprano | 262 | 32,8 ms | 3,8 ms | 98,4 % | 0,122 |

O preset ideal é o de **maior FMIN que ainda não perde nenhuma nota**. Para essa voz, o
contralto dá ao mesmo tempo a menor latência sem perda *e* a melhor qualidade — porque
restringir a grade de pitch à faixa da voz também reduz erros de oitava do Viterbi causal.
A conclusão cabe numa frase: **a latência mínima do sistema é ditada pela nota mais grave que
se quer corrigir.**

> **Inconsistência a corrigir antes da defesa.** O CLI (`autotune_rt.cpp:225`) calcula
> `look·hop + frame + 1·fs/FMIN + block`. O plugin (`autotune_stream.h:82`) calcula
> `frame + look·hop + 2·fs/FMIN`, *sem* o bloco do host. São fórmulas diferentes — por
> coincidência numérica, com bloco de 256 e FMIN de 175 Hz, um período extra (252 am) ≈ um
> bloco (256 am), e os números batem. **Padronizar as duas**, ou a banca vai perguntar.

---

## 8. Diagnóstico dos problemas

Os dois sintomas relatados no teste de usuário — 58 ms de delay e som duro, "sem cor" — não
são o mesmo problema, mas compartilham uma causa de fundo: **o sistema decide o pitch em
quadros, e a voz não é feita de quadros.**

### 8.1 O delay: de onde vêm os 57,9 ms

```
                 0        10       20       30       40       50    ms
                 ├────────┼────────┼────────┼────────┼────────┼──►
hoje (fábrica)   ████████████████████░░░░░░░░░░░░░░░░░▓▓▓▓▓▓▓▓     57,9 ms
look = 0         ████████████████████▓▓▓▓▓▓▓▓                      34,6 ms
+ frame 512, 1T  ██████████▓▓▓▓                                    17,3 ms
piso físico      ┄┄┄┄┄┄┄┄┄┄                                        11,4 ms

  ████ quadro de análise    ░░░░ look-ahead do Viterbi    ▓▓▓▓ guarda do PSOLA
```

**Dois terços dos 58 ms são compressíveis com o algoritmo que já existe.**

#### O que é compressível

| Termo | Hoje | Possível | Economia | Razão |
|---|---|---|---|---|
| `look · hop` | 4×256 = 23,2 ms | 0 | **−23,2 ms** | parâmetro já exposto no plugin |
| `frame` | 1024 = 23,2 ms | 512 = 11,6 ms | **−11,6 ms** | `frame ≈ 2·fs/FMIN`; contralto → 512 basta |
| guarda PSOLA | 2·fs/FMIN = 11,4 ms | 1·fs/FMIN = 5,7 ms | **−5,7 ms** | estimar a largura do último grão por `f0samp`, não pela marca vizinha |

#### O que é físico

Para o CMNDF medir um período τ, a janela precisa conter pelo menos τ — e no código
`tauMax = W = frame/2`, então o quadro precisa de `2τ`. Para a nota mais grave de um contralto
(F3 ≈ 175 Hz, τ = 5,7 ms), isso são **11,4 ms de sinal antes de qualquer resposta**. Nenhum
ajuste de parâmetro escapa disso — só uma arquitetura diferente de detecção.

#### A pergunta do Auto-Tune

Se a Antares entrega latência de poucos milissegundos, ela não pode estar reanalisando uma
janela de 2 períodos do zero a cada quadro. Hipóteses arquiteturais a investigar:

1. **Rastreio contínuo em vez de detecção por quadro.** Uma vez travado num período,
   atualizar a estimativa custa *um* período novo, não dois; a janela cheia só é necessária no
   *lock-in* inicial de cada nota.
2. **Ressíntese sem look-ahead de grão.** Um pitch-shifter por linha de atraso modulada emite
   saída imediatamente a partir do que já entrou, sem precisar posicionar o próximo grão.
3. **Aceitar transiente de entrada.** Talvez o Auto-Tune simplesmente erre os primeiros
   milissegundos de cada nota e ninguém perceba, porque o ataque é ruidoso.

> **Primeiro passo concreto:** medir a latência real do Auto-Tune por *loopback* (sinal de
> clique → gravar entrada e saída na mesma sessão → correlacionar) em vez de citar o número do
> manual. Uma medição própria vale muito mais na banca do que uma citação — e é material
> direto para o capítulo de discussão.

### 8.2 A cor: por que soa duro e estático

A hipótese do usuário — *"ele bate direto na nota"* — está correta, mas o mecanismo é mais
rico. São **quatro quantizações empilhadas**, e o `glide` só ataca uma delas.

```
F0 real da voz     ╭─╮   ╭─╮   ╭─╮   ╭─╮      contínua: desliza, respira, vibra
                  ╱   ╲ ╱   ╲ ╱   ╲ ╱   ╲
F0 detectada      ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐     escada: 20 cents × 5,8 ms
                  └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
nota-alvo         ─────────────────────────    reta: 12-TET
```

#### Quantização 1 — a grade de pitch de 20 cents
`dsp.h:29 · RES_CENTS = 20`

O Viterbi só pode emitir valores da grade. **Toda** a expressividade que sobrevive à zona
morta já chega quantizada em degraus de 20 cents — e 20 cents é audível (um quinto de
semitom). Pior: a zona morta `tol = 15` é *menor* que o passo da grade, então a
"micro-afinação preservada" tem resolução pior que a própria tolerância que a preserva.

#### Quantização 2 — o degrau de um hop
`autotune_stream.h:426 · emitirAmostras()`

Cada quadro emitido "carimba" `nHop = 256` amostras com *o mesmo* F0. A trajetória de pitch
da saída é uma escada com degraus de 5,8 ms — nunca uma curva. O β do PSOLA salta de patamar
em patamar. O glide de 1 polo *parcialmente* esconde isso, mas ele filtra o *alvo*, não o
*detectado*.

#### Quantização 3 — o reset do glide no ataque
`autotune_stream.h:434 · tinhaNota`

```cpp
glideEstado = tinhaNota ? (alpha*glideEstado + (1.0-alpha)*alvoCents) : alvoCents;
```

Em toda entrada de nota depois de um trecho não-vozeado — ou seja, depois de cada respiração
— a saída **nasce exatamente na nota**, sem nenhum caminho até ela. É literalmente o "bate
direto na nota" que o usuário ouviu. O reset existe por um bom motivo (não deslizar a partir
do silêncio), mas um cantor real chega na nota por baixo em ~40–80 ms — é exatamente esse
gesto que está sendo apagado.

#### Quantização 4 — zona morta ≠ velocidade de retune
`dsp.h:159 · notaAlvo()`

A `tol` é uma zona morta *em amplitude*: "desvios menores que X cents não são corrigidos".
O *Retune Speed* do Auto-Tune é conceitualmente diferente — um limite *em taxa*: "a correção
não pode se mover mais rápido que Y cents por milissegundo".

A diferença é enorme musicalmente:

| | Zona morta (implementada) | Limite de taxa (Retune Speed) |
|---|---|---|
| Vibrato lento (±40 ct, 1 Hz) | achatado (fora da zona) | achatado (a correção acompanha) |
| Vibrato rápido (±40 ct, 6 Hz) | **achatado** (fora da zona) | **preservado** (a correção não acompanha) |
| Desafinação constante (+30 ct) | corrigida | corrigida |
| Ataque com scoop | **apagado** | **preservado** |

**Uma zona morta não consegue distinguir expressão de erro; um limite de taxa consegue** —
porque expressão vocal é rápida e desafinação é lenta. Essa distinção sozinha pode ser um
capítulo do TCC 2, e o filtro de 1 polo já implementado é 80% da infraestrutura necessária,
só está aplicado no lugar errado da cadeia.

#### Duas ausências, além das quantizações

- **Não existe mistura seco/molhado.** Nem no CLI nem no plugin. A `forca` mistura
  *afinação*, não *sinal*.
- **Não existe processamento de timbre.** O TD-PSOLA preserva formantes por construção — o
  que é ótimo — mas isso também significa que *nada* na cadeia adiciona caráter. O que o
  Auto-Tune chama de "cor" (Throat Length, Formant, Humanize) são processos *adicionais*.
  O protótipo é um **corretor**, não um **colorizador**, e isso é uma escolha de escopo
  defensável — mas precisa estar explícita no texto.

### 8.3 Três achados de código

Análise estática — encontrados lendo o código, **não medidos**. Cada um tem um teste
associado, e todos são candidatos a estarem piorando a sensação ao vivo.

#### Achado 1 · janela de re-síntese que cresce sem parar

Em `autotune_stream.h:481`, `winStart` recua enquanto `f0samp[winStart−1] > 0` — isto é,
**até o início da região vozeada**. Numa nota sustentada ou numa frase legato de 8 segundos
sem quadro não-vozeado, a janela re-sintetizada passa a ter 8 segundos, e `psolaSintetiza()`
roda sobre ela *inteira, a cada bloco* — com a busca de correlação por marca dentro. O custo
por bloco cresce linearmente com o tempo desde a última respiração; o custo total da frase é
**quadrático**.

**Por que importa:** o xRT de 0,025 foi medido no caminho B (lote), que não tem esse laço. Se
a CPU sobe durante notas longas, o host pode estar gerando *dropouts* — que se percebem como
instabilidade, não como delay, mas contaminam a impressão geral.

**Teste:** instrumentar o tempo de `process()` por bloco e plotar contra a duração da nota,
com uma nota sustentada de 10 s. Se a curva subir, está confirmado.

#### Achado 2 · alocação dentro do callback de áudio

`xAll`, `f0samp`, `foutSamp` e `outBuf` crescem por `push_back`/`resize` a **cada amostra
processada** (`autotune_stream.h:249, 428, 492`). Todo `push_back` que estoura a capacidade
faz `malloc` + cópia — **dentro do `processBlock()`**, que é onde a regra de ouro do áudio em
tempo real proíbe alocar. Além disso, a memória cresce indefinidamente enquanto a sessão
estiver aberta. O próprio comentário do código já previa isto: *"C2 trocará por anel
limitado"* — a troca não foi feita.

**Teste:** rodar o Standalone por 10 minutos observando a memória do processo; e contar
`capacity()` antes/depois de blocos.

#### Achado 3 · normalização de pico por janela

`dsp.h:305` divide toda a saída por `pico` quando `pico > 1`. No lote isso é um ganho global,
inofensivo. **No streaming, `psolaSintetiza()` é chamada uma vez por janela** — então janelas
diferentes podem receber ganhos diferentes, e o miolo cometido de cada uma carrega o ganho da
sua janela. Com voz forte (pico > 1), isso vira **degraus de amplitude** nas fronteiras de
commit: audíveis como "bombeamento" ou aspereza — exatamente o tipo de coisa que se descreve
como "duro".

**Teste:** processar um trecho com pico > 1 pelo `stream_test.exe` e pelo gold e comparar as
**envoltórias**, não só a correlação (a correlação por região é insensível a ganho).

---

## 9. Soluções possíveis

Alternativas de solução para cada problema diagnosticado. Cada item traz o mecanismo, o
ganho esperado, o custo, o risco e **como verificar**. Nem todos precisam ser implementados —
a priorização está na Seção 10.

### 9.1 Latência

#### L1 · Look-ahead zero por padrão

**Mecanismo.** O termo `look · hop` some inteiro. O parâmetro já existe e já é exposto na
GUI; trata-se de mudar o valor de fábrica em `PluginProcessor.cpp`:

```cpp
// hoje
ParameterID{ ids::look, 1 }, "Look-ahead (quadros)", 0, 16, 4
// proposto
ParameterID{ ids::look, 1 }, "Look-ahead (quadros)", 0, 16, 0
```

**Ganho:** −23,2 ms (57,9 → 34,7 ms). **Custo:** nenhum. **Risco:** o Viterbi vira guloso;
a similaridade com o offline cai de 0,654 para 0,562 na varredura da §7.4.

> **Ressalva metodológica importante.** *Similaridade com o offline não é qualidade
> percebida.* Ela mede fidelidade a uma referência não-causal que o cantor nunca vai ouvir.
> Para monitoração ao vivo, um erro de oitava ocasional pode ser preferível a 23 ms de
> atraso — e essa é uma pergunta **perceptual**, não numérica. **Isso precisa virar um teste
> de escuta no TCC 2**, e o resultado é material de discussão qualquer que seja.

**Mitigação do risco.** A varredura de tessitura (§7.6) mostra que restringir a grade de
pitch à faixa da voz **já reduz erros de oitava do Viterbi causal** — o preset correto
compensa parte da perda do look-ahead.

**Verificação:** `bench_latencia.py` com `look=0` + teste de escuta A/B contra `look=4`.

---

#### L2 · Quadro de análise derivado do preset de tessitura

**Mecanismo.** Hoje `nFrame = 1024` é constante, independentemente da voz selecionada. Mas o
requisito real é `frame ≥ 2·fs/FMIN` (a janela do CMNDF precisa conter um período completo do
tom mais grave). Para contralto (FMIN = 175 Hz), `2·44100/175 = 504` — ou seja, **1024 é o
dobro do necessário**.

```cpp
// em prepare(), depois de fixar FMIN:
int minFrame = (int)std::ceil(2.0 * fs / FMIN);
p.nFrame = ((minFrame + p.nHop - 1) / p.nHop) * p.nHop;   // arredonda p/ múltiplo do hop
```

| Preset | FMIN | `2·fs/FMIN` | frame proposto | frame hoje | economia |
|---|---|---|---|---|---|
| Soprano | 262 | 337 | 512 | 1024 | −11,6 ms |
| Mezzo | 220 | 401 | 512 | 1024 | −11,6 ms |
| **Contralto** | **175** | **504** | **512** | **1024** | **−11,6 ms** |
| Tenor | 131 | 673 | 768 | 1024 | −5,8 ms |
| Baritono | 98 | 900 | 1024 | 1024 | 0 |
| Baixo | 82 | 1076 | 1280 | 1024 | +5,8 ms (corrige um bug latente) |

> **Efeito colateral positivo:** hoje, com `frame = 1024`, a menor frequência detectável é
> 86 Hz — **acima do FMIN do preset Baixo (82 Hz)**. O preset Baixo, portanto, promete uma
> faixa que o detector não alcança. A fórmula corrige isso.

**Ganho:** −11,6 ms para as três tessituras agudas. **Custo:** nenhum (menos CPU, inclusive:
o CMNDF é O(frame²/4)). **Risco:** baixo, mas a margem de segurança precisa ser validada —
usar exatamente `2·fs/FMIN` deixa o período de FMIN no limite de `tauMax`.

**Verificação:** `bench_nframe.py` por preset; confirmar que `% notas perdidas` continua 0.

---

#### L3 · Guarda do PSOLA de 2 para 1 período

**Mecanismo.** Os 2 períodos existem porque o grão da última marca da janela tem largura
estimada pela marca *anterior* (`Tana = marcas[ia] − marcas[ia−1]`), já que a próxima ainda
não chegou. Mas a largura correta é conhecida: `f0samp` já está decidido até a fronteira de
decisão.

```cpp
// dsp.h, no cálculo de Tana — hoje:
else if (ia - 1 >= (int)i0) Tana = marcas[ia] - marcas[ia - 1];
// proposto: usar o F0 já decidido nessa posição
else if (f0samp[a] > 0)     Tana = (long long)std::llround(fs / f0samp[a]);
```

Com o grão final estável, `psolaGuard` pode cair para `1 * fs/FMIN`.

**Ganho:** −5,7 ms. **Custo:** nenhum. **Risco:** médio — mexe no núcleo compartilhado.
**Mitigação:** a suíte de regressão cobre exatamente isso.

**Verificação obrigatória:** `bench_stream.py` (correlação com o gold ≥ 0,995 e identidade
bit-perfect em `forca = 0`) + contagem de pipoco = 0.

---

#### L4 · Hop menor (256 → 128)

**Mecanismo.** Com `look = 0` o hop não entra na latência, mas dobra a **taxa de atualização
do pitch** — o que ataca diretamente a Quantização 2 da §8.2 (degraus de 5,8 → 2,9 ms).

**Ganho:** indireto (naturalidade); com `look > 0`, reduz esse termo pela metade.
**Custo:** 2× chamadas de CMNDF → xRT de 0,025 para ~0,05. Ainda ~20× mais rápido que tempo
real. **Risco:** baixo.

---

#### L5 · Preset "Low Latency" na GUI

**Mecanismo.** Um botão que aplica de uma vez `look = 0`, `frame` mínimo pela tessitura e
guarda de 1 período — espelhando o *Low Latency Mode* do Auto-Tune.

**Valor para o TCC:** transforma o trade-off latência × qualidade, que é **o resultado
central do trabalho**, em algo que o usuário opera e a banca vê funcionando. O trabalho
deixa de "ter 58 ms" e passa a "expor um trade-off parametrizado, com os dois extremos
medidos".

---

#### L6 · Rastreio contínuo de período *(pesquisa — ataca o piso físico)*

**Mecanismo.** Hoje cada quadro roda um CMNDF completo sobre uma janela de 2 períodos, do
zero. Proposta: depois do *lock-in* de uma nota, atualizar a estimativa buscando **apenas o
próximo período** numa vizinhança estreita da estimativa anterior (±5%, digamos) — que é
exatamente o que o encadeamento de marcas do PSOLA já faz em `psolaSintetiza()`.

Em regime permanente, o requisito de janela cai de `2τ` para `~1τ` → o piso de detecção cai
de 11,4 para ~5,7 ms para um contralto. A janela cheia só é necessária no ataque de cada
nota.

**Ganho potencial:** até −5,7 ms **e** menos CPU. **Custo:** alto — é um detector novo.
**Risco:** alto. Precisa de estratégia de *re-lock* quando a nota muda, e de convivência com
o HMM (que hoje decide vozeamento).
**Recomendação:** só se as Sprints 12–13 fecharem cedo. É a frente mais interessante
academicamente e a mais arriscada em cronograma.

---

#### L7 · Medição da latência real (método, não redução)

Três medições que faltam e que o texto do TCC precisa:

1. **Round-trip do protótipo:** clique gravado → passado pela cadeia completa (driver + host
   + plugin) → correlação cruzada entrada/saída. Dá o número que o cantor de fato ouve.
2. **Round-trip da cadeia sem o plugin:** isola driver + buffer do host, permitindo reportar
   a contribuição do plugin isoladamente.
3. **Round-trip do Auto-Tune** na mesma cadeia e mesmo *buffer size*. Substitui o "~3 ms"
   relatado por um número medido.

**Custo:** baixo. **Valor:** alto — é o que fecha a comparação com o estado da arte de forma
defensável.

---

#### Resumo da latência

| Cenário | frame | look·hop | guarda | total |
|---|---|---|---|---|
| Hoje (fábrica) | 23,2 ms | 23,2 ms | 11,4 ms | **57,9 ms** |
| L1 | 23,2 ms | 0 | 11,4 ms | **34,6 ms** |
| L1 + L2 | 11,6 ms | 0 | 11,4 ms | **23,0 ms** |
| L1 + L2 + L3 | 11,6 ms | 0 | 5,7 ms | **17,3 ms** |
| + L6 (pesquisa) | 5,8 ms | 0 | 5,7 ms | **11,5 ms** |
| Piso físico (detecção de F3 por CMNDF) | — | — | — | **11,4 ms** |

**L1 + L2 + L3 levam o protótipo de 57,9 para 17,3 ms — abaixo do teto de 20–30 ms declarado
no próprio trabalho — sem trocar uma linha de arquitetura.**

---

### 9.2 Cor / naturalidade

#### C1 · Retune speed: filtrar a *correção*, não o *alvo* ⭐

**Esta é a mudança mais importante desta seção.** Ela resolve, com uma única alteração, tanto
o "bate direto na nota" (Quantização 3) quanto o achatamento do vibrato (Quantização 4).

**O problema.** Hoje o filtro de glide suaviza a **posição alvo absoluta**:

```cpp
estado = tinhaNota ? α·estado + (1−α)·alvoCents : alvoCents;
fout   = FMIN · 2^(estado/1200);
```

Como `alvoCents` é constante dentro de uma nota, o filtro converge para o alvo e **fica lá** —
a saída vira uma reta, independentemente do que a voz faz. O vibrato do cantor é descartado.

**A proposta.** Filtrar a **quantidade de correção** e somá-la à afinação real:

```cpp
double realCents  = 1200.0*std::log2(f0q/FMIN);
double alvoCents  = 1200.0*std::log2(notaAlvo(f0q, 1.0, tolCents)/FMIN);
double corrAlvo   = alvoCents - realCents;        // quanto falta corrigir AGORA

// filtro de 1 polo sobre a CORREÇÃO (não sobre o alvo)
corrEstado = tinhaNota ? (α*corrEstado + (1−α)*corrAlvo) : 0.0;   // ← ataque: correção 0

double saidaCents = realCents + forca * corrEstado;
foutSamp.push_back((float)(FMIN*std::pow(2.0, saidaCents/1200.0)));
```

**Por que isso muda tudo.** A saída passa a **seguir a afinação real** somada a um
deslocamento que varia lentamente:

| Situação | Comportamento resultante |
|---|---|
| Vibrato rápido (6 Hz) | `corrAlvo` oscila rápido, o filtro não acompanha, `corrEstado` fica ~constante → **o vibrato passa intacto para a saída** |
| Desafinação constante (+30 ct) | `corrAlvo` é constante, o filtro converge → **corrigida integralmente** |
| Ataque de nota | `corrEstado = 0` → a saída **começa na afinação real do cantor** e desliza até a nota em `glide` ms → **o scoop é preservado** |
| `glide = 0` (α = 0) | `corrEstado = corrAlvo` → **comportamento atual, idêntico** (retrocompatível) |

**É a definição funcional do *Retune Speed*:** a correção tem uma velocidade máxima, e o que
for mais rápido que ela passa como expressão.

**Ganho:** alto — ataca a raiz do "robótico". **Custo:** ~10 linhas. **Risco:** baixo;
retrocompatível em `glide = 0`, o que preserva todos os testes de regressão existentes.

**Verificação:** processar um trecho com vibrato e plotar F0 de entrada e de saída
sobrepostos. Hoje a saída é reta; com C1, deve acompanhar a ondulação da entrada deslocada.
O `synth.gen_vibrato()` do repositório Python já gera o sinal de teste ideal, com ground
truth.

> **Nota de nomenclatura para o TCC:** com essa mudança, o parâmetro `glide` passa a ser
> conceitualmente um **Retune Speed**, e vale renomeá-lo na GUI. Isso alinha o vocabulário do
> protótipo ao das ferramentas comerciais e facilita a comparação no capítulo de discussão.

---

#### C2 · Resolução de pitch: decodificação híbrida

**O problema.** `RES_CENTS = 20` — a saída do Viterbi só assume valores da grade, e 20 cents
é audível.

**Opção A — reduzir `RES_CENTS` para 5.**

Cuidado com a armadilha: `W_TRANS = 12` e `SIGMA_TRANS = 2` estão em **bins**, não em cents.
Reduzir a resolução sem reescalar transforma a gaussiana de transição de 40 cents em 10 cents
e a janela de ±240 para ±60 cents — **um modelo probabilístico diferente**, que impediria
saltos legítimos de nota.

```cpp
static const double RES_CENTS   = 5.0;
static const int    W_TRANS     = 48;   // 48 × 5 = ±240 cents (igual a hoje)
static const double SIGMA_TRANS = 8.0;  // 8 × 5 = 40 cents (igual a hoje)
```

**Custo real** (contralto, faixa de 2395 cents):

| | bins | custo Viterbi/quadro | custo CMNDF/quadro | total |
|---|---|---|---|---|
| RES = 20 | 120 | 120 × 25 = 3.000 | ~262.000 | ~265.000 |
| RES = 5 | 480 | 480 × 97 = 46.560 | ~262.000 | ~309.000 |

**+17% de CPU**, não 15× — porque o CMNDF domina o custo por quadro, não o Viterbi. Com xRT
de 0,025, isso vai para ~0,029. **Perfeitamente viável.**

**Opção B — refinar dentro do bin (recomendada).**

Melhor ainda: manter a grade de 20 cents para o HMM (robustez e custo) e **refinar o valor
emitido** usando os candidatos contínuos que o `candidato()` já produz — ele devolve F0
interpolado parabolicamente, e essa precisão é jogada fora ao converter para bin.

```cpp
// em passoPitch(), guardar também a média ponderada dos candidatos por bin:
for (int k = 0; k < K; ++k) {
    double f = candidato(dp, tauMin, tauMax, sLim[k], fs);
    if (f > 0) { int b = binDe(f);
        if (b >= 0 && b < nBins) { obs[b] += wLim[k];
                                   somaF[b] += wLim[k]*f;   // ← novo
                                   massa += wLim[k]; } }
}
// na emissão, em vez de fDeBin(s):
double f0q = (s == UV) ? 0.0
           : (obs[s] > EPS ? somaF[s]/obs[s] : fDeBin(s));
```

**Ganho:** resolução contínua. **Custo:** um vetor a mais e uma divisão por quadro —
essencialmente zero. **Risco:** baixo. O Viterbi continua decidindo *qual* bin (robustez
contra erro de oitava); o valor emitido deixa de ser o centro do bin.

**Verificação:** medir σ de estabilidade e a resolução efetiva num sinal de vibrato
sintético de amplitude conhecida (±50 cents). Hoje a saída quantiza em degraus de 20 ct.

---

#### C3 · Interpolação de F0 entre quadros

**O problema.** `emitirAmostras()` carimba `nHop = 256` amostras com o mesmo F0 → escada de
5,8 ms na trajetória → β do PSOLA salta de patamar em patamar.

**A proposta.** Interpolar linearmente **em cents** entre o quadro anterior e o atual. Não
custa latência: ambos os quadros já foram decididos.

```cpp
void emitirAmostras(double f0q) {
    bool ambosVozeados = (f0q > 0 && f0Anterior > 0);
    double c0 = ambosVozeados ? 1200.0*std::log2(f0Anterior/FMIN) : 0.0;
    double c1 = ambosVozeados ? 1200.0*std::log2(f0q/FMIN)        : 0.0;
    for (int k = 0; k < p.nHop; ++k) {
        double f = f0q;
        if (ambosVozeados) {                          // não interpola através de
            double t = (k + 0.5) / p.nHop;            // fronteiras de vozeamento
            f = FMIN*std::pow(2.0, (c0 + t*(c1 - c0))/1200.0);
        }
        f0samp.push_back((float)f);
        /* ... resto igual ... */
    }
    f0Anterior = f0q;
}
```

**Ganho:** trajetória contínua em vez de escada. **Custo:** um `pow` por amostra (ou uma
multiplicação incremental, se otimizado). **Risco:** baixo.

> **Atenção:** isso quebra a identidade bit-perfect com o gold do caminho B, que também usa
> a escada. Para manter a suíte de regressão válida, aplicar a mesma mudança em
> `autotune_rt.cpp` (passo 3) — os dois compartilham a mesma lógica por design.

---

#### C4 · Mistura seco/molhado

**Mecanismo.** Um parâmetro `Mix` 0–100% que interpola entre a entrada atrasada da latência e
a saída processada. Precisa da entrada **alinhada** com a saída, o que o motor já tem
(`xAll` indexado por amostra absoluta).

**Ganho:** o recurso mais usado por engenheiros para devolver naturalidade — e responde
diretamente à necessidade "controle de naturalidade" levantada com o usuário na proposta.
**Custo:** trivial. **Risco:** nenhum.

---

#### C5 · Reconhecer o escopo no texto

Nem tudo o que o usuário chamou de "falta de cor" é defeito. Parte é **ausência de processos
de timbre**, que nunca estiveram no escopo declarado. O texto do TCC deve separar
explicitamente:

- **Correção de afinação** (escopo do trabalho) — pYIN + TD-PSOLA, formantes preservados.
- **Coloração tímbrica** (fora do escopo) — Throat Length, Formant Shift, Humanize.

Registrar essa fronteira é honesto, protege o trabalho na banca e ainda rende uma seção de
trabalhos futuros.

---

### 9.3 Correções de engenharia

#### A1 · Limitar a janela de re-síntese

**Problema:** §8.3, Achado 1 — custo quadrático por frase.

**Solução.** Persistir a **última marca de análise cometida** entre chamadas e retomar o
encadeamento a partir dela, em vez de recuar até o início da região vozeada. A âncora de fase
passa a ser mantida como estado, não redescoberta.

```
estado adicional:  long long ultimaMarcaCometida;
avancarPsola():    winStart = max(synthFront − margem, ultimaMarcaCometida − K·T);
```

**Alternativa mais simples (mitigação, não solução):** limitar o recuo a um teto fixo
(`winStart ≥ synthFront − N·fs/FMIN`, com N ~ 8) e aceitar uma pequena divergência de fase em
notas muito longas — a ser medida.

**Ganho:** custo por bloco constante. **Custo:** médio. **Risco:** médio — mexe na âncora de
fase, que é justamente o que fez o streaming casar com o lote.
**Verificação obrigatória:** `bench_stream.py` + o novo teste de CPU × duração da nota.

---

#### A2 · Buffers de tamanho fixo, alocados em `prepare()`

**Problema:** §8.3, Achado 2 — alocação dentro do callback e memória sem limite.

**Solução.** Trocar `xAll`, `f0samp`, `foutSamp` e `outBuf` por **ring buffers
pré-alocados** em `prepare()`, dimensionados pelo maior histórico de que o PSOLA precisa
(que, depois de A1, passa a ser limitado). Índices absolutos continuam existindo; só o acesso
passa a ser `buf[i % cap]`.

**Ganho:** conformidade RT, memória constante. **Custo:** médio (mexe em todos os acessos).
**Risco:** baixo se A1 vier antes — é A1 que torna o tamanho limitado conhecido.
**Ordem:** A1 → A2.

---

#### A3 · Normalização de pico consistente

**Problema:** §8.3, Achado 3 — ganhos diferentes por janela no streaming.

**Três opções:**

1. **Remover a normalização de `psolaSintetiza()`** e deixar o *clipping* para o host (mais
   correto em contexto de plugin: normalizar dentro do processador é comportamento
   surpreendente).
2. **Mover para fora**, aplicando um limitador com estado contínuo entre blocos.
3. **Manter, mas com ganho suavizado**, guardado como estado entre janelas.

**Recomendação:** opção 1 para o plugin, mantendo a normalização apenas no caminho offline
(onde grava arquivo).

---

#### A4 · Re-`prepare()` fora do callback

**Problema:** `aplicarParametros()` → `core.prepare()` aloca dentro do `processBlock()`
quando um parâmetro estrutural muda.

**Solução (a do próprio comentário do código):** fazer no thread de mensagens com
`suspendProcessing(true)`.

**Custo:** baixo. **Risco:** baixo. **Valor:** mais para a robustez e para o texto (boas
práticas de áudio em tempo real) do que para a percepção.

---

### 9.4 Métricas que faltam

O teste de usuário expôs uma lacuna metodológica: **as métricas atuais medem fidelidade a uma
referência, não qualidade percebida.** Um sistema pode reproduzir perfeitamente seu próprio
gold e ainda assim soar mal — foi exatamente o que aconteceu.

Propostas para o TCC 2:

| Métrica | O que mede | Como |
|---|---|---|
| **Teste de escuta A/B/X** | preferência perceptual | seco × protótipo × Auto-Tune, cego, com N ouvintes |
| **MOS de naturalidade** | naturalidade percebida | escala 1–5, por trecho |
| **MOS de incômodo de latência** | usabilidade ao vivo | escala 1–5, por configuração de latência |
| **Preservação de vibrato** | quanto da modulação sobrevive | razão entre a amplitude de modulação de F0 na saída e na entrada, na banda 4–8 Hz |
| **Desvio residual de afinação** | eficácia da correção | mediana de \|F0 − nota mais próxima\| na saída, em cents |
| **CPU × duração da nota** | achado 1 | tempo de `process()` por bloco vs. tempo desde o último quadro não-vozeado |
| **Round-trip medido** | latência real | loopback (L7) |

As duas últimas linhas são diagnósticas; as cinco primeiras são resultado de TCC.

> A **preservação de vibrato** merece destaque: ela transforma "soa mais natural" em um
> número. Com C1 implementado, espera-se razão próxima de 1,0 na banda 4–8 Hz (vibrato
> passa) e próxima de 0 abaixo de 1 Hz (desafinação corrigida) — **uma resposta em frequência
> da correção**. Esse gráfico sozinho justificaria a seção de resultados do TCC 2.

---

## 10. Backlog priorizado e plano por sprint

Entrega da Sprint 11. Ordenado por (valor ÷ esforço), respeitando dependências.

| # | Item | Alvo | Ganho | Esforço | Risco |
|---|---|---|---|---|---|
| 1 | Confirmar achados 1–3 por medição | diagnóstico | desbloqueia o resto | baixo | nenhum |
| 2 | **L7** — medir round-trip (protótipo, cadeia, Auto-Tune) | método | baseline defensável | baixo | nenhum |
| 3 | **L1** — `look = 0` padrão | latência | −23,2 ms | trivial | qualidade a validar |
| 4 | **C1** — retune speed (filtrar a correção) ⭐ | cor | ataque + vibrato | baixo | baixo |
| 5 | **L2** — `frame` derivado da tessitura | latência | −11,6 ms | baixo | baixo |
| 6 | **C2-B** — refino dentro do bin | cor | resolução contínua | baixo | baixo |
| 7 | **C3** — interpolação de F0 entre quadros | cor | tira a escada de 5,8 ms | baixo | baixo |
| 8 | **C4** — mix seco/molhado | cor | naturalidade sob demanda | trivial | nenhum |
| 9 | **L3** — guarda do PSOLA 2T → 1T | latência | −5,7 ms | médio | regressão cobre |
| 10 | **A1** — limitar a janela de re-síntese | robustez | CPU constante | médio | médio |
| 11 | **A2** — ring buffers pré-alocados | robustez | RT-safe, memória fixa | médio | baixo (após A1) |
| 12 | **A3** — normalização consistente | robustez | tira degraus de amplitude | baixo | baixo |
| 13 | **L5** — preset "Low Latency" | usabilidade | expõe o trade-off central | baixo | nenhum |
| 14 | **9.4** — métricas perceptuais | método | resultado do TCC 2 | médio | nenhum |
| 15 | **A4** — re-prepare fora do callback | robustez | boas práticas | baixo | baixo |
| 16 | Detecção automática de tonalidade | funcionalidade | já prevista no TCC 1 | médio | baixo |
| 17 | **L6** — rastreio contínuo de período | pesquisa | −5,7 ms e menos CPU | alto | alto |
| 18 | **L4** — hop 256 → 128 | cor | dobra a taxa de pitch | trivial | baixo |

### Encaixe no cronograma existente

O plano **não exige alterar o cronograma do TCC 1** — os itens se encaixam nos sprints já
declarados.

| Sprint | Período | Foco declarado no TCC 1 | Itens deste backlog |
|---|---|---|---|
| **11** | 24/08 – 06/09 | Consolidação do feedback | Estes dois documentos · itens **1, 2** |
| **12** | 07/09 – 20/09 | Refinamento a partir do feedback (naturalidade, usabilidade, parâmetros de correção) | **3, 4, 5, 6, 7, 8** |
| **13** | 21/09 – 04/10 | Refinamento e correções · nova rodada de testes | **9, 10, 11, 12** + 2º teste de usuário |
| **14–15** | 05/10 – 01/11 | Funcionalidades adicionais · interface | **13, 16** (+ **17** se houver folga) |
| **16** | 02/11 – 15/11 | Empacotamento e robustez | **15** + build final VST3 |
| **17** | 16/11 – 29/11 | Análise final dos resultados | **14** — consolidar as métricas perceptuais |
| **18–19** | 30/11 – 21/12 | Redação, revisão e defesa | — |

### Meta quantificada para o TCC 2

| Requisito | Hoje | Meta | Como se verifica |
|---|---|---|---|
| **RNF01** (baixa latência) | 57,9 ms · *Parcial* | **≤ 20 ms** algorítmica, com round-trip medido | L1+L2+L3 → 17,3 ms · L7 |
| **RNF03** (naturalidade) | *Parcial*, reprovado na escuta | vibrato preservado (razão ≥ 0,8 em 4–8 Hz) e preferência em teste cego | C1+C2+C3 · métricas da §9.4 |
| **RNF05** (simplicidade) | *Parcial* | preset "Low Latency" + Mix; usabilidade avaliada | L5, C4 · 2º teste de usuário |

### Correções pendentes no texto do TCC 1

Levantadas durante esta análise:

1. **Tabela `tab:requisitos_atingidos`, RNF01** — remover ou qualificar "compensada pelo
   host": o PDC não compensa monitoração ao vivo (§6.3).
2. **Padronizar a fórmula de latência** entre o CLI e o plugin (§7.6).
3. **Citar a fonte** do teto de 20–30 ms de latência tolerável — hoje é apenas um comentário
   de código (§3.4).
4. **Preset Baixo (FMIN = 82 Hz)** promete uma faixa abaixo do que o detector alcança com
   `frame = 1024` (86 Hz) — corrigir no texto ou no código (§9.1, L2).
5. **Versionar os CSVs de resultado** do repositório Python (§1).

---

## 11. Glossário técnico

### Sinal e afinação

**F0 (frequência fundamental)** — A frequência de repetição do sinal, percebida como "a
altura da nota". Não confundir com o primeiro harmônico, que pode estar ausente do espectro
sem que o F0 deixe de ser percebido.

**Período (T = fs/F0)** — Quantas amostras dura um ciclo. Em 175 Hz a 44,1 kHz: 252 amostras
= 5,7 ms. É a unidade natural do PSOLA e o que fixa o piso de latência.

**Cent** — 1/100 de semitom, 1/1200 de oitava. `cents = 1200·log₂(f₂/f₁)`. Escala
logarítmica porque a percepção de intervalo é logarítmica. ~5 cents é o limiar de detecção
para a maioria das pessoas; 20 cents é claramente audível.

**12-TET (temperamento igual)** — Divisão da oitava em 12 semitons de 100 cents.
`midi = 69 + 12·log₂(f/440)`, com A4 = 440 Hz = MIDI 69.

**Vozeado / não-vozeado** — Vozeado = as pregas vocais vibram → sinal periódico → tem F0
(vogais, /m/, /n/). Não-vozeado = turbulência sem periodicidade (/s/, /f/, /ʃ/, respiração).
No HMM, um estado dedicado (`UV`).

**Formantes** — Picos de ressonância do trato vocal (F1, F2, F3…), fixos em frequência
independentemente do F0. **São eles que definem a vogal e a identidade da voz.** Deslocá-los
junto com o pitch produz o efeito "esquilo".

**Envelope espectral** — Curva suave que "veste" os harmônicos no espectro; seus picos são os
formantes. Estimado no `formantes.py` por **liftering cepstral** — cortar as quefrências
altas do cepstro para separar o filtro (trato) da fonte (pregas).

**Tessitura (vocal range / Fach)** — Faixa de notas confortável de um cantor. Classificação
SATB: Baixo E2–E4, Barítono G2–G4, Tenor C3–C5, Contralto F3–F5, Mezzo A3–A5, Soprano C4–C6.
No protótipo, define `[FMIN, FMAX]` — e portanto a latência.

### Detecção de pitch

**Autocorrelação (ACF)** — Correlação do sinal com uma cópia atrasada de si mesmo. Pico no
atraso = período. Simples, mas tende a errar oitavas porque 2T também correlaciona bem.

**CMNDF** — *Cumulative Mean Normalized Difference Function*. Núcleo do YIN: função de
diferença quadrática normalizada pela média cumulativa dos atrasos anteriores. Remove o viés
que faz a autocorrelação preferir lags longos.

**YIN** — de Cheveigné & Kawahara (2002). CMNDF + primeiro mínimo abaixo de um limiar +
refino parabólico. Rápido; um limiar só; curva de F0 com tremor.

**pYIN** — Mauch & Dixon (2014). YIN com muitos limiares ponderados por uma Beta,
transformando o resultado em distribuição de probabilidade, e decisão de trajetória por
HMM+Viterbi. É o algoritmo do protótipo.

**SWIPE′** — Camacho (2008). Compara `√|X(f)|` com um kernel de peneira harmônica. Grade
uniforme em ERB. Frame-a-frame, sem Viterbi → naturalmente causal.

**ERB** — *Equivalent Rectangular Bandwidth*. Escala perceptual que modela a largura dos
filtros auditivos: `ERB = 21,4·log₁₀(1 + 0,00437·f)`.

**Erro de oitava** — O detector reporta F0/2 ou 2·F0. Modo de falha mais comum e mais
audível. A diferença entre RPA e RCA isola exatamente esses erros.

**Quadro / hop** — Quadro = pedaço analisado de uma vez (1024 amostras = 23,2 ms). Hop =
passo entre quadros (256 amostras = 5,8 ms). Razão frame/hop = 4 → 75% de sobreposição.

### Decisão de trajetória

**HMM (Hidden Markov Model)** — Modelo em que um estado oculto (qual bin de pitch) evolui por
probabilidades de transição e gera emissões observáveis (os votos dos 100 limiares). Permite
dizer "esse quadro parece 440 Hz, mas dado que o anterior era 220 Hz, é mais provável que
seja erro de oitava".

**Viterbi** — Programação dinâmica que acha a sequência de estados de maior probabilidade
conjunta. Guarda ponteiros de retrocesso (`psi`) e faz backtrack no fim. Custo `O(T·S²)`,
aqui reduzido por limitar a transição a ±12 bins.

**Viterbi de lag fixo** — Versão causal: decide o quadro `t` retrocedendo a partir de
`t+look`. Com `look = 0` vira decodificação gulosa; com look grande converge para o Viterbi
global. **É o botão latência ↔ qualidade do projeto.**

**Distribuição Beta** — Distribuição sobre [0,1]. Aqui `Beta(2,18)` (`w ∝ s(1−s)¹⁷`) pondera
os 100 limiares do YIN, concentrando confiança perto de s ≈ 0,1.

**Log-probabilidade** — Somar logs em vez de multiplicar probabilidades. Evita *underflow* ao
longo de milhares de quadros e troca multiplicações por somas.

### Correção e síntese

**TD-PSOLA** — *Time-Domain Pitch-Synchronous Overlap-Add* (Moulines & Charpentier, 1990).
Corta o sinal em grãos centrados em marcas espaçadas de um período e os recola com
espaçamento diferente. Muda o F0 **sem** reamostrar → preserva formantes.

**Marca de análise (pitch mark)** — Uma posição por período, ancorada num pico e encadeada
por correlação cruzada máxima com a anterior. O alinhamento de fase é o que evita
cancelamento no overlap-add.

**β (razão de deslocamento)** — `β = f_alvo / f_real`. β > 1 = subir a afinação = grãos mais
juntos = alguns duplicados. β = 1 = identidade (e o teste de regressão exige que a saída seja
*idêntica* à entrada nesse caso).

**Overlap-add / COLA** — Somar segmentos janelados sobrepostos. *Constant OverLap-Add*: com
janela Hann e 50% de sobreposição, as janelas somam exatamente 1 → reconstrução perfeita.

**Janela de Hann** — `w[n] = ½(1 − cos(2πn/N))`. Sobe e desce suavemente até zero nas bordas,
evitando descontinuidades (cliques).

**Overlap-save** — Estratégia de streaming: reprocessa uma janela com sobreposição do passado
e **comete** só o miolo novo, descartando as bordas contaminadas.

**Zona morta / soft knee** — Faixa em torno da nota em que nenhuma correção é aplicada
(`tol`, em cents). "Soft knee" = fora da faixa, subtrai-se `tol` do desvio, sem degrau na
fronteira.

**Glide / portamento** — Deslizar entre afinações em vez de saltar. Filtro de 1 polo em
cents: `α = e^(−1/(τ·fs))`.

**Filtro de 1 polo** — `y[n] = α·y[n−1] + (1−α)·x[n]`. Passa-baixas mais simples que existe;
resposta exponencial; um coeficiente; causal; O(1) por amostra.

**Retune speed** — Termo do Auto-Tune para um limite **de taxa** da correção (cents por ms).
Conceitualmente diferente da zona morta, que é um limite **de amplitude**. Ver §8.2 e §9.2 C1.

### Tempo real e plugins

**Latência algorítmica** — Atraso que o *algoritmo* impõe por precisar de contexto futuro,
independentemente da velocidade da máquina: `frame + look·hop + 2·fs/FMIN`. Não some
comprando um PC melhor.

**Latência de ida-e-volta (round-trip)** — O que o cantor realmente ouve:
`driver_in + bloco_host + algoritmo + driver_out`. Sempre maior que a algorítmica.

**xRT (fator de tempo real)** — Tempo de processamento ÷ duração do áudio. `xRT < 1` =
viável. O projeto mede 0,025 no caminho B.

**PDC (Plugin Delay Compensation)** — A DAW atrasa as outras faixas para compensar a latência
declarada, alinhando o material *gravado*. **Não ajuda em monitoração ao vivo.**

**RT-safe** — Regra do callback de áudio: nada de `malloc`, `lock`, I/O ou qualquer coisa de
tempo não-limitado. Violar causa *dropouts*. Ver §8.3, Achado 2.

**Ring buffer** — Buffer circular de tamanho fixo com aritmética modular nos índices. Guarda
as N amostras mais recentes sem nunca alocar.

**Block size** — Quantas amostras a DAW entrega por chamada. **Escolhido pelo host**
(64–2048), não pelo plugin. O algoritmo tem que produzir o mesmo resultado para qualquer
valor — o que este projeto verifica explicitamente.

**ASIO** — Driver de áudio de baixa latência no Windows, contornando a pilha do sistema.

**APVTS** — `AudioProcessorValueTreeState` do JUCE: guarda os parâmetros, sincroniza com a
GUI, serializa o estado no projeto da DAW e expõe os valores como átomos lidos com segurança
pelo thread de áudio.

**VST3** — Formato de plugin da Steinberg. No Windows, o caminho oficial do JUCE exige MSVC —
por isso o projeto tem duas toolchains (g++ para os executáveis, MSVC para o plugin).

### Métricas

**RPA** — *Raw Pitch Accuracy*: fração de quadros vozeados com erro ≤ 50 cents.

**RCA** — *Raw Chroma Accuracy*: igual à RPA, com o erro tomado módulo uma oitava.

**GPE** — *Gross Pitch Error*: fração de quadros com erro relativo > 20%.

**Estabilidade temporal** — σ das diferenças quadro-a-quadro da F0, em cents. **A métrica que
decidiu o algoritmo do projeto.** Mede tremor, não acurácia.

**"Pipoco"** — Nome interno para cliques na saída. Medido contando descontinuidades
amostra-a-amostra maiores que 30× a mediana. Meta e resultado atual: zero.

**Similaridade com o offline** — Correlação média, em janelas de 0,5 s, entre a saída causal e
a saída offline ("ouro"). Mede o *custo em qualidade* de ser causal — **não** mede qualidade
percebida (§9.1, L1).

---

## 12. Mapa de arquivos e referências

### `TCC-autotune-cpp`

| Arquivo | Linhas | Papel |
|---|---|---|
| `src/core/dsp.h` | 309 | CMNDF, escalas, nota-alvo, TD-PSOLA, WAV — **compartilhado por tudo** |
| `src/offline_causal/main.cpp` | 207 | Caminho A — offline / gold → `autotune.exe` |
| `src/offline_causal/autotune_rt.cpp` | 233 | Caminho B — causal + relatório de latência → `autotune_rt.exe` |
| `src/c1_streaming/autotune_stream.h` | 542 | Caminho C1 — **núcleo de streaming**, header-only |
| `src/c1_streaming/stream_test.cpp` | 122 | driver headless do C1 → `stream_test.exe` |
| `plugin/PluginProcessor.cpp/.h` | 344 | Caminho C2 — APVTS, processBlock, setLatencySamples |
| `plugin/PluginEditor.cpp/.h` | 267 | GUI: afinador, medidor de cents, LookAndFeel âmbar |
| `plugin/CMakeLists.txt` | — | JUCE 8.0.4 via FetchContent, VST3 + Standalone |
| `python/bench_latencia.py` | 57 | varredura de look-ahead: latência × qualidade × xRT |
| `python/bench_nframe.py` | 59 | varredura de N_FRAME: piso × fmin detectável |
| `python/bench_fmin.py` | 90 | varredura de tessitura: latência × notas perdidas |
| `python/bench_stream.py` | 30 | C1 vs. gold, invariância ao block size |
| `python/bench_pitch.py` · `bench_frames.py` | 50 | trilha de F0 e disparo de quadros vs. gold |
| `python/formantes.py` | 66 | verificação de preservação de formantes (cepstro) |
| `external/dr_wav.h` | — | leitor/gravador WAV header-only |

### `TCC-autotune-python`

| Arquivo | Linhas | Papel |
|---|---|---|
| `pitch_compare/algorithms/reference.py` | 74 | wrappers de autocorr, YIN e pYIN (librosa) |
| `pitch_compare/algorithms/swipe.py` | 127 | **SWIPE′ implementado do zero** — kernel harmônico zero-mean, grade ERB |
| `pitch_compare/metrics.py` | 86 | RPA, RCA, GPE, estabilidade, tempo, memória |
| `pitch_compare/synth.py` | 75 | senoide, harmônico, ruidoso, vibrato, glissando |
| `pitch_compare/datasets.py` | 77 | Vocadito + alinhamento de anotação à grade temporal |
| `pitch_compare/smoothing.py` | 74 | mediana causal/centrada e EMA, com look-ahead declarado |
| `scripts/02` · `03` | 143 | benchmark sintético e no Vocadito |
| `scripts/04` · `05` | 261 | figuras e relatório-síntese em markdown |
| `scripts/06_realtime_benchmark.py` | 204 | **viabilidade em tempo real**: ms/quadro, latência, estabilidade pós-suavização |
| `tests/` | 5 arquivos | pytest sobre synth, datasets, metrics, swipe, reference |

### `TCC-TEXT`

Fonte LaTeX do TCC (classe `tcc.cls` da Escola Politécnica / PUCRS), em
**repositório separado**: [`gacherubini/TCC-TEXT`](https://github.com/gacherubini/TCC-TEXT).

| Arquivo | Papel |
|---|---|
| `tcc1.tex` | documento principal, resumo, inclusão dos capítulos |
| `tcc1-intro.tex` | Capítulo 1 — Introdução |
| `tcc1-sections.tex` | Capítulos 2–11 — contexto, problema, proposta, conceitos, algoritmos, estudo comparativo, resultados, implementação, avaliação com usuário, conclusão, cronograma |
| `apendice-formulas.tex` | apêndice de fórmulas |
| `tcc.bib` | bibliografia |
| `fig/` | figuras (plugin, Ableton, Auto-Tune, PSOLA, exemplo antes/depois) |
| `Makefile` · `sort.sh` | compilação |
| `sync-overleaf.sh` | sincronização com o Overleaf via ZIP |

### Bibliografia do que está implementado

- **de Cheveigné, A.; Kawahara, H.** (2002). *YIN, a fundamental frequency estimator for
  speech and music.* JASA 111(4). — o CMNDF do estágio 2.
- **Mauch, M.; Dixon, S.** (2014). *pYIN: a fundamental frequency estimator using
  probabilistic threshold distributions.* ICASSP. — os 100 limiares Beta e o HMM.
- **Moulines, E.; Charpentier, F.** (1990). *Pitch-synchronous waveform processing techniques
  for text-to-speech synthesis using diphones.* Speech Communication 9(5–6). — o TD-PSOLA.
- **Rabiner, L. R.** (1989). *A tutorial on hidden Markov models and selected applications in
  speech recognition.* Proc. IEEE 77(2). — o Viterbi.
- **Camacho, A.** (2008). *SWIPE: A sawtooth waveform inspired pitch estimator for speech and
  music.* Tese, Univ. of Florida. — o quarto algoritmo do benchmark.
- **Zölzer, U.** (org.). *DAFX: Digital Audio Effects.* Wiley. — referência transversal.
- **Vocadito** — Bittner et al., Zenodo 5578807, CC-BY-4.0.

### Leituras que faltam para o TCC 2

- **WSOLA** (Verhelst & Roelands, 1993) — alternativa ao PSOLA que não precisa de marcas de
  período, potencialmente com menos look-ahead. Relevante para L6.
- **Phase vocoder** (Laroche & Dolson) — a outra família de pitch-shifting, para comparação
  no texto.
- **Pitch shifting por linha de atraso modulada** — arquitetura de latência quase-zero,
  candidata para a hipótese 2 da §8.1.
- **Percepção de latência em monitoração vocal** — a literatura que fundamenta o limiar de
  "quantos ms incomodam o cantor". O número de 20–30 ms hoje aparece apenas como comentário
  de código e **precisa de citação** (correção pendente nº 3 da §10).
- **Retune speed / naturalidade em correção de afinação** — fundamentação para a mudança C1.

---

*Documentação técnica consolidada — TCC PUCRS, 2026. Os itens da §8.3 são análise estática de
código e ainda não foram confirmados por medição.*
