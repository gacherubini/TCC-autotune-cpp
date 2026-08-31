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

## Redesenho da interface para paridade com o Auto-Tune (2026-08-26)

Origem: análise comparativa do Auto-Tune Artist (view ADVANCED) contra o protótipo, registrada
em [comparacao-antares.md](comparacao-antares.md). A análise partiu do sintoma de naturalidade
reprovado no teste de usuário e desembocou numa pergunta anterior: **o protótipo tem os
controles que um corretor de afinação precisa ter?**

> **Status geral: DECIDIDO, NÃO IMPLEMENTADO.** Nada abaixo alterou código ainda.
> Este bloco existe para que cada passo do TCC 2 fique registrado com a data e o motivo.

### Decisão 1 — remover o parâmetro `Forca`

**O que muda:** `Forca` (0–1, `PluginProcessor.cpp:57`) sai da interface e de
`notaAlvo()` (`dsp.h:159`).

**Por quê:** a análise mostrou que `Forca` é um **escalar estático** multiplicando a correção
(`corrMidi = midi + (forca * mov) / 100.0`) — sem estado, sem memória, sem dimensão temporal.
É um dry/wet no domínio da afinação. O efeito prático de `Forca < 1` é deixar o cantor
**permanentemente desafinado**: numa nota sustentada 50 cents abaixo, `Forca = 0,5` entrega
25 cents abaixo *para sempre*, em vez de chegar à nota devagar.

O Auto-Tune não tem controle equivalente justamente por isso: a filosofia é ir sempre 100% até
a nota e controlar apenas o **tempo**. Ver [comparacao-antares.md §4](comparacao-antares.md).

**Ressalva registrada:** `Forca = 0` é hoje o caminho de bypass usado pelo teste de regressão
bit-perfect (`python/bench_stream.py` verifica identidade em `forca = 0`). **Remover `Forca`
exige substituir esse teste.**

> ✅ **Resolvido em 2026-08-26 — decisão do autor: mix seco/molhado.**
>
> A `Forca` é substituída por um **mix seco/molhado** (item C4 do backlog), que passa a
> acumular as duas funções:
>
> 1. **Controle musical.** Mistura de *sinal*, não de *afinação* — é o recurso que engenheiros
>    usam para devolver naturalidade, e é o que faltava na interface (ver
>    [comparacao-antares.md §5](comparacao-antares.md), Nível 1).
> 2. **Caminho de bypass para o teste de regressão.** Com `mix = 0%` (100% seco) a saída deve
>    ser **bit-idêntica** à entrada, exatamente como `forca = 0` era hoje. O
>    `python/bench_stream.py` passa a verificar identidade nessa condição.
>
> **Diferença conceitual que precisa ficar clara no texto do TCC:** `Forca = 0` e `mix = 0%`
> produzem o mesmo áudio, mas por motivos diferentes. `Forca = 0` mandava o corretor mirar na
> própria altura do cantor (o PSOLA rodava, com β = 1); `mix = 0%` **não processa** — devolve a
> entrada. O segundo é mais barato e mais honesto como bypass.
>
> **Consequência não óbvia:** com o mix, `bench_stream.py` deixa de exercitar o caminho de
> PSOLA com β = 1. Esse teste ainda tem valor (ele pegava drift de fase), então convém
> **manter os dois**: identidade por `mix = 0%` e identidade por β = 1 forçado.

> 🔨 **EXECUTADO em 2026-08-26** — Etapa 2 do plano. Registro completo em
> [`execucao-do-plano.md`](execucao-do-plano.md).
>
> A ressalva acima estava certa, e o "β = 1 forçado" não precisou de código novo: **`tol=600`**
> (tolerância maior que meio semitom) faz `mov = 0`, logo `alvo == f0`, logo β = 1 com o PSOLA
> rodando inteiro. Verificado por checksum **antes** de qualquer alteração, ainda com o código
> antigo: `tol=600` produz o mesmo arquivo, byte a byte, que `forca=0` produzia — nos dois
> caminhos (offline `4f35cced…`, streaming `373037487…`). A cobertura migrou com prova.
>
> Os dois casos viraram um **invariante** verificado pelo `baseline.sh`: `tol600` e `mix0` têm
> de ser iguais **entre si**, o que continua valendo depois de qualquer re-baseline. Se o PSOLA
> ganhar drift de fase, o primeiro muda e o segundo não. O script aborta se isso quebrar.
>
> **Duas coisas a mais, que a decisão não previa:**
>
> 1. **Alinhamento no streaming.** A saída sai atrasada de `latSamples`; o seco precisa ser
>    atrasado igual, senão a mistura vira filtro-pente. Como só afeta valores *intermediários*
>    de mix, nenhum teste de identidade pegaria. Virou a Seção 3 do `test_mix.cpp`.
> 2. **O id do parâmetro mudou** (`"forca"` → `"mix"`), em vez de ser reaproveitado — senão o
>    host restauraria um valor antigo com semântica nova, calado.

