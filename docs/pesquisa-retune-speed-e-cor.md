# Pesquisa — Retune Speed e a origem da "cor"

> **Data:** 2026-08-26
> **Motivo:** antes de implementar o Retune Speed, entender exatamente o que ele faz. E
> responder a uma pergunta separada: **de onde vem a "cor" de um processador vocal?**
> **Fonte principal:** *Auto-Tune Artist User Guide* (Antares, PDF oficial), lido integralmente.
> **Status:** pesquisa concluída. **Contém uma correção a documentação já escrita** — ver §2.

**Marcadores:** `[P]` fonte primária lida · `[P-2ª]` primária via terceiros · `[D]` derivação
própria · `[✗]` procurado, não encontrado.

---

## 0. Resumo — os três achados

| # | Achado | Consequência |
|---|---|---|
| **A** | O **Retune Speed é uma constante de tempo em milissegundos**, confirmado verbatim no manual oficial. Zero suprime vibrato completamente; 10–50 ms é a faixa típica para correção natural. | Confirma a formulação de C1. Define a faixa e o padrão do controle. |
| **B** | 🔴 **O `tol` do protótipo NÃO é o Flex-Tune.** São mecanismos **opostos**: o `tol` é zona morta *em volta da nota* (não corrige perto); o Flex-Tune só corrige *perto da nota* e deixa passar o que está longe. | **Cancela a Decisão 2** (renomear `Tolerancia` → `Flex-Tune`). O nome daria ao controle o significado de outro mecanismo. |
| **C** | **A "cor" de um corretor não vem do timbre — vem da trajetória de altura.** A literatura de transformação de envelope espectral mostra que mudanças de formante só são perceptíveis em deslocamentos da ordem de **uma quinta**; correções de afinação são de dezenas de cents. | Elimina formante/Throat como caminho para "cor". Redireciona para vibrato sintetizado, Natural Vibrato e Humanize — todos baratos na arquitetura atual. |

---

## 1. Retune Speed — o que a fonte primária diz

### 1.1 Manual do Auto-Tune Artist, verbatim `[P]`

> "Retune Speed controls how rapidly the pitch correction is applied to the incoming audio.
> **The units are milliseconds.** A zero setting will cause immediate changes from one pitch to
> another, and will **completely suppress any vibrato or deviations in pitch**.
>
> For the Auto-Tune Effect, set the Retune Speed to zero. **A setting between 10 and 50 is
> typical for more natural sounding pitch correction.** Larger values allow more vibrato and
> other interpretive pitch gestures, but slow down how rapidly corrections are made."

Três coisas ficam estabelecidas:

1. **É tempo, não profundidade.** Milissegundos.
2. **Zero suprime o vibrato inteiro** — o que confirma que o filtro age sobre a *variação* de
   altura, não sobre um alvo estático. Um filtro sobre o alvo (o `glide` de hoje) **não** teria
   esse efeito, porque o alvo já é estático dentro da nota.
3. **A faixa útil é 10–50 ms**, com 0 reservado para o efeito duro.

### 1.2 A implementação, pela patente `[P]`

Já registrado em [pesquisa-bibliografica.md §2.5](pesquisa-bibliografica.md): a patente
(Hildebrand, US 5.973.252) calcula `Resample_Raw_Rate = Cycle_period / desired_Cycle_period`
— **a razão de correção** — e o passo seguinte "*smooths out Resample_Raw_Rate*" com um
coeficiente `Decay` ajustável pelo usuário.

O manual e a patente concordam: **o filtro age sobre a correção.**

### 1.3 Como o Retune Speed interage com o Flex-Tune `[P-2ª]`

Da documentação de uso da Antares:

> "The Retune Speed and Flex-Tune knobs heavily influence each other — for example, a fast
> Retune Speed will require a large Flex-Tune value to sound natural."

**Interpretação `[D]`:** os dois eixos compensam um ao outro. Retune Speed rápido corrige tudo
depressa (soa duro), então precisa de um Flex-Tune que deixe passar mais gesto para compensar.
Isso confirma que são controles **complementares**, e que expor os dois é o desenho certo.

### 1.4 O que ainda não tem fonte `[✗]`

- A **curva exata** do filtro (1 polo? ordem maior? slew limiter?). O manual não especifica.
  A patente descreve suavização exponencial de 1º grau, o que é consistente com 1 polo.
