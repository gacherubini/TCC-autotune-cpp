# Pesquisa bibliográfica — fundamentação das soluções do TCC 2

Levantamento de **fontes primárias** (artigos revisados por pares, documentação oficial de
fabricante, patentes e código-fonte de primeira mão) para as três frentes em aberto do
trabalho: o limiar de latência tolerável em monitoração vocal, a distinção entre *retune
speed* e zona morta, e as arquiteturas de baixa latência para detecção e deslocamento de
pitch.

**Como ler este documento.** Cada achado é amarrado, quando aplicável, ao identificador da
solução correspondente na [documentação técnica](documentacao-tecnica.md): **L1–L7**
(latência, §9.1), **C1–C5** (cor/naturalidade, §9.2) e **A1–A4** (engenharia, §9.3).

**Convenção de confiança usada aqui:**

| Marca | Significado |
|---|---|
| **[P]** | Fonte primária lida diretamente (artigo, manual do fabricante, patente, código-fonte). |
| **[P-2ª]** | Fonte primária existe e foi identificada, mas o dado citado foi lido **dentro de outra fonte primária** (citação de segunda mão). O original não foi acessado. |
| **[V]** | Material de fabricante **não revisado por pares** (blog, página de marketing). Vale como registro do que a indústria afirma, não como evidência científica. |
| **[D]** | **Derivação minha** a partir de fontes citadas. Não é citação — é raciocínio que precisa ser apresentado como tal no texto do TCC. |
| **[✗]** | Procurado e **não encontrado**. |

---

## 0. Sumário — o que foi e o que não foi encontrado

### 0.1 Achados que mudam decisões do trabalho

| # | Achado | Impacto |
|---|---|---|
| **A** | O teto de **"20–30 ms"** não tem respaldo na literatura revisada por pares **para monitoração vocal com fone in-ear**. O número aparece com esse exato fraseado em **material de marketing da própria Antares** [V]. Os dados experimentais disponíveis são **mais rigorosos**: Lester & Boley (2007) [P] reportam, para voz com in-ear, limiar "Good" de ~1 ms e "Fair" de ~6,5 ms (limite inferior de intervalo de confiança de 85%). | **Reformular RNF01.** A meta de ≤ 20 ms não é "o limiar do incômodo"; é, na melhor leitura, o limiar de "Fair" para **monitor de chão**, não para in-ear. |
| **B** | O **Retune Speed do Auto-Tune** é documentado pela Antares como um controle **em milissegundos** que rege "quão rapidamente a correção é aplicada" — e a patente original (Hildebrand, US 5.973.252) [P] revela a implementação: um **filtro de suavização exponencial aplicado à taxa de reamostragem**, isto é, **à razão de correção**, não ao alvo. | **C1 está correto, mas não é novo.** É a formulação da própria Auto-Tune desde 1997. Isso é bom: dá citação de primeira mão para a mudança, e obriga a reposicionar C1 como *replicação fundamentada*, não contribuição original. |
| **C** | A zona morta implementada no protótipo (`tol` em cents) corresponde conceitualmente ao **Flex-Tune** da Antares, não ao Retune Speed. A Antares expõe os **dois** controles, separadamente [P]. | Reescreve a §8.2 "Quantização 4": o protótipo não implementou o mecanismo errado — implementou **um** dos dois mecanismos e chamou de o outro. |
| **D** | A hipótese **L6** ("uma vez travado, basta um período novo") **já existe na literatura** — e, mais importante, **já está na patente da Auto-Tune** [P]: as funções E e H são atualizadas **recursivamente a cada amostra** (custo O(1) por lag), o que dá estimativa disponível *em toda amostra*, usando apenas sinal **passado**. Existe também linha acadêmica ativa: laços travados em período (PM-HLL, Hohmann 2021, Acta Acustica [P]) que seguem varreduras de F0 "com atraso menor que um período". | **L6 não é contribuição original**, mas **é viável e está bem fundamentada**. E o ganho real é maior do que o previsto: não é 2τ→1τ, é a **eliminação do lote quadro/hop**. |
| **E** | O Auto-Tune **declara oficialmente** sua latência: **112 amostras (2,3 ms) em Modern Mode e 37 amostras (0,77 ms) em Classic Mode, a 48 kHz, em Low Latency Mode** [P]. A crença do autor ("~3 ms") está **confirmada em ordem de grandeza**. A latência do **HQ mode não é publicada**. | L7 continua necessário (medir), mas a citação de manual agora existe e é precisa. |

### 0.2 O que **não** foi encontrado

| Buscado | Resultado |
|---|---|
| Estudo revisado por pares que estabeleça "20–30 ms" como limiar de incômodo para cantores | **[✗] Não encontrado.** Nenhuma fonte revisada por pares foi localizada com esse limiar para monitoração vocal. |
| Código-fonte do **GSnap** (GVST) | **[✗] Não encontrado.** O GSnap é *freeware*, não *open source*. Não há repositório público nem licença que libere o fonte. Só o manual do usuário está disponível. |
| Número oficial de latência do Auto-Tune **fora** do Low Latency Mode (modo HQ) | **[✗] Não publicado** em nenhum manual Antares consultado. Números como "58,2 ms" circulam em páginas de revendedor, **sem respaldo em documentação do fabricante**. |
| Latência declarada em ms para Auto-Tune Pro 11, EFX+ 10 e Hybrid | **[✗] Não publicada.** Os manuais só mencionam a *existência* da opção "Use Low Latency". |
| Formulação "filtrar a correção, não o alvo" descrita em **artigo acadêmico** | **[✗] Não encontrada** em literatura revisada por pares. Está em **patente** [P] e implicitamente em manual [P]. |
| Fonte primária afirmando explicitamente "cantores toleram menos latência que instrumentistas" **como regra geral** | **[✗] Refutado parcialmente** — ver §1.3. Nos dados de Lester & Boley o saxofone é ainda mais crítico que a voz. O que os dados sustentam é uma afirmação diferente e mais precisa. |
| Valor de latência de referência para pitch-shifter por linha de atraso modulada | **[✗] Não publicado** no artigo primário (Disch & Zölzer, 1999). O artigo descreve a arquitetura e os artefatos, mas não quantifica latência. |
| Texto integral de Verhelst & Roelands (1993) e de Driedger & Müller (2016) | **[✗] Não extraído.** PDFs digitalizados/protegidos. Só metadados e resumos foram obtidos. As afirmações sobre *look-ahead* do WSOLA na §3.1 são, portanto, **[D]** e não citação. |

---

## 1. Frente 1 — Percepção de latência em monitoração vocal e a latência real do Auto-Tune

### 1.1 A origem provável do "20–30 ms"

O comentário de código do protótipo e a meta do RNF01 usam o intervalo 20–30 ms sem fonte.
Esse fraseado aparece, **quase literalmente**, em material de marketing da própria Antares:

> "At small amounts (under 10 ms), latency is imperceptible. Most people cannot detect a
> delay that small. At 20 to 30 ms, it starts to feel slightly off, like a very mild echo.
> Above 40 ms, the delay becomes genuinely disruptive."
> — Antares, *Low Latency Monitoring in DAWs* [V] (blog do fabricante, sem citação de estudo)

**Status:** isso é **[V]**, não **[P]**. É um texto comercial sem referência a experimento. Ele
serve como registro de "o que a indústria diz", nunca como fundamentação de um requisito
não funcional em trabalho acadêmico.

> **Recomendação para o texto do TCC:** substituir a meta "≤ 20–30 ms" por uma meta ancorada
> nos dados de Lester & Boley (2007) e Marentakis et al. (2012), explicitando **qual** limiar
> está sendo perseguido. Isso resolve a correção pendente nº 3 da §10 da documentação técnica.

### 1.2 O que a literatura mede — e a distinção entre os limiares

Os números da literatura **não são intercambiáveis**. Há pelo menos quatro grandezas
distintas frequentemente confundidas:

| Tipo de limiar | Pergunta que responde | Ordem de grandeza típica |
|---|---|---|
| **Detecção / discriminação (JND)** | "a partir de quanto o ouvinte *nota* a diferença?" | dezenas de ms (Schmid et al., 2024) |
| **Coloração** | "a partir de quanto o timbre muda por filtro-pente?" | ~10 ms para voz com in-ear (Marentakis et al., 2012) |
| **Preferência / incômodo** | "a partir de quanto o músico *reclama*?" | 1–26 ms conforme instrumento e monitor (Lester & Boley, 2007) |
| **Degradação de desempenho** | "a partir de quanto a execução *piora*?" | ~11,5 ms para andamento em conjunto (Chafe & Gurevich, via Lester & Boley) |

#### 1.2.1 Lester & Boley (2007) — AES 123ª Convenção, paper 7198 **[P]**

Estudo subjetivo com **19 músicos praticantes** (11 profissionais), **6 instrumentos** (voz,
saxofone, guitarra elétrica, teclado, baixo elétrico, bateria), **dois mecanismos de
monitoração** (monitor de chão a 4–6 pés do ouvido, e in-ear) e duas situações (solo e com
metrônomo não atrasado). O método foi um **MUSHRA adaptado ao ao vivo** ("live pseudo-MUSHRA"),
com escala contínua de 0–100 dividida em cinco faixas nomeadas (Excellent / Good / Fair /
Poor / Bad). O menor atraso realizável no aparato era **1,4 ms** (throughput do mixer digital),
o que os autores chamam de "0 ms digital".

Do resumo, verbatim:

> "the audibility of latency is dependent on both the type of instrument and monitoring
> environment"; acceptable latency ranged "from 42ms to possibly less than 1.4ms".