### Decisão 2 — renomear `Tolerancia` para `Flex-Tune`

**O que muda:** o rótulo do parâmetro. O comportamento é idêntico.

**Por quê:** a pesquisa bibliográfica confirmou que a Antares expõe **dois** controles
separados — *Flex-Tune* (zona morta em cents) e *Retune Speed* (constante de tempo em ms) — e
que o protótipo implementou corretamente o **primeiro**, apenas batizando-o de forma diferente.
Adotar o nome do domínio elimina a ambiguidade no texto do TCC e na banca.

**Nota de compatibilidade:** o `ParameterID` (`ids::tol`) **deve ser preservado** para não
quebrar projetos de DAW já salvos. Muda o nome visível, não o identificador.

> ❌ **CANCELADA em 2026-08-26**, após leitura do manual oficial do Auto-Tune Artist.
> Ver [pesquisa-retune-speed-e-cor.md §2](pesquisa-retune-speed-e-cor.md).
>
> **A premissa estava errada.** O `tol` do protótipo e o Flex-Tune da Antares não são o mesmo
> mecanismo com nomes diferentes — são **mecanismos opostos**. Verbatim do manual:
>
> > "When Flex-Tune is engaged, **it only applies correction as the performer approaches the
> > target note**. As you move the control toward higher values, **the correction area around
> > the scale note gets smaller**."
>
> | | `tol` do protótipo | Flex-Tune |
> |---|---|---|
> | Não corrige | **perto** da nota | **longe** da nota |
> | Preserva | micro-variação *na* nota | macro-gesto *entre* notas |
>
> Adotar o nome criaria uma afirmação falsa de paridade que a banca pode conferir no manual.
>
> **Decisão revista: manter `Tolerância`.** O protótipo não perde nada — ele tem um mecanismo
> diferente, que resolve um problema diferente. O texto do TCC deve **descrever corretamente o
> que tem**, não reivindicar o que não tem. Implementar o Flex-Tune de verdade vira item novo
> (**K5**), não renomeação.

### Decisão 3 — adicionar `Retune Speed`

**O que muda:** parâmetro novo, em milissegundos, ao lado do Flex-Tune.

**Por quê:** é o eixo que falta. Os três eixos de um corretor são **profundidade** (quão longe),
**limiar** (quão grande o erro precisa ser) e **tempo** (quão rápido). O protótipo tinha os dois
primeiros e nenhum controle real do terceiro.

**Como implementar:** a infraestrutura já existe — o `Glide` é um filtro de 1 polo, exatamente a
estrutura necessária. O problema é **onde** ele está ligado
(`autotune_stream.h:434`): ele filtra `alvoCents`, o **destino**, que dentro de uma nota
sustentada é praticamente constante — o filtro converge e deixa de agir. A formulação correta,
retirada da patente Hildebrand US 5.973.252, filtra a **correção**:

```c
// hoje
glideEstado = α*glideEstado + (1−α)*alvoCents;
// proposto
movFiltrado = α*movFiltrado + (1−α)*(mov);
corrMidi    = midi + movFiltrado / 100.0;
```

**Dimensionamento:** τ ≳ 50 ms para preservar vibrato de 5–8 Hz, com faixa do controle indo de 0
(efeito duro, que precisa continuar disponível) até ~200 ms. Ver
[pesquisa-bibliografica.md §2.7](pesquisa-bibliografica.md).

**Questão em aberto:** `Glide` e `Retune Speed` viram o mesmo controle, ou coexistem?

> ✅ **Resolvido em 2026-08-26 — decisão do autor: fundir.**
>
> A fusão não é uma troca, é uma **generalização**. Com dois estados de filtro:
>
> ```
> outCents = LP(alvoCents) + k·(realCents − LP(realCents))
> ```
>
> | `k` | Comportamento |
> |---:|---|
> | **0** | `outCents = LP(alvo)` — **exatamente o Glide de hoje** |
> | **1** | `outCents = real + LP(mov)` — Retune Speed da patente |
> | **> 1** | vibrato exagerado — Natural Vibrato positivo (K1) |
>
> O comportamento antigo **não se perde**: vira o caso `k = 0`. Isso dá o teste de
> não-regressão da etapa — com `k = 0` e o τ atual, a saída deve bater **amostra a amostra**
> com a versão anterior.
>
> E o Natural Vibrato (K1) deixa de ser item separado: é o próprio `k`.
>
> Plano completo em [plano-de-implementacao.md](plano-de-implementacao.md).