- O **mapeamento** do valor do knob para milissegundos reais (a faixa do controle não é
  publicada).

---

## 2. 🔴 CORREÇÃO — o `tol` do protótipo não é o Flex-Tune

Esta seção corrige afirmação escrita em
[comparacao-antares.md](comparacao-antares.md) e em
[historico-e-decisoes.md](historico-e-decisoes.md) em 2026-08-26.

### 2.1 O que o manual diz, verbatim `[P]`

> "When Flex-Tune is set to zero, Auto-Tune Artist is **always pulling every note toward a
> target scale note**. When Flex-Tune is engaged, **it only applies correction as the performer
> approaches the target note**. As you move the control toward higher values, **the correction
> area around the scale note gets smaller**, and more expressive pitch variation is allowed
> through."

Confirmado por fonte secundária independente `[P-2ª]`: *"Flex-Tune allows you to apply
pitch-correction only when the pitch is close to a scale note; other audio is left
unprocessed."*

### 2.2 Os dois mecanismos são opostos

| | Protótipo (`tol`) | Antares (Flex-Tune) |
|---|---|---|
| Onde **não** corrige | **perto** da nota (`abs(err) <= tol`) | **longe** da nota |
| Onde **corrige** | longe da nota | **perto** da nota |
| O que preserva | micro-variação *na* nota (vibrato pequeno) | macro-gesto *entre* notas (scoop, bend, portamento) |
| Valor alto significa | zona sem correção maior | área **de correção** menor |

São topologias invertidas. O `tol` protege o que está **dentro** de uma faixa; o Flex-Tune
protege o que está **fora** dela.

### 2.3 Por que o Flex-Tune faz sentido musical `[D]`

O caso de uso é um cantor deslizando entre duas notas da escala, ou fazendo um *scoop* de
entrada. Durante o gesto, a altura está **longe de qualquer nota** da escala — e é exatamente
aí que o Flex-Tune não corrige, deixando o gesto intacto. Quando o cantor **pousa** perto da
nota, a correção age e trava a afinação.

Isso funciona melhor com escalas **não-cromáticas**, onde há 200 cents entre notas e o gesto
tem espaço para existir. Na cromática, com no máximo ±50 cents de erro possível, os dois
mecanismos ficam mais parecidos.

### 2.4 Consequência para a Decisão 2

**A Decisão 2 (renomear `Tolerancia` → `Flex-Tune`) fica cancelada.** Dar ao controle o nome de
um mecanismo diferente seria pior que o nome atual: criaria uma afirmação falsa de paridade,
e a banca pode conferir o manual.

Opções, em ordem de preferência `[D]`:

| Opção | Avaliação |
|---|---|
| **A — manter `Tolerância`** | honesto, já compreendido, custo zero. **Recomendada.** |
| **B — renomear para `Zona Morta`** | mais preciso tecnicamente, mas é jargão |
| **C — implementar o Flex-Tune de verdade, como controle adicional** | é o único caminho para paridade real. Vira item novo de backlog, não renomeação |

O protótipo **não perde nada** por não ter Flex-Tune: ele tem um mecanismo diferente que
resolve um problema diferente. O que o texto do TCC precisa é **descrever corretamente o que
tem**, não reivindicar o que não tem.

---

## 3. A "cor" — de onde ela vem

### 3.1 O caminho do formante, e por que ele **não** serve aqui `[P]`

Fonte: Santacruz, Tardón, Barbancho & Barbancho (2016), *Spectral Envelope Transformation in
Singing Voice for Advanced Pitch Shifting*, **Applied Sciences** 6(11):368.

Achado central do artigo, que **contraria a intuição corrente**:

> "This paper presented a novel scheme to achieve more natural modifications of singing voice
> when applying a pitch shifting process. The system addresses the problem of **timbre variation
> with the pitch** by modifying the spectral envelope, **unlike most of the systems developed
> until now, which are based on the idea of formant preservation**."

E a justificativa fisiológica:

> "Depending on the pitch of the note, **trained singers position their voices differently when
> singing**, producing various vocal resonances in the same vocal range, like the chest voice or
> the falsetto voice. This is why spectral envelope preservation **might not be the best choice**
> to perform wide variations in pitch."

Ou seja: **a preservação perfeita de formantes do TD-PSOLA é ela própria uma fonte de
artificialidade**, porque uma voz real muda de timbre ao mudar de altura.