**Tabela 1 do artigo** — comparação entre instrumentos no **nível de confiança de 85%**
(limite inferior do intervalo de confiança sobre ouvintes críticos, isto é, um valor
deliberadamente **conservador / pior caso**), em milissegundos:

| Limiar | Sax | **Voz** | Guitarra | Bateria | Baixo | Teclado |
|---|---|---|---|---|---|---|
| **IEM Good** | 0 | **1** | 4,5 | 8 | 4,5 | 27 |
| **Wedge Good** | 1,5 | **10** | 6,5 | 9 | 8 | 22 |
| **IEM Fair** | 3 | **6,5** | 14,5 | 24,5 | 25,5 | 43 |
| **Wedge Fair** | 10 | **26** | 16 | 25 | 30 | 40,5 |

> ⚠️ **Ressalva de extração.** Esta tabela foi extraída programaticamente do PDF do artigo. A
> ordem das colunas e os valores conferem com o texto corrido do artigo, mas **antes de citar
> na banca o autor deve conferir a Tabela 1 no PDF original** (a numeração das figuras no PDF
> tem inconsistências internas — as Figs. 26–31 são referenciadas como "Figs. 26-31" em um
> ponto e "Figs. 32-35" em outro).

Números adicionais do mesmo artigo, para casos individuais em **monitor de chão sem
metrônomo** (Seção 3.1, sujeito único): baixo elétrico ~18 ms (Good) e ~28 ms (Fair);
teclado ~30 ms (Good) e ~43 ms (Fair).

#### 1.2.2 Marentakis, Kranzler, Frank, Opitz & Sontacchi (2012) — DAGA **[P]**

Experimento com **10 cantores em atividade**, usando in-ear AKG IP2 e headset DPA 4088F,
comparando cinco sistemas de monitoração (do sinal seco ao sinal convolvido com resposta
impulsiva binaural + reverberação) em **latências de 4, 7, 10, 13 e 16 ms** (mais 1 ms para
o sistema seco), contra uma referência analógica de latência zero. O sinal de monitoração
estava a **+10 dB** em relação ao sinal acústico medido na cabeça artificial.

Resultados relevantes, verbatim do artigo:

> "T-tests showed that Systems 4 and 5 at all latencies and **System 1 at latencies of 4, 7
> and 10 ms were not different than the reference**." (preferência)

> "System 1 did not [colorate the sound more than the reference] **at 1, 4, 7 ms but did at
> higher latencies**." (coloração)

> "Both techniques seem to yield **acceptable preference up to about 13 ms**." (com informação
> de sala adicionada)

**Interpretação.** Para o caso mais próximo do nosso (monitoração in-ear **seca**, que é o que
um plugin de autotune entrega): a **preferência** não se degrada até 10 ms; a **coloração**
já é significativa a partir de 13 ms (e mede-se "não diferente da referência" só até 7 ms).
Isto é: o **limiar de coloração** está entre 7 e 13 ms, não em 20–30 ms.

O artigo também registra um **máximo local de preferência** entre 4 e 7 ms — ou seja, uma
latência pequena pode ser *preferida* a zero, porque simula a reflexão de um monitor de chão.

#### 1.2.3 Fontes citadas por Marentakis que reforçam o quadro **[P-2ª]**

- **Noson, Sato, Sakai & Ando (2002)**, *Journal of Sound and Vibration* 258(3):473–485 —
  cantores em câmara anecoica **rejeitaram** o canto sem reflexão e **preferiram um atraso de
  10–20 ms a −5 dB** em relação ao som direto. *Não li o original; a afirmação vem do texto de
  Marentakis et al.*
- **Marshall, Gottlob & Alrutz (1978)**, *JASA* 64(5):1437–1442 — preferência forte por
  reflexões a −14 dB e 20 ms. *Idem.*
- **Ando (1977)**, *JASA* 62(6):1436–1441 — preferência subjetiva em campos sonoros com eco
  único. *Idem.*

> **Cuidado importante.** Esses três medem **reflexões acústicas atenuadas**, não o caminho de
> monitoração. Numa reflexão a −5 dB ou −14 dB o direto domina; num in-ear a **+10 dB** o
> atrasado domina. **Não** se pode usar "cantores preferem 10–20 ms" para justificar 20 ms de
> latência de plugin. Marentakis et al. levantam exatamente essa diferença como a lacuna que o
> estudo deles preenche.

#### 1.2.4 Chafe & Gurevich — o "ponto doce" de 11,5 ms **[P-2ª]**

Citado dentro de Lester & Boley (2007):

> "the sweet spot for musicians to play with each other in tempo is 11.5ms. […] a delay of
> less than 11.5ms caused 74% of musicians to speed up, and a delay of more than 11.5ms
> caused 85% of the musicians to slow down."

Este é um **limiar de degradação de desempenho** (deriva de andamento em conjunto), não de
incômodo, e não é sobre auto-monitoração. *Não li o original.* Referência conforme Lester &
Boley: Chafe, C. & Gurevich, M., *Network Time Delay and Ensemble Accuracy*.

#### 1.2.5 Schmid, Ambros, Bogon & Wimmer (2024) — Audio Mostly **[P]**

JND de latência de áudio pelo método PEST, n = 37:

> "Participants achieved a mean JND of **49 ms** for a base latency of 0 ms, **27 ms** for a
> base latency of 64 ms, and **77 ms** for a base latency of 512 ms. Furthermore, the JND was
> lower for participants with high musical sophistication."

> ⚠️ **Não use este número para o RNF01.** É um limiar de **discriminação entre duas latências**
> (o quanto a latência precisa mudar para que se note a mudança), não o limiar absoluto de
> percepção nem o de incômodo, e a tarefa não é auto-monitoração vocal. Citar 49 ms como
> "tolerância" seria exatamente o tipo de conflação que a banca detecta.

### 1.3 Cantores são mais sensíveis que instrumentistas? — o que os dados realmente dizem