### Decisão 4 — expor as 24 tonalidades

**O que muda:** o combo único `Escala` (7 opções fixas) vira dois controles: `Key` (12 tônicas)
× `Scale` (cromática / maior / menor natural).

**Por quê:** **o motor já suporta as 24 tonalidades; a interface expõe 6.** `definirEscala()`
(`dsp.h:112`) calcula `g_permitida[(pc + iv[i]) % 12]` para qualquer tônica `pc`, mas
`PluginProcessor.cpp:30` passa apenas sete strings fixas. Na prática é impossível corrigir em
Ré maior, Si bemol maior ou Mi maior — qualquer tonalidade com mais de um sustenido ou bemol.

**Custo:** nenhuma linha de DSP. É montagem de combo e formatação da string passada a
`definirEscala()`. É a maior melhoria por linha de código identificada no projeto.

**Nota de compatibilidade:** trocar um `AudioParameterChoice` por dois quebra o estado salvo.

> ✅ **Resolvido em 2026-08-26 — decisão do autor: expor todas as tonalidades, aceitando a
> quebra de estado salvo.**
>
> **A justificativa deixou de ser teórica.** Durante a preparação do teste de usuário foi
> preciso **procurar um instrumental que estivesse em uma das 6 tonalidades disponíveis**, em
> vez de escolher o material musical livremente. Isso foi registrado retroativamente como
> **Achado 3** do teste de usuário — ver
> [teste-de-usuario.md §5-bis](teste-de-usuario.md).
>
> Ou seja: a limitação não é uma lacuna de paridade com o Auto-Tune detectada em análise de
> escritório. É uma **falha de usabilidade observada em uso real**, que chegou a **enviesar o
> repertório do próprio teste** (registrada como limitação metodológica §6, item 7).
>
> **A quebra de estado salvo é aceitável** porque o plugin ainda não tem base instalada — não
> há projetos de terceiros a preservar. Documentar a quebra e seguir.
>
> **Escopo confirmado:** 12 tônicas × {cromática, maior, menor natural}. Os modos gregos e as
> menores harmônica/melódica **não** entram — não foram pedidos, e o `definirEscala()` hoje só
> conhece `maior[7]` e `menorN[7]` (`dsp.h:132-133`). Ampliar além disso seria mudança de DSP,
> não de interface, e sairia do escopo desta decisão.

### Decisão 5 — modo de baixa latência

**O que muda:** um controle novo que reconfigura `look`, `nFrame` e a guarda do PSOLA de uma vez.

**Por quê:** o teste de usuário reprovou o requisito de latência, e a pesquisa mostrou que a meta
original (≤ 20 ms) estava mal fundamentada — os limiares medidos para voz com in-ear são
**mais rigorosos** (Lester & Boley 2007: "Fair" ≈ 6,5 ms; Marentakis et al. 2012: coloração a
partir de 13 ms).

**Status:** ⏸️ **especificação escrita, implementação suspensa por decisão do autor.**
A especificação completa está em [modo-baixa-latencia.md](modo-baixa-latencia.md) e inclui seis
questões em aberto (§8) que precisam de resposta antes de qualquer código. O ponto central: o
modo v1 (só parâmetros) chega a 17,1 ms no preset contralto, o que **ainda não cruza o limiar de
coloração**; só o v2, que exige mudança de arquitetura de detecção (CMNDF recursivo), chega aos
5,7 ms.

### Decisão 6 — escala global permanece global (2026-08-26)

`g_permitida[12]` (`dsp.h:109`) é estado global. Duas instâncias do plugin compartilham a
escala. **Decisão do autor: fora do escopo, documentar como limitação conhecida.** Não é
regressão (já é assim), o uso previsto é uma faixa por vez, e não afeta os requisitos
reprovados no teste de usuário. Caminho de correção registrado para trabalho futuro: mover
`g_permitida` para dentro do estado do `AutotuneStream`.

### Decisão 7 — o deslize de entrada é fixo (2026-08-26)

A nota sempre nasce na afinação cantada e desliza até o alvo. **Não vira botão.** Um controle
a menos, o Auto-Tune também não expõe a escolha, e o deslize é justamente o gesto que o
diagnóstico apontou como apagado — torná-lo opcional enfraqueceria o resultado.