**Mas — e este é o achado que economiza semanas de trabalho — o próprio artigo diz que o efeito
é imperceptível na escala em que um autotune opera:**

> "Observe that the values in Table 2 imply that **medium-large frequency displacements are
> required in order to perceive the effect** of the envelope transformation. Generally, our
> observations indicated that **increasing or decreasing the pitch by a fifth is enough to
> perceive the changes. Smaller shifts do not produce significant changes.**"

Uma correção de afinação move a altura em **dezenas de cents**. Uma quinta são **700 cents**.

> ### 🎯 Conclusão
> **Processamento de formante não resolve o problema de "cor" de um corretor de afinação.**
> A escala de deslocamento é pequena demais para que a transformação de envelope seja audível.
> Isto vale para Throat Length, Formant Correction e para o artigo acima.
>
> Este é um **resultado negativo útil** — ele fecha uma linha de investigação inteira com
> citação, e merece um parágrafo no texto do TCC.

**Ressalva `[D]`:** isso vale para o **corretor**. Se o trabalho algum dia adicionar
*Transpose* (deslocamento de oitava/quinta deliberado), o formante volta a importar.

### 3.2 Então de onde vem a cor? Da trajetória de altura

Os controles que a Antares chama de expressivos, e o que cada um custa na arquitetura atual:

| Controle Antares | O que faz `[P]` | Custo no protótipo `[D]` |
|---|---|---|
| **Retune Speed** | constante de tempo da correção | 🟢 o polo já existe, só muda de lugar |
| **Humanize** | Retune Speed mais lento **só na parte sustentada** de notas longas | 🟢 τ variável no tempo desde o ataque |
| **Natural Vibrato** | aumenta ou diminui o vibrato **já presente** | 🟢 **uma multiplicação** — ver §3.3 |
| **Create Vibrato** | vibrato **sintetizado**, com forma, taxa, envelope e variação aleatória | 🟢 modular o alvo em cents |
| **Targeting Ignores Vibrato** | impede que vibrato largo faça o alvo alternar entre notas | 🟡 lógica no Viterbi |
| **Throat / Formant** | modelo físico de trato vocal | 🔴 caro **e inútil aqui** (§3.1) |

**Cinco dos seis controles expressivos do Auto-Tune atuam sobre a curva de altura**, não sobre o
timbre. Só o último é timbre — e é justamente o que não se aplica.

### 3.3 Natural Vibrato sai quase de graça `[D]`

Este é o achado mais econômico da pesquisa.

Com o Retune Speed implementado como filtro sobre a correção, a saída é:

```
saída = alvo + HP(real)
```

onde `HP(real)` é exatamente **o vibrato do cantor** (a parte rápida do contorno). Basta um
ganho:

```
saída = alvo + k · HP(real)
```

| `k` | Efeito |
|---|---|
| 0 | vibrato removido (equivale a Retune Speed = 0) |
| 1 | vibrato preservado como está |
| > 1 | vibrato **exagerado** — o "Natural Vibrato" positivo da Antares |

**Um multiplicador.** O Natural Vibrato vem junto com C1, sem trabalho adicional de DSP.

### 3.4 Create Vibrato — o conjunto de parâmetros, verbatim `[P]`

O manual especifica o modelo completo, o que dispensa inventar:

| Parâmetro | Definição verbatim |
|---|---|
| **Shape** | seno, quadrada ou dente-de-serra. *"A sine wave […] is the best choice for natural-sounding vibrato"* |
| **Rate** | *"sets the speed of the vibrato in Hertz"* |
| **Onset Delay** | *"the amount of time (in milliseconds) between the beginning of a note and the onset of vibrato"* |
| **Onset Rate** | *"time […] between the onset of vibrato and the point at which the vibrato reaches the full amounts"* |
| **Variation** | *"the amount of random variation applied to the Rate and Amount parameters on a note to note basis […] useful for humanizing the vibrato"* |
| **Pitch Amount** | *"sets the width of the vibrato in cents"* |
| **Amplitude Amount** | *"the amount that the loudness changes. For more realistic vibrato, the amount of amplitude change should usually be **substantially less than pitch change**"* |

**Custo no protótipo:** o pipeline já produz um alvo em cents por amostra e o converte em β
para o PSOLA. Somar `A·sin(2πft)` a esse alvo é aritmética local. **O Onset Delay/Rate já tem a
infraestrutura**: o `tinhaNota` marca o início da nota.