A afirmação **como regra geral é [✗] não sustentada** pelos dados que encontrei: na Tabela 1
de Lester & Boley o **saxofone** tem limiares menores que a voz em todas as quatro condições
(embora os próprios autores registrem variância alta e peçam mais sujeitos: "Wedge, Good:
Mean = 7.9, σ = 8.5", etc.).

O que **é** sustentado, e é uma afirmação melhor:

1. **Voz é o único instrumento cujo limiar "Fair" com in-ear é *menor* que o limiar "Good"
   com monitor de chão.** Verbatim: *"vocalists tend to have a lower latency threshold for a
   'Fair' IEM mechanism than a 'Good' Wedge monitor system. This is in contrast to the results
   from other instruments."* Ou seja: **o cantor é excepcionalmente sensível ao *mecanismo* de
   monitoração**, não apenas à latência absoluta.
2. **Cantores têm "alta sensibilidade geral"**, isto é, mudam de opinião sobre a latência
   conforme muda o contexto: *"Vocalists have a high general sensitivity and are thus more
   likely to change their perception of latency based on variable change. This means that the
   thresholds described in Section 4.3 are not precise in a wide variety of situations."*
3. A explicação proposta pelos autores é a ausência de reverberação de sala no in-ear: com
   monitor de chão a latência "se mistura" com a reverberação natural, que o cantor espera
   ouvir; com in-ear essa expectativa se quebra. Marentakis et al. (2012) **testaram e
   confirmaram** essa hipótese: adicionar informação de sala ao in-ear eleva a preferência
   aceitável para ~13 ms.
4. **Para in-ear e no pior caso, voz e sax são os mais críticos e teclado o menos crítico** —
   uma faixa de 1 ms a 27 ms para o mesmo limiar ("Good"). Isso, sim, é citável.

> **Nota sobre condução óssea.** A explicação popular de que "o cantor ouve a própria voz por
> condução óssea, então o sinal atrasado forma filtro-pente" é fisicamente plausível e aparece
> em textos de divulgação, **mas não encontrei fonte primária que a meça em contexto de
> monitoração com latência**. Marentakis et al. modelam o problema como superposição do sinal
> atrasado com "o sinal no caminho acústico" — medido em cabeça artificial, portanto via ar,
> **não** por condução óssea. **[✗]** Se o TCC quiser afirmar condução óssea, precisa de fonte
> própria ou precisa marcar como hipótese.

### 1.4 Realimentação auditiva atrasada (DAF) — regime errado, cuidado ao citar

A literatura de DAF é abundante e tentadora, mas opera em **outra escala temporal**:

- O efeito disruptivo clássico do DAF sobre a fala tem **máximo em torno de 200 ms**
  (Pfordresher & Dalla Bella, 2011, *JEP:HPP*) **[P-2ª]** — ordem de grandeza 10× maior que a
  latência de um plugin.
- Pfordresher & Dalla Bella (2011) e Pfordresher & Mantell (2012, *Acta Psychologica*)
  mostram que **feedback assíncrono degrada o *timing*** enquanto **alteração de *altura* do
  feedback degrada a *acurácia de notas***, e que a manipulação de altura afeta **mais o canto
  do que o teclado**.

**Implicação para o TCC.** Um autotune ao vivo faz **as duas coisas ao mesmo tempo**: atrasa
*e* altera a altura do feedback. A literatura de DAF sustenta que a alteração de altura é
especialmente danosa para cantores — o que é um argumento **a favor** de C4 (mistura
seco/molhado) e de C1 (correção suave) **independente da latência**. Mas ela **não** fornece
o limiar de 20–30 ms, e citá-la para isso seria erro.

> **[✗] Não encontrei** estudo de DAF que meça o limiar de perturbação para atrasos abaixo de
> ~50 ms em canto. Esse é um vazio real da literatura, e vale registrá-lo no TCC como tal —
> é uma justificativa forte para o teste perceptual próprio proposto na §9.4.

### 1.5 A latência oficial declarada pela Antares

**Fonte primária: manual do produto AutoTune 2026, seção "Mode"** (documentação oficial
Antares) **[P]**, verbatim:

> "There are two **Latency Modes** available in AutoTune:
> **Low Latency** mode is optimized for minimal latency during live performances. **At 48kHz,
> Modern Mode reports 112 samples (2.3ms) and Classic Mode reports 37 samples (0.77ms).**
> **HQ (High Quality)** mode provides more transparent Formant Correction, which results in a
> more natural sound when the pitch correction applied is larger. Note: HQ processing is only
> available in Modern Mode."

**Veredito sobre a crença do autor ("~3 ms"):** **confirmada em ordem de grandeza**, com três
qualificações que precisam ir para o texto:

1. O número é **2,3 ms**, não 3 ms, e vale para **Modern Mode**. O Classic Mode é ainda menor
   (0,77 ms).
2. O número é declarado **a 48 kHz**. A 44,1 kHz, se a contagem de amostras for a mesma, seria
   2,54 ms / 0,84 ms — mas **a Antares não afirma isso** e essa conversão é **[D]**, não citação.
3. É a latência **reportada ao host** (compensação de atraso do plugin, PDC), em **Low Latency
   Mode**. A latência do **modo HQ não é publicada** **[✗]**.

Manuais de **Auto-Tune Pro 11**, **Auto-Tune EFX+ 10** e **Auto-Tune Hybrid** **[P]** só
registram a existência da opção, sem número: *"If you plan to use AutoTune Pro in a live
performance or monitor through it in real time while recording, turn on Use Low Latency to
minimize processing delay."*

**Concorrente relevante — Waves Tune Real-Time [P]** (tabela de especificação da página oficial
do produto): latência declarada como **"Zero or near-zero latency (depending on pitch)"** a
44,1 / 48 / 88,2 / 96 kHz. A expressão *"depending on pitch"* é reveladora: a latência do
produto é **função do período do sinal**, o que é assinatura de uma arquitetura **síncrona com
o período** — exatamente a família discutida na §3.4.

### 1.6 Consequências para L1–L7 e para o RNF01

| Item | Como a bibliografia o afeta |
|---|---|
| **RNF01 (meta ≤ 20 ms)** | **Meta subdimensionada, mas por um motivo bom de discutir.** 20 ms fica acima do limiar de coloração para in-ear seco (7–13 ms, Marentakis et al.) e muito acima do limiar "Good" para voz com in-ear (~1 ms, Lester & Boley). Proposta: adotar **duas metas** — "aceitável" (≤ 13 ms, ancorado em Marentakis) e "alvo" (≤ 7 ms). |
| **L1 + L2 + L3 → 17,3 ms** | **Ainda não atinge o limiar de coloração para in-ear.** O resultado continua sendo um avanço grande (57,9 → 17,3 ms), mas o texto **não pode** declarar RNF01 cumprido com base em 20 ms sem justificar de onde vem o 20. |
| **L5 (preset "Low Latency")** | **Fortemente apoiado**: espelha uma prática documentada do fabricante líder [P], e Lester & Boley mostram que o limiar aceitável **depende do mecanismo de monitoração** — logo, expor o trade-off ao usuário é a decisão de projeto correta, não uma desculpa. |
| **L6 (rastreio contínuo)** | Passa de "pesquisa arriscada" para **caminho conhecido** — ver §3.4. É o único item do backlog que leva o protótipo à faixa de 7–13 ms. |
| **L7 (medição de round-trip)** | **Continua indispensável**, e agora com alvo definido: comparar o número medido com os **2,3 ms declarados** pela Antares [P] e com o **17,3 ms algorítmico** do protótipo. |
| **§10, correção nº 3** | **Resolvida** — mas resolvida no sentido de *refutar* o número, não de confirmá-lo. |

---

## 2. Frente 2 — Retune Speed × preservação de vibrato

### 2.1 Antares: três controles distintos, documentados separadamente **[P]**

O ponto central desta seção é que a Antares **não** trata "velocidade de correção" e "zona
morta" como o mesmo controle. São três parâmetros com descrições oficiais distintas.

#### Retune Speed — **limite em taxa**, unidade em milissegundos

Manual do **AutoTune 2026**, verbatim:

> "Retune Speed controls **how rapidly the pitch correction is applied** to the incoming
> audio. Setting the Retune Speed to **0 milliseconds** will cause immediate changes from one
> pitch to another, and will **completely suppress any vibrato or deviations in pitch**. This
> is best for recreating the iconic 'AutoTune Effect'. **Longer Retune Speeds decrease how
> rapidly corrections are made, and allow more vibrato and other interpretive pitch gestures
> to pass through.** For more natural sounding pitch correction, set Retune Speed **between 10
> and 50 milliseconds**."

Manual do **Auto-Tune Pro 11**, verbatim, com a unidade explícita:

> "Retune Speed controls how rapidly the pitch correction is applied to the incoming audio.
> **(Units are in milliseconds.)** […] For more natural sounding pitch correction, set between
> 10 and 50. Larger values allow more vibrato and other interpretive pitch gestures, but
> decrease how rapidly corrections are made."

Manual do **Auto-Tune Hybrid**, verbatim: *"The units are milliseconds."* Mesma redação.

→ **Isto é literalmente a definição funcional usada na §8.2, Quantização 4, e é exatamente o
que C1 propõe.** Agora com citação.

#### Flex-Tune — **zona morta em amplitude** (o que o protótipo de fato implementou)

Manual do **Auto-Tune Pro 11**, verbatim:

> "The Flex-Tune control allows you to preserve a singer's expressive vocal gestures, while
> still correcting an out of tune vocal. When Flex-Tune is set to 0, AutoTune pulls every
> incoming note toward a target scale note. When Flex-Tune is engaged, **it only applies
> correction as the performer approaches the target note**. As you move the control toward
> higher values, **the correction area around the scale note gets smaller**, and more
> expressive pitch variation is allowed through."

→ **Este é o mecanismo da `tol` do `notaAlvo()` em `dsp.h:159`.** A conclusão da §8.2 precisa
ser refinada: o protótipo não implementou "o mecanismo errado"; implementou **o Flex-Tune** e
**não** implementou o Retune Speed, chamando o glide de Retune Speed. Os dois coexistem no
produto comercial e são complementares.

#### Humanize — Retune Speed dependente da duração da nota

Manual do **AutoTune 2026**, verbatim:

> "Humanize helps you to add realism to sustained notes when using fast Retune Speeds. […]
> In order to get the short notes in tune, you would need to set a fast Retune Speed, but this
> can cause **sustained notes to sound unnaturally static**. Humanize **applies a slower Retune
> Speed only during the sustained portion of longer notes**, making the overall performance
> sound both in tune and natural."

→ Diretamente relevante para o sintoma "duro/estático" relatado no teste de usuário. Sugere
uma extensão natural de C1 (chamemos **C1-b**): tornar a constante de tempo da correção
**função do tempo desde o ataque**. Rápida no ataque (afina o começo da nota), lenta no
sustentado (deixa o vibrato passar).

### 2.2 Waves Tune Real-Time **[P]** / **[V]**

Da tabela de especificação oficial do produto **[P]**: o plugin expõe **Speed** e **Note
Transition** como controles principais, mais **Tolerance** e **Range**, e lista
*"Natural vibrato detection & correction: Yes"* como recurso.

De artigo instrucional da própria Waves **[V]** (conteúdo de fabricante, não revisado):
o valor padrão de **Note Transition é 120 ms**; reduzi-lo "para cerca de 20 ms" produz
quantização de altura audível; "cerca de 45 ms" é o meio-termo sugerido. Um controle de
preservação de modulação vai de 0% ("quantiza completamente") a 100% ("deixa passar toda a
modulação de altura"), com ~75% sugerido.

→ Confirma que o **intervalo de constantes de tempo relevante para naturalidade é da ordem de
dezenas a ~120 ms**, coerente com os 10–50 ms da Antares. Útil para dimensionar o parâmetro
de C1.

### 2.3 Celemony Melodyne **[P]** — a separação explícita rápido/lento

A documentação oficial da Celemony faz a distinção **exatamente** nos termos que fundamentam
C1 — e nomeia os dois regimes:

> "**Pitch modulation** covers **rapid and usually intentional** variations in pitch such as
> trills or vibrato." / "**Pitch drift** [is] **slow fluctuations** in pitch of the kind that
> are usually **unintentional and symptomatic of poor technique**."

E, na macro *Correct Pitch*, o comportamento resultante, verbatim:

> "More rapid fluctuations in pitch, such as pitch modulation or vibrato, **remain unaffected**"
> [pelo controle *Correct Pitch Drift*].

→ **É a melhor formulação conceitual disponível em documentação de fabricante para justificar
C1**: *expressão é rápida, desafinação é lenta*. Ressalva metodológica: o Melodyne é
**offline** (edição por objetos de correção), portanto não é evidência de que a separação seja
viável em tempo real — o Auto-Tune e o Waves Tune Real-Time é que são.

### 2.4 GSnap **[✗]** e Autotalent **[P]** — o que o código aberto realmente faz

- **GSnap (GVST)**: **[✗] fonte não disponível.** É freeware de binário fechado; a página
  oficial de download não menciona código-fonte nem licença livre, e não há repositório
  público. **Não é fonte primária utilizável.** Só o manual do usuário é acessível.
- **Autotalent (Tom Baran, GPL-2)**: **[P] fonte disponível.** Este é o exemplar de código
  aberto que substitui o GSnap na argumentação. A porta VST/AU mantida por M. Donovan
  (`AutoTalent.cpp`) contém, na seção de baixa taxa:

```c
// Glide on circular scale
tf3 = (float)ptarget - sptarget;
tf3 = tf3 - (float)12*floorf(tf3/12 + 0.5);
if (fGlide>0) { tf2 = (float)1-pow((float)1/24, (float)N*1000/(noverlap*fs*fGlide)); }
else          { tf2 = 1; }
sptarget = sptarget + tf3*tf2;              // <- filtro de 1 polo sobre o ALVO
...
// ---- Determine correction to feed to the pitch shifter ----
tf = sptarget - pitch;                      // <- correção = alvo suavizado - altura real
tf = tf - (float)12*floorf(tf/12 + 0.5);
if (conf<vthresh) { tf = 0; }
lrshift = fShift + fAmount*tf;              // <- fAmount é mistura em AMPLITUDE
```

**Leitura.** O Autotalent **filtra o alvo** (`sptarget`), exatamente como o protótipo faz
hoje, e depois calcula a correção como `sptarget − pitch`. Com `fAmount = 1` (100%) a saída é
`pitch + (sptarget − pitch) = sptarget` — **o vibrato é destruído**. O parâmetro `Amount` é
uma **mistura proporcional**, não um limite em taxa: a saída fica
`(1−A)·pitch + A·sptarget`, o que preserva uma fração `1−A` do vibrato **e ao mesmo tempo**
deixa passar a mesma fração `1−A` da desafinação. Não separa expressão de erro.

→ **Achado com valor direto para o TCC:** o principal autotune de código aberto tem
**exatamente a mesma limitação** que o teste de usuário apontou no protótipo. Isso reposiciona
o problema: não é um defeito de implementação do TCC, é uma **lacuna comum ao software livre
de correção de altura**. Excelente material para a introdução do capítulo de discussão.

### 2.5 A patente do Auto-Tune (Hildebrand, US 5.973.252 A) **[P]** — a formulação exata

Fonte de primeira mão sobre como o Auto-Tune realmente implementa o Retune Speed. Verbatim:

> "The variable **Resample_Raw_Rate** is computed at logic step 66 by dividing **Cycle_period**
> […] by **desired_Cycle_period**. […] If Resample_Raw_Rate were used to resample the data, the
> pitch of the output would be precisely in tune with the desired pitch […]. Because the
> desired pitch will change instantaneously to a different scale note or a different MIDI note,
> the output pitch would also change instantaneously. **This is objectionable when the human
> voice is being processed, because the voice does not instantly change pitch.** The computation
> for **Resample_Rate1** of logic step 67 **smooths out Resample_Raw_Rate**, alleviating this
> problem. The variable, **Decay**, is between zero and one and is set by the user. A value of
> zero causes Resample_Rate1 to equal Resample_Raw_Rate, giving instantaneous pitch changes. A
> value close to one causes a lot of smoothing, making pitch changes gradual."

**Isto é decisivo para C1.** `Resample_Raw_Rate = Cycle_period / desired_Cycle_period` **é a
razão de correção** — o quociente entre o período medido e o período desejado. Filtrar essa
razão com um filtro exponencial de coeficiente `Decay` é, em domínio logarítmico,
**exatamente** um filtro de 1 polo sobre `alvoCents − realCents`. **[D]** (a equivalência
log/linear é derivação minha; a patente não a enuncia).

**Veredito sobre a novidade de C1:**

| Pergunta | Resposta |
|---|---|
| A ideia "filtrar a correção em vez do alvo" é nova? | **Não.** Está na patente do Auto-Tune de 1997 [P] e é o comportamento documentado do Retune Speed [P]. |
| Está descrita em literatura acadêmica revisada por pares? | **[✗] Não encontrada.** Nenhum artigo localizado enuncia a formulação. |
| Está em alguma implementação de código aberto que eu tenha lido? | **Não.** O Autotalent [P] filtra o alvo. |
| Então C1 tem valor para o TCC? | **Sim, e maior do que antes.** Deixa de ser "uma ideia minha que talvez funcione" e passa a ser **replicação fundamentada de um mecanismo patenteado e documentado, aplicada a um pipeline pYIN+TD-PSOLA**, com **verificação quantitativa própria** (a "resposta em frequência da correção" da §9.4). *Essa* medida — razão de preservação de vibrato por banda — é que não achei publicada em lugar nenhum. |

> **Nota de propriedade intelectual, não jurídica.** A US 5.973.252 foi depositada em 1997 e
> concedida em 1999; patentes de utilidade nos EUA têm prazo de 20 anos a partir do depósito,
> o que coloca a expiração em ~2017–2018. **Não sou fonte jurídica**; se o texto do TCC for
> mencionar isso, deve fazê-lo em nota de rodapé cautelosa.

### 2.6 Literatura acadêmica sobre correção que preserva expressão

Achados **[P]**:

- **Wager, Tzanetakis, Wang & Kim (2020)**, *Deep Autotuner: a Pitch Correcting Network for
  Singing Performances*, ICASSP 2020 — o modelo **prediz o deslocamento de altura em cents**
  (isto é, **a correção**, não o alvo) por nota, e é treinado tanto em desafinação (que aprende
  a corrigir) quanto em **variação intencional de altura (que aprende a preservar)**. Trata
  altura como **valor contínuo**, não quantizado à grade da partitura.
- **Wager et al. (2019)**, arXiv:1902.00956 — versão anterior, com o corpus de **4.702**
  execuções de karaokê amadoras.

→ **Convergência importante:** o estado da arte em aprendizado de máquina **também** modela a
tarefa como *"prever uma correção lentamente variável a somar à trajetória real"*, e **também**
identifica "preservar variação intencional" como o critério de naturalidade. Isso valida a
formulação de C1 num segundo eixo, independente do Auto-Tune.

**[✗] Não encontrei** artigo revisado por pares que meça objetivamente **preservação de
vibrato** em corretores de altura por uma razão de amplitude de modulação por banda de
frequência. Se a métrica proposta na §9.4 não existir mesmo, ela é a **contribuição
metodológica mais defensável do trabalho** — mais do que C1 em si.

### 2.7 Parâmetros de vibrato — dimensionando a constante de tempo de C1

Fontes:

| Fonte | Achado | Confiança |
|---|---|---|
| **Prame (1994)**, *JASA* 96(4):1979–1984 | Dez cantores executando o *Ave Maria* de Schubert, medidos em gravações comerciais. **Taxa média entre cantores: 6,0 Hz.** Variação intra-artista de cerca de **±8%** em torno da própria média. A taxa **aumenta ~15% no fim de cada nota**. | **[P-2ª]** (resumo JASA obtido por busca; PDF não acessado) |
| **Prame (1997)**, *JASA* 102(1):616–621 | **Extensão média de ±71 cents**, com correlação negativa com a duração da nota. | **[P-2ª]** (não consegui acessar o resumo original; ver ressalva abaixo) |
| **Sundberg (1994)**, STL-QPSR 35(2–3):45–68 | Em música lírica ocidental, vibrato tipicamente com **taxa de 5–8 Hz** e **extensão menor que ±1 semitom** (≈ ±100 cents). | **[P-2ª]** (o servidor do KTH retornou erro 500 no momento da consulta; **o autor deve baixar o PDF e conferir antes de citar**) |

> ⚠️ **Ressalva séria.** Dos três, **nenhum PDF foi lido integralmente**. As referências
> bibliográficas (autor, ano, periódico, volume, páginas) estão corretas e conferidas em
> registro de periódico; **os valores numéricos vieram de resumos e de citações em terceiros**.
> Antes da defesa, obtenha os PDFs. O intervalo qualitativo — **vibrato de canto lírico entre
> 5 e 8 Hz, com extensão de algumas dezenas de cents** — é consistente entre todas as fontes e
> é seguro; os valores pontuais precisam de conferência.

#### Dimensionamento do filtro de C1 **[D]** — derivação, não citação

Com a formulação de C1, `saida = real + LP(alvo − real)`. Se o alvo é constante dentro da
nota, a componente de vibrato da saída é `real − LP(real) = HP(real)`, onde HP é o
complementar do filtro de 1 polo. Para um polo com frequência de corte *f_c*, o ganho sobre
uma modulação de frequência *f_v* é:

```
G(f_v) = f_v / sqrt(f_v² + f_c²)          (fração do vibrato que sobrevive)
```

| *f_c* | τ = 1/(2π·f_c) | G a 6 Hz | Leitura |
|---|---|---|---|
| 8,0 Hz | 20 ms | 0,60 | 60% do vibrato passa — "Retune Speed 20 ms" da Antares |
| 3,2 Hz | 50 ms | 0,88 | 88% — extremo natural da faixa 10–50 ms |
| 1,6 Hz | 100 ms | 0,97 | 97% — vibrato praticamente intacto |
| 1,3 Hz | 120 ms | 0,98 | ≈ *Note Transition* padrão do Waves Tune Real-Time |
| 0,16 Hz | 1 s | 0,9997 | correção só de deriva lenta |

**Consequência prática para a implementação de C1:** a meta da §9.4 ("razão ≥ 0,8 na banda
4–8 Hz e ≈ 0 abaixo de 1 Hz") exige **f_c ≲ 3 Hz**, isto é, **τ ≳ 50 ms**, e a faixa útil do
controle deve ir de 0 (efeito Auto-Tune) até pelo menos ~200 ms. Note que o requisito
"≈ 0 abaixo de 1 Hz" e "≥ 0,8 em 4–8 Hz" **não são simultaneamente satisfazíveis com um único
polo**: em f_c = 3 Hz, G(1 Hz) = 0,32, não 0. Ou a meta da §9.4 se afrouxa, ou C1 precisa de
**filtro de ordem maior** — o que é um resultado técnico honesto e citável do próprio trabalho.

### 2.8 Consequências para C1–C5

| Item | Como a bibliografia o afeta |
|---|---|
| **C1** | **Confirmado e fundamentado** (Antares [P], patente Hildebrand [P], Melodyne [P], Wager et al. [P]). **Deixa de ser contribuição original** — reposicionar como replicação fundamentada. Adicionar **C1-b** (constante de tempo variando com o tempo desde o ataque), inspirado no *Humanize* [P]. Dimensionamento: τ ≳ 50 ms, com faixa até ~200 ms **[D]**. |
| **C2** (resolução de 20 cents) | **Apoiado indiretamente**: com extensão de vibrato de dezenas de cents [P-2ª], uma grade de 20 cents descarta uma fração grande do gesto. Wager et al. [P] argumentam explicitamente por tratar altura como valor contínuo. Reforça a **Opção B**. |
| **C3** (interpolação entre quadros) | Sem fonte específica encontrada; é boa prática de DSP. **Não citar como fundamentado na literatura.** |
| **C4** (mistura seco/molhado) | **Apoiado obliquamente** pela literatura de DAF: alteração de altura no feedback degrada acurácia de canto (Pfordresher & Dalla Bella, 2011 [P-2ª]). Um controle que reduz a alteração no monitor tem justificativa perceptual, não só estética. |
| **C5** (reconhecer o escopo) | **Apoiado**: os manuais Antares [P] documentam *Throat Length*, *Formant*, *Humanize* e *Vibrato Controls* como processos **adicionais** e separados da correção de altura. A fronteira que a §8.2 propõe traçar é a fronteira que o próprio fabricante traça. |

---

## 3. Frente 3 — Arquiteturas de baixa latência

### 3.1 WSOLA — o que a fonte primária sustenta, e o que não sustenta

**Referência primária:** Verhelst, W. & Roelands, M. (1993), *An overlap-add technique based on
waveform similarity (WSOLA) for high quality time-scale modification of speech*, ICASSP-93,
pp. 554–557, DOI 10.1109/ICASSP.1993.319366. Artigo complementar: Roelands & Verhelst (1993),
Eurospeech, pp. 337–340, DOI 10.21437/Eurospeech.1993-59.

**O que consegui verificar [P]:** os registros bibliográficos e os resumos. O resumo do ICASSP
afirma que o algoritmo *"produces high-quality speech output, is algorithmically and
computationally efficient and robust, and allows for **online processing** with arbitrary
time-scaling factors"*. O do Eurospeech menciona *"revealing the versatile possibilities for
**on-line operation**"*.

**[✗] O que NÃO consegui verificar:** o texto integral. Ambos os PDFs disponíveis são
digitalizações sem camada de texto e a extração falhou. **Portanto, não tenho citação para
nenhum número de *look-ahead*.**

**Análise [D] — apresentar como raciocínio, não como citação.** O WSOLA substitui a detecção de
marcas de período por uma **busca de similaridade** dentro de uma região de tolerância Δ em
torno da posição ideal do próximo segmento. Para escolher o melhor segmento é preciso ter
disponível o sinal em toda essa região; logo o buffer necessário é da ordem de
`(janela/2 + Δ)`, e Δ precisa cobrir pelo menos um período do tom mais grave para que a busca
possa alinhar formas de onda. Isso é **da mesma ordem** do que o TD-PSOLA precisa para
posicionar o grão seguinte.

**Resposta à pergunta do briefing:** com base no que é verificável, **abandonar as marcas de
período reduz a *complexidade* e a *fragilidade* (não há mais o que errar na marcação), mas
não há evidência de que reduza a *latência***, porque a tolerância de busca ocupa o mesmo
espaço temporal que as marcas ocupavam. Recomendo que o TCC afirme **exatamente isso**, com a
ressalva de que a fonte primária não foi lida integralmente.

> **Para fechar essa lacuna:** obter o PDF do ICASSP-93 via portal institucional (IEEE Xplore
> pela PUCRS) e verificar a definição de Δ. É uma tarefa de 20 minutos que fecha um buraco
> real na fundamentação de L3/L6.

### 3.2 Vocoder de fase

**Referências primárias:**
- Flanagan, J. L. & Golden, R. M. (1966), *Phase Vocoder*, Bell System Technical Journal
  45(9):1493–1509 — a formulação original.
- Laroche, J. & Dolson, M. (1999), *Improved phase vocoder time-scale modification of audio*,
  IEEE Trans. Speech and Audio Processing 7(3):323–332 — travamento de fase e redução de
  "phasiness".
- Laroche, J. & Dolson, M. (1999), *New phase-vocoder techniques for pitch-shifting,
  harmonizing and other exotic effects*, IEEE WASPAA.

**Status:** **[P]** para os registros bibliográficos; **[✗]** não consegui extrair o texto
integral de nenhum dos três (todos atrás de paywall ou com PDF não extraível).

**Análise [D].** A latência inerente do vocoder de fase é **estrutural e conhecida**: a
resolução em frequência exige janela de análise longa. Para separar parciais de um sinal com
F0 = 175 Hz (contralto), é preciso resolução de bin melhor que 175 Hz, o que a
`fs/N` de uma FFT impõe: `N > 44100/175 ≈ 252` amostras para *tocar* a resolução, e na prática
**2 a 4 vezes isso** para que a janela (com apodização) separe parciais adjacentes — ou seja,
**512 a 1024 amostras (12–23 ms) só de janela**, mais o *hop*. Isso é da **mesma ordem ou pior**
que o quadro de análise do CMNDF que o protótipo já usa.

**Conclusão para o texto:** o vocoder de fase **não é rota de redução de latência**; é rota de
robustez para material polifônico e de qualidade em fatores de esticamento extremos. Vale
como comparação no capítulo teórico, **não** como candidato a substituir o TD-PSOLA no
requisito RNF01. Isso confirma a intuição já registrada na §12 ("Leituras que faltam").

### 3.3 Pitch shifting por linha de atraso modulada

**Referência primária localizada e lida [P]:** Disch, S. & Zölzer, U. (1999), *Modulation and
delay line based digital audio effects*, Proc. DAFx-99, Trondheim, pp. 5–8.

Verbatim, da seção 5 (*Pitch Transposing*):

> "In this section an enhanced method for transposing audio signals is proposed. The method is
> based on an **overlap-add scheme** and **does not need any base frequency estimation**. […]
> The enhanced transposing system is based on an overlap-add scheme with **three parallel time
> varying delay lines**. […] Adjacent blocks overlap with **2/3 of the block length**. The
> modulation signals form a system of three **120° phase shifted raised cosine functions**. The
> sum of these functions is constant for all arguments."

Sobre os **artefatos**, verbatim:

> "The amplitude modulation only produces sum and difference frequencies with the base
> frequency of the modulation signal, which can be very low (**6-10 Hz**). Harmonics are not
> present in the modulation signal and hence cannot form sum or difference frequencies of
> higher order. **The perceived artifacts are phasing like effects** and are less annoying than
> local discontinuities of other applications based on twofold overlap-add methods."

**Nota corretiva importante.** A referência que a §12 da documentação técnica sugeria
(Dattorro, *Effect Design Part 2*, JAES 45(10), 1997) **não trata de pitch shifting** — li o
artigo e ele cobre interpolação de linha de atraso, chorus, flange e vibrato, sem seção de
transposição. **Trocar a referência por Disch & Zölzer (1999).**

**Resposta à pergunta do briefing:**

| Pergunta | Resposta |
|---|---|
| Qual o piso de latência real dessa arquitetura? | **[✗] O artigo não declara latência.** **[D]** O que ele *permite* inferir: como não há estimação de frequência-base, **não existe piso de detecção** — some o termo `2·fs/FMIN`. A latência residual é o **deslocamento médio da linha de atraso**, necessário para que o ponteiro de leitura possa varrer o bloco sem ultrapassar o de escrita, portanto da ordem do **comprimento do bloco de sobreposição**, que é escolha de projeto, não requisito físico do sinal. |
| Quais artefatos? | **Modulação de fase/"phasing"** e componentes de soma e diferença com a frequência do modulador (6–10 Hz) [P]. Não preserva formantes e, para material vozeado, produz descontinuidades de forma de onda que o TD-PSOLA evita por construção (é essa a razão de o protótipo ter escolhido TD-PSOLA). |
| É a arquitetura que a Antares usa? | **Não.** A patente [P] descreve reamostragem com **repetição/descarte de ciclos inteiros** sincronizados ao período — ver §3.4. A hipótese 2 da §8.1 está **refutada** para o Auto-Tune. |

### 3.4 Rastreio contínuo de período (L6) — o achado central desta pesquisa

A pergunta do briefing: *a ideia de que um rastreador travado precisa de apenas um período novo
já existe? É (a) padrão e nomeada, (b) mencionada mas pouco explorada, ou (c) inédita?*

**Resposta: (a), com uma correção conceitual importante.** A ideia existe, é nomeada, e a
correção é que o ganho real **não é** "2τ → 1τ".

#### 3.4.1 A patente da Auto-Tune descreve exatamente isso **[P]**

Hildebrand, US 5.973.252 A. Verbatim:

> "The method and apparatus of this invention provides the ultimate redundancy in measuring
> pitch, or its inverse, the period of the waveform: it uses **all samples in the waveform, by
> continuously comparing each cycle with the previous cycle**."

> "In the pitch detection mode, the pitch detection is virtually instantaneous. The device can
> **detect the repetition in a periodic sound within a few cycles**. This usually occurs before
> the sound has sufficient amplitude to be heard."

As equações centrais, verbatim (notação da patente):

> "E_i(L) = E_{i−1}(L) + x_i² − x_{i−2L}²   (4)
> H_i(L) = H_{i−1}(L) + x_i·x_{i−L} − x_{i−L}·x_{i−2L}   (5)
> In other words, for each prospective lag, L, **four multiple-adds** must be computed."

> "The function E_i(L) is so named because it is the accumulated energy of the waveform over
> **two periods, 2L**."

Teste de periodicidade: `E_i(L) − 2·H_i(L) ≤ eps·E_i(L)`.

E o modo de correção, verbatim:

> "**The correction mode must track changes in pitch. This is done by computing equations (4)
> and (5) over a small range of L values around the detected pitch. As the input pitch shifts,
> the minimum value of equation (6) shifts, and the range of L values is shifted accordingly.**"

A patente também descreve a estratégia de custo: o **modo de detecção** roda sobre dados
**decimados 8:1** (44,1 kHz → 5,5125 kHz), varrendo L de 2 a 110 (isto é, 50,1 Hz a 2756 Hz),
fora do laço de interrupção; o **modo de correção** roda em taxa plena numa janela estreita de
L em torno do período detectado. Há ainda interpolação parabólica (ajuste quadrático nos três
pontos em torno de L_min) para obter o período em ponto flutuante.

#### 3.4.2 A correção conceitual que isto impõe a L6

A hipótese registrada na §8.1 e em L6 é *"uma vez travado num período, atualizar a estimativa
custa um período novo, não dois; o piso de detecção cai de 11,4 para ~5,7 ms."*

**Isso subestima o ganho e erra o mecanismo.** **[D]** Na formulação da patente:

- A janela **continua sendo de 2L** — `E_i(L)` é explicitamente "a energia acumulada ao longo
  de dois períodos". **A hipótese de que bastaria 1 período está errada.**
- Mas esses 2L são **passado**, não *look-ahead*. A recursão de (4) e (5) atualiza o estimador
  **a cada amostra nova**, com custo O(1) por lag. A estimativa em `i` usa apenas
  `x_j, j ≤ i`. **A latência algorítmica da detecção é, no limite, uma amostra**, não 2τ.
- O que o protótipo paga hoje **não é** o comprimento da janela — é o fato de **acumular um
  quadro inteiro e só então analisar** (`frame` + `hop` + `look`). É o **lote**, não a janela.

**Reformulação recomendada de L6:**

> **L6 (revisado) — CMNDF recursivo em vez de CMNDF por quadro.** Manter a janela de 2τ, mas
> trocar o cálculo em lote por atualização incremental por amostra sobre uma faixa estreita de
> lags em torno do período corrente, com re-varredura completa só quando o teste de
> periodicidade falha (ataque de nota, região não vozeada). Ganho esperado: **elimina os termos
> `frame` e `look·hop` do orçamento de latência**, deixando apenas a guarda do PSOLA — isto é,
> de 17,3 ms (após L1+L2+L3) para a ordem de **5,7 ms**, dentro da faixa recomendada pela
> literatura perceptual da §1.2. Custo: 4 multiplicações-acumulações por lag por amostra;
> com ~24 lags de faixa estreita e fs = 44,1 kHz, ~4,2 M MAC/s — perfeitamente viável.

Isso muda L6 de "pesquisa arriscada, só se sobrar tempo" (item 17 no backlog) para
**a única mudança capaz de levar o RNF01 à faixa defensável pela literatura**. Recomendo
repriorizar.

> **Ressalva honesta a manter no texto:** o rastreio recursivo resolve a latência da **detecção**.
> Ele **não** resolve, sozinho, a decisão de vozeamento nem a robustez contra erro de oitava,
> que hoje são responsabilidade do HMM/Viterbi. A patente resolve isso com um teste de limiar
> (`E − 2H ≤ eps·E`) mais uma verificação de sub-harmônico nos lags L e 2L, **muito mais
> simples e menos robusto** que o pYIN. Trocar Viterbi por limiar é um **retrocesso em
> robustez** que precisa ser medido, não assumido. Este é exatamente o trade-off que o TCC 2
> pode quantificar — e que seria um resultado original de verdade.

#### 3.4.3 A linha acadêmica correspondente **[P]**

**Hohmann, V. (2021).** *The Period-Modulated Harmonic Locked Loop (PM-HLL): A low-effort
algorithm for rapid time-domain multi-periodicity estimation.* **Acta Acustica 5:56.**
DOI 10.1051/aacus/2021050. (Pré-print arXiv:2107.06645.)

Do resumo, verbatim:

> "Depending on the Signal-to-Noise Ratio (SNR), the estimator was found to **converge within
> 3-4 signal repetitions**, even at SNR close to or below 0dB. Furthermore, it was found to
> **follow fundamental frequency sweeps with a delay of less than one period** […]. The results
> suggest that the proposed algorithm may be applicable to **low-delay** speech and music
> analysis and processing."

Do corpo do artigo, verbatim:

> "**Once locked to the periodic component**, the PM-HLL adapts its internal PM-HLL oscillator
> frequency over time to the fundamental frequency of the periodic component."

> "Hearing aids, e.g., **require the total delay to be well below 10 ms**, which makes the
> application of pitch-based processing difficult but yet interesting. […] Another option is to
> improve spectral estimates by using the PM-HLL to derive **period-synchronous
> ('pitch-synchronous') signal features**."

> "An autocorrelation function testing the same range of delays needs a delay line of the same
> length, but, being the inverse Discrete Fourier Transform of the power spectrum, uses
> implicitly a **less specific sinusoidal signal model**."

O artigo também situa a família: laços de fase (**PLL**) são usuais mas *"most PLLs do not
include internal time delays corresponding to the periodicity to be detected, and are
therefore best adapted to detecting sinusoidal signals by design. **An exception is the
Delay-Locked-Loop (DLL)**, which uses internal delays to detect and estimate periodic signals
of [arbitrary waveform]"*.

**Nomes sob os quais a ideia vive**, para busca bibliográfica futura:
`period-synchronous` / `pitch-synchronous analysis`, `harmonic locked loop (HLL)`,
`delay-locked loop (DLL)`, `frequency-locked loop pitch tracker`, `low-delay f0 estimation`.

#### 3.4.4 Evidência indireta do mercado **[P]**

A página oficial do **Waves Tune Real-Time** declara a latência como
**"Zero or near-zero latency (**depending on pitch**)"**. Uma latência que depende da altura do
sinal é assinatura de processamento **síncrono com o período** — coerente com a família acima
e incoerente com um pipeline de quadro fixo. É evidência circunstancial, mas de primeira mão.

### 3.5 Consequências para L3, L6, A1–A4 e para o texto

| Item | Como a bibliografia o afeta |
|---|---|
| **L3** (guarda 2T → 1T) | **Consistente com a patente** [P]: o Auto-Tune reamostra repetindo/descartando **um ciclo inteiro** quando o ponteiro de saída ultrapassa o de entrada, o que exige exatamente **um período** de folga, não dois. Reforça a viabilidade de L3. |
| **L6** | **Reformular** conforme §3.4.2 e **repriorizar**. Passa de item 17 a candidato ao topo do backlog do TCC 2. |
| **A1–A4** | Sem impacto bibliográfico direto. A1/A2 (buffers e janela limitada) **ficam mais fáceis** se L6 for adotado: o rastreio recursivo já exige histórico limitado e fixo (2·L_max amostras), que é precisamente o dimensionamento de anel que A2 precisa. **Ordem sugerida: L6 → A1 → A2.** |
| **§12 "Leituras que faltam"** | Atualizar: trocar Dattorro por Disch & Zölzer; acrescentar Hildebrand (patente), Hohmann (2021), Lester & Boley (2007), Marentakis et al. (2012), Prame (1994, 1997), Wager et al. (2020). |
| **Referência a Verhelst & Roelands** | Manter, mas **sem afirmar redução de latência** (§3.1). |

---

## 4. Placar — o que agora tem citação e o que continua sem

### 4.1 Afirmações que passam a ter fonte primária

| Afirmação atual do trabalho | Fonte que a sustenta | Ressalva |
|---|---|---|
| "Auto-Tune tem latência de poucos ms" | Antares, manual do AutoTune 2026 [P]: **112 amostras / 2,3 ms (Modern)** e **37 amostras / 0,77 ms (Classic)** a **48 kHz**, em **Low Latency Mode** | Só vale para Low Latency Mode; HQ não publicado. |
| "Retune Speed é um limite em taxa, não uma zona morta" | Antares, manuais AutoTune 2026 / Pro 11 / Hybrid [P]: *"how rapidly the pitch correction is applied"*, **unidade em ms**, faixa natural **10–50 ms** | — |
| "Uma zona morta não distingue expressão de erro" | Melodyne (Celemony) [P]: **modulação de altura = rápida e intencional** vs. **deriva = lenta e não intencional**; a macro de correção deixa a modulação intacta | Melodyne é offline. |
| "O protótipo implementou zona morta em vez de retune speed" | Antares [P] documenta **Flex-Tune** como zona morta (*"the correction area around the scale note gets smaller"*) **separadamente** do Retune Speed | Reformular §8.2: os dois coexistem no produto comercial. |
| "Filtrar a correção em vez do alvo (C1)" | Hildebrand, US 5.973.252 [P]: **suavização exponencial de `Resample_Raw_Rate`** (= razão de correção), com coeficiente `Decay` ajustável pelo usuário | **C1 não é original.** A equivalência log/linear é derivação minha **[D]**. |
| "Auto-Tune não reanalisa uma janela do zero a cada quadro" (§8.1, hipótese 1) | Hildebrand [P]: equações recursivas (4) e (5), *"continuously comparing each cycle with the previous cycle"*, correção sobre *"a small range of L values around the detected pitch"* | Confirma a hipótese — e mostra que o ganho é maior que o previsto (§3.4.2). |
| "Detecção de pitch travada precisa de menos sinal" (L6) | Hohmann (2021), Acta Acustica [P]: convergência em **3–4 repetições**, seguimento de varredura com **atraso menor que um período** | A janela continua sendo de 2 períodos; o ganho vem de eliminar o lote. |
| "Cantores são especialmente sensíveis à latência com in-ear" | Lester & Boley (2007) [P]: voz é o **único** instrumento cujo limiar "Fair" com IEM é menor que o "Good" com monitor de chão | **Não** vale a versão forte ("cantores são os mais sensíveis") — sax é mais crítico nos dados. |
| "Pitch shifting por linha de atraso modulada dispensa detecção de F0" | Disch & Zölzer (1999) [P]: *"does not need any base frequency estimation"* | O artigo **não** declara latência. |
| "Preservar expressão é o critério de naturalidade em correção de altura" | Wager et al. (2020), ICASSP [P]: modelo prediz **deslocamento em cents**, treinado a corrigir desafinação e **preservar variação intencional** | — |

### 4.2 Afirmações que continuam **sem** fonte

| Afirmação | Situação |
|---|---|
| **"Autotune ao vivo tolera no máximo ~20–30 ms antes de incomodar o cantor"** | **[✗] SEM FONTE CIENTÍFICA. E provavelmente falsa para in-ear.** O fraseado idêntico existe em blog da Antares [V]. Os dados experimentais apontam **7–13 ms** (coloração, in-ear seco, Marentakis et al.) e **~1 ms / ~6,5 ms** (Good/Fair, voz com IEM, pior caso, Lester & Boley). **Ação obrigatória: reescrever o RNF01.** |
| "O cantor ouve a própria voz por condução óssea e isso agrava o filtro-pente" | **[✗] Não encontrada** medida primária em contexto de latência de monitoração. Marcar como hipótese ou remover. |
| "Um cantor real chega na nota por baixo em ~40–80 ms" (§8.2, Quantização 3) | **[✗] Não encontrada fonte.** É afirmação empírica plausível, mas nenhum estudo de *pitch scoop* / ataque foi localizado que dê esse intervalo. **Sugestão:** medir no próprio corpus (Vocadito) e apresentar como **medida do trabalho**, não como citação. |
| "WSOLA reduz latência em relação ao TD-PSOLA" | **[✗] Sem evidência.** O que está verificado é *"allows for online processing"*. A análise da §3.1 sugere que o ganho é de **complexidade e robustez**, não de latência. Não afirmar redução de latência. |
| "A latência do vocoder de fase é maior que a do TD-PSOLA" | **[✗] Sem citação direta.** A derivação da §3.2 é **[D]**. Apresentar como raciocínio de projeto. |
| "O piso de latência da linha de atraso modulada é quase zero" | **[✗] Não publicado.** Disch & Zölzer não quantificam. |
| Valores exatos de taxa e extensão de vibrato (6,0 Hz; ±71 cents) | **[P-2ª] — conferir os PDFs de Prame (1994, 1997) antes da defesa.** O intervalo qualitativo (5–8 Hz, dezenas de cents) é seguro. |
| "A métrica de razão de preservação de vibrato por banda já existe" | **[✗] Não encontrada.** Isto é **bom**: é a candidata mais forte a contribuição original do trabalho. |
| Latência do Auto-Tune fora do Low Latency Mode (o "58,2 ms" que circula) | **[✗] Não confirmado** em documentação Antares. **Não citar.** Se for usado, tem de vir de medição própria (L7). |

### 4.3 Ações recomendadas, em ordem

1. **Reescrever o RNF01** com meta ancorada em Marentakis et al. (2012) e Lester & Boley (2007),
   declarando explicitamente **qual limiar** está sendo perseguido (coloração? preferência?).
   Resolve a correção pendente nº 3 da §10.
2. **Reposicionar C1** no texto: de "contribuição original" para "replicação fundamentada do
   mecanismo descrito em US 5.973.252 e documentado pela Antares, aplicada a um pipeline
   pYIN+TD-PSOLA, com verificação quantitativa inédita".
3. **Reformular e repriorizar L6** conforme §3.4.2 (recursão por amostra, não redução de janela).
4. **Corrigir a §8.2**: separar Flex-Tune (zona morta, implementada) de Retune Speed (limite em
   taxa, não implementada), citando os dois manuais.
5. **Trocar Dattorro por Disch & Zölzer** na lista de leituras da §12.
6. **Obter e conferir**: PDFs de Prame (1994, 1997), Sundberg (1994) e Verhelst & Roelands
   (1993). São as três lacunas de verificação que restam.
7. **Remover ou marcar como hipótese** as afirmações da §4.2 que não têm fonte.

---

## 5. Referências — prontas para colar em `tcc.bib`

```bibtex
@inproceedings{lester2007latency,
  author       = {Lester, Michael and Boley, Jon},
  title        = {The Effects of Latency on Live Sound Monitoring},
  booktitle    = {Proceedings of the 123rd Audio Engineering Society Convention},
  year         = {2007},
  month        = oct,
  address      = {New York, NY, USA},
  note         = {Convention Paper 7198},
  url          = {https://aes2.org/publications/elibrary-page/?id=14256}
}

@inproceedings{marentakis2012latency,
  author       = {Marentakis, Georgios and Kranzler, Christian and Frank, Matthias
                  and Opitz, Martin and Sontacchi, Alois},
  title        = {Latency Tolerance Enhancement in In-Ear Monitoring Systems},
  booktitle    = {Fortschritte der Akustik --- DAGA 2012},
  year         = {2012},
  pages        = {323--324},
  address      = {Darmstadt, Germany},
  url          = {https://pub.dega-akustik.de/DAGA_2012/data/articles/000239.pdf}
}

@inproceedings{schmid2024jnd,
  author       = {Schmid, Andreas and Ambros, Maria and Bogon, Johanna and Wimmer, Raphael},
  title        = {Measuring the Just Noticeable Difference for Audio Latency},
  booktitle    = {Proceedings of the 19th International Audio Mostly Conference:
                  Explorations in Sonic Cultures},
  year         = {2024},
  address      = {Milan, Italy},
  publisher    = {ACM},
  doi          = {10.1145/3678299.3678331}
}

@article{noson2002melisma,
  author       = {Noson, D. and Sato, S. and Sakai, H. and Ando, Y.},
  title        = {Melisma Singing and Preferred Stage Acoustics for Singers},
  journal      = {Journal of Sound and Vibration},
  volume       = {258},
  number       = {3},
  pages        = {473--485},
  year         = {2002},
  note         = {Citado indiretamente via Marentakis et al. (2012); original nao consultado}
}

@article{marshall1978ensemble,
  author       = {Marshall, A. H. and Gottlob, D. and Alrutz, H.},
  title        = {Acoustical Conditions Preferred for Ensemble},
  journal      = {The Journal of the Acoustical Society of America},
  volume       = {64},
  number       = {5},
  pages        = {1437--1442},
  year         = {1978},
  note         = {Citado indiretamente via Marentakis et al. (2012); original nao consultado}
}

@article{ando1977preference,
  author       = {Ando, Y.},
  title        = {Subjective Preference in Relation to Objective Parameters of
                  Music Sound Fields with a Single Echo},
  journal      = {The Journal of the Acoustical Society of America},
  volume       = {62},
  number       = {6},
  pages        = {1436--1441},
  year         = {1977},
  note         = {Citado indiretamente via Marentakis et al. (2012); original nao consultado}
}

@article{pfordresher2011daf,
  author       = {Pfordresher, Peter Q. and Dalla Bella, Simone},
  title        = {Delayed Auditory Feedback and Movement},
  journal      = {Journal of Experimental Psychology: Human Perception and Performance},
  volume       = {37},
  number       = {2},
  pages        = {566--579},
  year         = {2011},
  doi          = {10.1037/a0021487},
  note         = {Consultado via resumo; PDF completo nao acessado}
}

@misc{antares2026manual,
  author       = {{Antares Audio Technologies}},
  title        = {{AutoTune 2026 Product Manual}},
  year         = {2026},
  howpublished = {Documentacao oficial do produto},
  url          = {https://www.antarestech.com/documentation/autotune-2026},
  note         = {Secoes ``Retune Speed'', ``Humanize'', ``Flex Tune'' e ``Mode''
                  (Latency Modes). Acesso em 26 ago. 2026}
}

@misc{antarespro11manual,
  author       = {{Antares Audio Technologies}},
  title        = {{AutoTune Pro 11 Product Manual}},
  year         = {2025},
  howpublished = {Documentacao oficial do produto},
  url          = {https://www.antarestech.com/documentation/auto-tune-pro-11},
  note         = {Secoes ``Retune Speed'' (unidades em milissegundos) e ``Flex-Tune''.
                  Acesso em 26 ago. 2026}
}

@misc{antareshybridmanual,
  author       = {{Antares Audio Technologies}},
  title        = {{AutoTune Hybrid Product Manual}},
  year         = {2025},
  howpublished = {Documentacao oficial do produto},
  url          = {https://www.antarestech.com/documentation/auto-tune-hybrid},
  note         = {Acesso em 26 ago. 2026}
}

@misc{antaresblog2025latency,
  author       = {{Antares Audio Technologies}},
  title        = {{Low Latency Monitoring in DAWs: What It Is and Why It Matters}},
  year         = {2025},
  howpublished = {Blog do fabricante --- material de marketing, sem revisao por pares},
  url          = {https://www.antarestech.com/blog/low-latency-monitoring-in-daws-what-it-is-and-why-it-matters},
  note         = {Fonte provavel da afirmacao ``20 a 30 ms''. Nao citavel como evidencia}
}

@patent{hildebrand1999patent,
  author       = {Hildebrand, Harold A.},
  title        = {Pitch Detection and Intonation Correction Apparatus and Method},
  number       = {US 5,973,252 A},
  year         = {1999},
  month        = oct,
  assignee     = {Auburn Audio Technologies, Inc.},
  url          = {https://patents.google.com/patent/US5973252A/en},
  note         = {Patente original da tecnologia Auto-Tune. Equacoes (4)--(6):
                  atualizacao recursiva por amostra das funcoes E e H;
                  suavizacao exponencial de Resample\_Raw\_Rate (Retune Speed)}
}

@misc{waves_tunerealtime,
  author       = {{Waves Audio Ltd.}},
  title        = {{Waves Tune Real-Time --- Product Specification}},
  howpublished = {Pagina oficial do produto},
  url          = {https://www.waves.com/plugins/waves-tune-real-time},
  note         = {Latencia declarada: ``Zero or near-zero latency (depending on pitch)''
                  a 44,1/48/88,2/96 kHz. Acesso em 26 ago. 2026}
}

@misc{celemony_modulation_drift,
  author       = {{Celemony Software GmbH}},
  title        = {{Melodyne 5 --- Pitch Modulation and Drift / Correct Pitch Macro}},
  howpublished = {Centro de ajuda oficial},
  url          = {https://helpcenter.celemony.com/M5/doc/melodyneStudio5/en/M5tour_ToolModulationDrift_2},
  note         = {Definicoes de ``pitch modulation'' (rapida, intencional) e
                  ``pitch drift'' (lenta, nao intencional). Acesso em 26 ago. 2026}
}

@misc{baran_autotalent,
  author       = {Baran, Thomas A.},
  title        = {{Autotalent: a real-time pitch correction plug-in}},
  howpublished = {Codigo-fonte, licenca GPL-2},
  url          = {http://tombaran.info/autotalent.html},
  note         = {Porte VST/AU 64 bits mantido por M. Donovan:
                  https://github.com/michaeldonovan/AutoTalent .
                  O filtro de glide e aplicado ao alvo (sptarget), nao a correcao}
}

@inproceedings{wager2020deepautotuner,
  author       = {Wager, Sanna and Tzanetakis, George and Wang, Cheng-i and Kim, Minje},
  title        = {Deep Autotuner: a Pitch Correcting Network for Singing Performances},
  booktitle    = {IEEE International Conference on Acoustics, Speech and Signal
                  Processing (ICASSP)},
  year         = {2020},
  eprint       = {2002.05511},
  archivePrefix= {arXiv},
  url          = {https://arxiv.org/abs/2002.05511}
}

@article{wager2019deepautotuner,
  author       = {Wager, Sanna and Tzanetakis, George and Wang, Cheng-i and Guo, Lijiang
                  and Sivaraman, Aswin and Kim, Minje},
  title        = {Deep Autotuner: A Data-Driven Approach to Natural-Sounding Pitch
                  Correction for Singing Voice in Karaoke Performances},
  journal      = {arXiv preprint},
  year         = {2019},
  eprint       = {1902.00956},
  doi          = {10.48550/arXiv.1902.00956}
}

@article{prame1994vibrato,
  author       = {Prame, Eric},
  title        = {Measurements of the Vibrato Rate of Ten Singers},
  journal      = {The Journal of the Acoustical Society of America},
  volume       = {96},
  number       = {4},
  pages        = {1979--1984},
  year         = {1994},
  doi          = {10.1121/1.410141},
  note         = {Taxa media entre cantores: 6,0 Hz. Valor obtido do resumo;
                  PDF completo nao consultado}
}

@article{prame1997vibrato,
  author       = {Prame, Eric},
  title        = {Vibrato Extent and Intonation in Professional Western Lyric Singing},
  journal      = {The Journal of the Acoustical Society of America},
  volume       = {102},
  number       = {1},
  pages        = {616--621},
  year         = {1997},
  doi          = {10.1121/1.419735},
  note         = {Extensao media relatada de +/- 71 cents. CONFERIR NO ORIGINAL}
}

@article{sundberg1994vibrato,
  author       = {Sundberg, Johan},
  title        = {Acoustic and Psychoacoustic Aspects of Vocal Vibrato},
  journal      = {STL-QPSR (Speech, Music and Hearing Quarterly Progress and
                  Status Report, KTH)},
  volume       = {35},
  number       = {2-3},
  pages        = {45--68},
  year         = {1994},
  url          = {http://www.speech.kth.se/prod/publications/files/qpsr/1994/1994_35_2-3_045-068.pdf},
  note         = {Taxa tipica 5--8 Hz, extensao inferior a +/- 1 semitom.
                  Servidor do KTH indisponivel na consulta; CONFERIR NO ORIGINAL}
}

@book{sundberg1987science,
  author       = {Sundberg, Johan},
  title        = {The Science of the Singing Voice},
  publisher    = {Northern Illinois University Press},
  address      = {DeKalb, IL},
  year         = {1987}
}

@article{hohmann2021pmhll,
  author       = {Hohmann, Volker},
  title        = {The Period-Modulated Harmonic Locked Loop (PM-HLL): A Low-Effort
                  Algorithm for Rapid Time-Domain Multi-Periodicity Estimation},
  journal      = {Acta Acustica},
  volume       = {5},
  pages        = {56},
  year         = {2021},
  doi          = {10.1051/aacus/2021050},
  note         = {Pre-print: arXiv:2107.06645}
}

@inproceedings{verhelst1993wsola,
  author       = {Verhelst, Werner and Roelands, Marc},
  title        = {An Overlap-Add Technique Based on Waveform Similarity (WSOLA)
                  for High Quality Time-Scale Modification of Speech},
  booktitle    = {IEEE International Conference on Acoustics, Speech, and Signal
                  Processing (ICASSP-93)},
  volume       = {2},
  pages        = {554--557},
  year         = {1993},
  doi          = {10.1109/ICASSP.1993.319366},
  note         = {Somente resumo consultado; texto integral nao acessado}
}

@inproceedings{roelands1993wsola,
  author       = {Roelands, Marc and Verhelst, Werner},
  title        = {Waveform Similarity Based Overlap-Add (WSOLA) for Time-Scale
                  Modification of Speech: Structures and Evaluation},
  booktitle    = {Proceedings of the 3rd European Conference on Speech Communication
                  and Technology (Eurospeech 1993)},
  pages        = {337--340},
  year         = {1993},
  doi          = {10.21437/Eurospeech.1993-59}
}

@inproceedings{disch1999delayline,
  author       = {Disch, Sascha and Z{\"o}lzer, Udo},
  title        = {Modulation and Delay Line Based Digital Audio Effects},
  booktitle    = {Proceedings of the 2nd COST G-6 Workshop on Digital Audio Effects
                  (DAFx-99)},
  pages        = {5--8},
  year         = {1999},
  address      = {Trondheim, Norway},
  url          = {https://www.dafx.de/paper-archive/1999/disch.pdf},
  note         = {Secao 5: transposicao de altura por tres linhas de atraso moduladas,
                  sem estimacao de frequencia fundamental}
}

@article{dattorro1997effect2,
  author       = {Dattorro, Jon},
  title        = {Effect Design, Part 2: Delay-Line Modulation and Chorus},
  journal      = {Journal of the Audio Engineering Society},
  volume       = {45},
  number       = {10},
  pages        = {764--788},
  year         = {1997},
  url          = {https://ccrma.stanford.edu/~dattorro/EffectDesignPart2.pdf},
  note         = {ATENCAO: nao trata de pitch shifting. Cobre interpolacao de linha
                  de atraso, chorus, flange e vibrato}
}

@article{laroche1999pvoc,
  author       = {Laroche, Jean and Dolson, Mark},
  title        = {Improved Phase Vocoder Time-Scale Modification of Audio},
  journal      = {IEEE Transactions on Speech and Audio Processing},
  volume       = {7},
  number       = {3},
  pages        = {323--332},
  year         = {1999},
  doi          = {10.1109/89.759041}
}

@inproceedings{laroche1999pitchshift,
  author       = {Laroche, Jean and Dolson, Mark},
  title        = {New Phase-Vocoder Techniques for Pitch-Shifting, Harmonizing
                  and Other Exotic Effects},
  booktitle    = {IEEE Workshop on Applications of Signal Processing to Audio
                  and Acoustics (WASPAA)},
  pages        = {91--94},
  year         = {1999},
  doi          = {10.1109/ASPAA.1999.810857}
}

@article{flanagan1966phasevocoder,
  author       = {Flanagan, James L. and Golden, R. M.},
  title        = {Phase Vocoder},
  journal      = {Bell System Technical Journal},
  volume       = {45},
  number       = {9},
  pages        = {1493--1509},
  year         = {1966},
  doi          = {10.1002/j.1538-7305.1966.tb01706.x}
}

@article{driedger2016tsm,
  author       = {Driedger, Jonathan and M{\"u}ller, Meinard},
  title        = {A Review of Time-Scale Modification of Music Signals},
  journal      = {Applied Sciences},
  volume       = {6},
  number       = {2},
  pages        = {57},
  year         = {2016},
  doi          = {10.3390/app6020057},
  note         = {Texto integral nao extraido; consultado apenas via resumo}
}
```

---

*Pesquisa bibliográfica — TCC PUCRS, 2026. Fontes consultadas em 25–26 de agosto de 2026.
As marcas [P], [P-2ª], [V], [D] e [✗] indicam o nível de verificação de cada afirmação e
devem ser preservadas na transposição para o texto do TCC.*