O comportamento antigo continua alcançável por flag interna (`ataqueNoAlvo`), usada **apenas**
pelo teste de não-regressão da etapa 3. Ver
[plano-de-implementacao.md §11](plano-de-implementacao.md).

### Decisão 8 — Create Vibrato sai da interface, fica no DSP (2026-08-31)

**O que muda:** os quatro widgets do Create Vibrato saem da faixa de controles do editor — o
combo `Create Vib` e os sliders `Vib Rate`, `Vib Depth` e `Vib Amp`. **Só eles.** `FormaVibrato`,
`formaVibrato()`, os campos `vibForma`/`vibTaxa`/`vibProf`/`vibAmp` de `ParamsCorrecao`, as flags
`vibforma=`, `vibtaxa=`, `vibprof=` e `vibamp=` dos CLIs e os quatro parâmetros do APVTS
**continuam existindo e funcionando**. Como permanecem no APVTS, seguem **automatizáveis pelo
host** e alcançáveis pela linha de comando — só deixam de ocupar espaço na tela custom.

Em uma linha: **o DSP fica; a exposição na GUI sai.**

**Por quê — quatro motivos:**

1. **Tensão com o escopo declarado do projeto.** [comparacao-antares.md §6](comparacao-antares.md)
   e [documentacao-tecnica.md §8.2](documentacao-tecnica.md#82-a-cor-por-que-soa-duro-e-estático)
   sustentam que o protótipo é um **corretor**, não um **colorizador** — e foi esse mesmo
   argumento que justificou cortar Throat Length e Formant Correction (Nível 3). O Create
   Vibrato é um **gerador**: não corrige nada. Manter os quatro controles em pé de igualdade com
   os de correção abre um flanco óbvio na banca — *"cortaram formante alegando que corretor não
   colore, e implementaram um LFO de vibrato?"*.

2. **A justificativa de entrada era a mais fraca das cinco etapas.** K1 (Natural Vibrato) e K2
   (Humanize) entraram porque **saem de graça** do Retune Speed: uma multiplicação e um τ
   variável. K3/K4 não saem de graça de nada — são LFO, quatro formas de onda, rampa de onset e
   modulação de amplitude. A única justificativa registrada é a da tabela do
   [§5 Nível 2](comparacao-antares.md), *"parâmetros já especificados pelo manual"*, que diz
   **como** implementar, não **se** deveria. Diferente do Mix e do Retune Speed, o K3 nunca
   recebeu o `✅ decidido` naquela tabela.

3. **Ele não endereça a reprovação que originou o plano.** O teste de usuário reprovou porque o
   vibrato **do cantor** era destruído. Quem conserta isso é o Natural Vibrato (`k`, Decisão 3).
   Somar um vibrato sintético é outro problema.

4. **Custo de interface desproporcional.** Eram **4 dos 13** controles da faixa — ~31 % da
   densidade que a dívida de interface da Etapa 5 registrou como problema. Sem eles a faixa cai
   para **9** controles e cabe em três grupos numa linha só (**Escala | Correção | Motor**).

**O que se perde, registrado:** a *descoberta*. Quem abrir a janela do plugin não vê mais que o
vibrato sintético existe — precisa saber que ele está lá, na lista genérica de parâmetros do host
ou nas flags dos CLIs. É um custo assumido: o público do gerador é o próprio autor, para
demonstração, não o usuário do corretor.

**Não muda áudio.** Nenhum caminho de DSP é tocado, então nenhuma hash do `baseline.sh` deve
mudar, e o `test_expressao.cpp` continua cobrindo o gerador inteiro — inclusive a forma
`off`, que precisa reproduzir a Etapa 4 bit a bit.

### Ordem de implementação acordada

| # | Item | Muda DSP? | Risco | Questões em aberto |
|---|---|---|---|---|
| 1 | **24 tonalidades (Key × Scale)** | não | quebra estado salvo (aceito) | ✅ nenhuma |
| 2 | Remover Forca, adicionar mix seco/molhado | sim | teste de regressão a reescrever | ✅ nenhuma |
| 3 | **Retune Speed** (funde o `Glide`; polo sobre a correção) | sim | médio | ✅ nenhuma — fundir |
| 4 | **K1 · Natural Vibrato** | sim | 🟢 trivial | ✅ **é o próprio `k`** da fusão |
| 5 | **K2 · Humanize** | sim | 🟢 baixo | ✅ nenhuma — sai junto do item 3 |
| 6 | **K3 · Create Vibrato** | sim | médio | ⚠️ **DSP feito, GUI retirada** — Decisão 8 |
| 7 | **K4 · Amplitude Amount** | sim | 🟢 trivial | idem K3 — sai da GUI junto, Decisão 8 |
| 8 | **K5 · Flex-Tune de verdade** | sim | médio | ⚠️ convive com `tol` como? |
| 9 | **K6 · Targeting Ignores Vibrato** | sim | médio | ⚠️ mexe no Viterbi |
| 10 | Modo de baixa latência | v1 não, v2 sim | alto | ⏸️ 6 questões em aberto |
| — | ~~Renomear Tolerancia → Flex-Tune~~ | — | — | ❌ **cancelada** — premissa errada |

**A ordem mudou duas vezes em 2026-08-26.**

Primeiro, as 24 tonalidades subiram para o primeiro lugar: único item com risco nulo, causa
trivial e **evidência direta de uso real** (Achado 3 do teste de usuário).

Depois, a pesquisa sobre Retune Speed
([pesquisa-retune-speed-e-cor.md](pesquisa-retune-speed-e-cor.md)) mostrou que o item 3
**não é um controle — é a fundação da camada de expressão inteira**. Com o polo movido para a
correção, a saída passa a ser `alvo + HP(real)`, e daí saem de graça:

- **K1 (Natural Vibrato)** — um ganho `k` sobre o termo `HP(real)`. Uma multiplicação.
- **K2 (Humanize)** — τ variável com o tempo desde o ataque. O `tinhaNota` já marca o ataque.

Três controles expressivos pelo preço de um. Isso torna o item 3 o de melhor retorno da fila
depois das tonalidades.

---

## Errata — afirmações corrigidas pela pesquisa bibliográfica (2026-08-26)

> **Este bloco não apaga nada.** As afirmações originais continuam no corpo dos documentos, como
> registro do que se acreditava antes da revisão bibliográfica. Esta errata diz o que mudou e
> por quê — que é exatamente o tipo de rastro que um TCC precisa ter.

Fonte das correções: [pesquisa-bibliografica.md](pesquisa-bibliografica.md), levantada em
2026-08-26 (5 artigos revisados por pares, 1 patente, 4 manuais de fabricante, 2 implementações
de código aberto).

| # | Onde | Afirmação original | Correção |
|---|---|---|---|
| 1 | doc. técnica §8.2 | O protótipo implementou "o mecanismo errado" (zona morta em vez de retune speed) | **Falso.** A Antares expõe **os dois**, separadamente. O protótipo implementou o Flex-Tune corretamente e só o nomeou como o outro. Falta *acrescentar*, não *substituir* |
| 2 | doc. técnica §9.2 (C1) | "Filtrar a correção em vez do alvo" apresentado como contribuição original | **Não é original.** Está na patente Hildebrand US 5.973.252 (1997) e é o comportamento documentado do Retune Speed. Reposicionar como **replicação fundamentada** |
| 3 | doc. técnica §9.1 (L6) | Mecanismo descrito como "janela de 2τ → 1τ" | **Erra o mecanismo.** A janela continua sendo de 2 períodos — mas de sinal **passado**, atualizado recursivamente por amostra. O custo real é o **lote quadro/hop**, não a janela |
| 4 | doc. técnica §10 | L6 no item 17 do backlog ("só se sobrar tempo") | **Repriorizar para o topo.** É a única mudança capaz de levar o RNF01 à faixa defensável pela literatura |
| 5 | doc. técnica §9.2 | Meta da métrica: "razão ≥ 0,8 em 4–8 Hz **e** ≈ 0 abaixo de 1 Hz" | **Matematicamente insatisfazível com um único polo:** em f_c = 3 Hz, G(1 Hz) = 0,32, não 0. Ou a meta afrouxa, ou C1 precisa de filtro de ordem maior |
| 6 | doc. técnica §12 | Dattorro (JAES 1997) citado para pitch shifting por linha de atraso modulada | **Referência errada** — o artigo não trata de pitch shifting. A fonte correta é **Disch & Zölzer, DAFx-99** |
| 7 | doc. técnica §12 | WSOLA descrito como "potencialmente com menos look-ahead" | **Sem evidência.** Manter a referência, retirar a afirmação de redução de latência |
| 8 | repo Python, comentário | "autotune ao vivo tolera no máximo ~20–30 ms" | **Sem respaldo revisado por pares.** Rastreia até marketing da Antares. Os valores medidos para voz + in-ear são bem mais rigorosos (§5 acima) |

**Consequência para o texto do TCC:** o requisito **RNF01** precisa ser reescrito com um limiar
**nomeado e citado**, e não com o número de 20–30 ms. Ver
[modo-baixa-latencia.md §5](modo-baixa-latencia.md).

---