Isto casa diretamente com os parâmetros de vibrato já levantados em
[pesquisa-bibliografica.md §2.7](pesquisa-bibliografica.md): 5–8 Hz, dezenas de cents de
extensão.

### 3.5 Humanize, verbatim `[P]`

> "One situation that can be problematic for pitch correction is a performance that includes
> both short and long sustained notes. In order to get the short notes in tune, you would need
> to set a fast Retune Speed, but this can cause sustained notes to sound unnaturally static.
> **Humanize applies a slower Retune Speed only during the sustained portion of longer notes.**"

Isto responde ao risco levantado ao discutir o Retune Speed: τ lento deixa ataques errados
audíveis. A solução do fabricante é τ **variável com o tempo desde o ataque** — rápido no
começo, lento no sustentado.

### 3.6 Classic Mode — cor por versão de algoritmo `[P]`

> "The difference between Classic Mode and the default sound of Auto-Tune Artist is subtle, but
> if you listen carefully, you may notice a **slightly brighter quality, and a more pronounced
> attack and transition between notes at faster Retune Speeds**."

E o detalhe revelador: no Classic Mode ficam **desabilitados** Formant, Throat Length, Transpose
e Flex-Tune. Ou seja, o "som clássico" cultuado é o algoritmo **sem** o processamento de timbre.

**Leitura `[D]`:** reforça a §3.1. A identidade sonora do Auto-Tune que as pessoas reconhecem
não vem do processamento de formante — vem do **comportamento da correção de altura**.

---

## 4. Consequências para a fila de implementação

### 4.1 Alterações nas decisões já registradas

| Decisão | Situação |
|---|---|
| **2 — renomear `Tolerancia` → `Flex-Tune`** | ❌ **CANCELADA.** Ver §2.4. Recomendação: manter `Tolerância` |
| **4 — adicionar Retune Speed** | ✅ **confirmada**, com faixa e padrão agora fundamentados |

### 4.2 Itens novos, derivados desta pesquisa

| Id | Item | Depende de | Custo | Fundamentação |
|---|---|---|---|---|
| **K1** | **Natural Vibrato** (ganho `k` sobre o vibrato preservado) | C1 | 🟢 trivial — uma multiplicação | §3.3 |
| **K2** | **Humanize** (τ variável com o tempo desde o ataque) | C1 | 🟢 baixo | §3.5 |
| **K3** | **Create Vibrato** (seno somado ao alvo, com envelope de ataque) | — | 🟡 médio | §3.4 |
| **K4** | **Amplitude Amount** (tremolo acoplado ao vibrato) | K3 | 🟢 trivial | §3.4 |
| **K5** | **Flex-Tune de verdade**, como controle adicional ao `tol` | — | 🟡 médio | §2.4, opção C |
| **K6** | **Targeting Ignores Vibrato** | — | 🟡 lógica no Viterbi | §3.2 |

**Nota importante `[D]`:** K1 e K2 **saem de graça junto com C1**. Implementar o Retune Speed
corretamente entrega três controles expressivos pelo preço de um. Isso reordena a prioridade —
C1 deixa de ser "um controle" e passa a ser **a fundação da camada de expressão inteira**.

### 4.3 O que fica explicitamente fora

| Item | Motivo |
|---|---|
| Throat Length / modelagem de trato | §3.1 — imperceptível na escala de correção de afinação |
| Formant Correction | idem. O TD-PSOLA já preserva formantes por construção |
| Transformação de envelope espectral (Santacruz et al.) | idem; e o método usa otimização não-linear com restrições, **sem afirmação de tempo real** `[✗]` |

---

## 5. Referências — com URL

Todas as fontes consultadas nesta pesquisa, com endereço, para conferência e para citação no
texto do TCC.

### 5.1 Fontes primárias — documentação de fabricante `[P]`

| Fonte | URL | O que sustenta |
|---|---|---|
| **Antares** — *Auto-Tune Artist User Guide*, 33 p. (PDF oficial, lido integralmente) | https://antares-web-frontend.sfo3.cdn.digitaloceanspaces.com/documentation/pdfs/Auto-Tune_Artist_Manual.pdf | Retune Speed (§1.1), Flex-Tune (§2.1), Humanize (§3.5), Create Vibrato (§3.4), Classic Mode (§3.6), Throat, Tracking, Transpose, Detune |
| **Antares** — *Auto-Tune Pro X User Guide* v10.3.1 | https://antares-web-frontend.sfo3.cdn.digitaloceanspaces.com/documentation/pdfs/Auto-Tune_Pro_X_User_Guide_Version_10.3.1.pdf | versão completa do produto; confirma a nomenclatura |
| **Antares** — *Auto-Tune Access User Guide* v1.1 | https://antares-web-frontend.sfo3.cdn.digitaloceanspaces.com/documentation/pdfs/Auto-Tune_Access_User_Guide_v1.1.pdf | versão reduzida; confirma o subconjunto mínimo de controles |
| **Antares** — documentação web, *Basic View Controls* | https://www.antarestech.com/documentation/auto-tune-artist/basic-view-controls | mesma redação do PDF, em HTML |
| **Antares Support** — *AutoTune 2026 FAQ* | https://help.antarestech.com/hc/en-us/articles/42855736822932-AutoTune-2026-FAQ | estado atual do produto |
| **Universal Audio** — *Auto-Tune Realtime X Manual* | https://media.uaudio.com/support/manuals/dd/Auto-Tune+Realtime+X+Manual.pdf | variante de baixa latência licenciada pela UA |

### 5.2 Fontes primárias — patente e literatura revisada por pares `[P]`

| Fonte | URL / DOI | O que sustenta |
|---|---|---|
| **Hildebrand, H. A.** (1999). *Pitch detection and intonation correction apparatus and method.* US 5.973.252 A | https://patents.google.com/patent/US5973252A/en | a implementação do Retune Speed como suavização da razão de reamostragem; o rastreio recursivo de período |
| **Santacruz, J. L.; Tardón, L. J.; Barbancho, I.; Barbancho, A. M.** (2016). *Spectral Envelope Transformation in Singing Voice for Advanced Pitch Shifting.* Applied Sciences 6(11):368 | DOI [10.3390/app6110368](https://doi.org/10.3390/app6110368) · PDF: https://pdfs.semanticscholar.org/b49f/eeb850c5990e71a00af8a7d350230b9a9f67.pdf | **o resultado negativo da §3.1**: transformação de envelope só é perceptível a partir de ~uma quinta |

### 5.3 Fontes secundárias `[P-2ª]`

| Fonte | URL | O que sustenta |
|---|---|---|
| **KVR Audio** — *Antares releases Auto-Tune 8 with Flex-Tune Pitch Correction* | https://www.kvraudio.com/news/antares-releases-auto-tune-8-with-flex-tune-pitch-correction-28823 | confirmação independente da semântica do Flex-Tune (§2.1) |
| **Sweetwater** — *How does Auto-Tune's Retune Speed setting work?* | https://www.sweetwater.com/sweetcare/articles/auto-tune-retune-speed-setting-work/ | faixa típica de uso |
| **Black Ghost Audio** — *How to Perfectly Auto-Tune Vocals in 7 Steps* | https://www.blackghostaudio.com/blog/how-to-perfectly-auto-tune-vocals-in-7-steps | interação Retune Speed × Flex-Tune (§1.3); uso prático do Humanize |
| **Sound on Sound** — *Antares Auto-Tune 8* (review) | https://www.soundonsound.com/reviews/antares-auto-tune-8 | contexto histórico da introdução do Flex-Tune |

### 5.4 Consultadas e **não** utilizadas

Registradas para que ninguém refaça o caminho.

| Fonte | URL | Por que não foi usada |
|---|---|---|
| **Kong, T.** — *Phase Vocoder Implementation with FLWT and TD-PSOLA*, Stanford EE264 | https://web.stanford.edu/class/ee264/projects/EE264_w2015_final_project_kong.pdf | trabalho de disciplina, não revisado por pares; nada além do que já está em Moulines & Charpentier |
| **Bernsee, S.** — *Time Stretching And Pitch Shifting of Audio Signals: An Overview* | http://blogs.zynaptiq.com/bernsee/time-pitch-overview/ | material de blog; bom panorama, sem dado citável novo |

> ⚠️ **Nota de acesso.** O PDF do manual da Antares e o artigo da *Applied Sciences* foram
> baixados e extraídos com `PyMuPDF` — o texto corrido do PDF não é legível por conversores
> simples. As citações verbatim deste documento vieram dessa extração e **foram conferidas
> contra o texto original**. Os PDFs devem ser arquivados junto ao TCC.
