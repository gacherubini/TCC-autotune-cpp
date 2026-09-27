# Spec — encaixe de nota e estabilidade da correção

> Estado: **pronto para implementação**
> Data: 2026-09-03 · **revisado em 2026-09-03** após sessão de perguntas dirigidas com o autor.
> Origem: sessão de diagnóstico com medição, a partir de escuta do plugin no Ableton.
> Todos os números deste documento foram **medidos**, com harnesses sobre `AutotuneStream` e
> `exemplo-antes.wav`. Onde houver estimativa, ela está marcada como tal.

> **O que a revisão mudou.** A primeira redação deixava três decisões em aberto e errava uma
> premissa.
> 1. A tessitura do autor foi **medida** e mostra que `Low Male` **não** cobre a voz dele — §3.
> 2. A lista de tessituras **encolhe** para a do Auto-Tune em vez de crescer. A decisão
>    anterior fazia o oposto, e o argumento de compatibilidade que a sustentava caiu junto.
> 3. As escolhas pendentes de D3, D6 e D7 foram fechadas.
> 4. "Outros tipos de escala", que aparecia como pedido do autor, era na verdade um pedido de
>    **Input Type**. As três escalas atuais (cromática, maior, menor natural) são as três que
>    ele quer; escalas saem de escopo.

---

## 1. Declaração do problema

Do ponto de vista de quem usa o plugin:

1. **As notas não encaixam.** A saída raramente para em cima de um semitom. Ela fica perto,
   oscilando, e o medidor de correção vive fora da faixa clara.
2. **O motor padrão (TD-PSOLA) pipoca.** Estalos frequentes durante a performance. Ligar o
   Low Latency faz o pipoco sumir.
3. **O Retune Speed não se comporta como o do Auto-Tune.** No Auto-Tune, o máximo (400 ms)
   soa como "sem correção". Aqui, o máximo (200 ms) soa como *correção ruim* — nem afinado
   nem cru, um enjoo permanente.
4. **Notas saem na oitava errada** em alguns presets de tessitura.
5. **Não existe um preset de tessitura que sirva a uma voz masculina inteira.** O `Contralto`
   ignora tudo abaixo de F3; os presets graves deslocam os agudos.
6. **A lista de tessituras não é a do Auto-Tune.** São 7 presets SATB (`Baixo`, `Baritono`,
   `Tenor`, `Contralto`, `Mezzo`, `Soprano`, `Instrumento`) e nenhum dos nomes que o Auto-Tune
   usa. O autor quer a lista dele, e **mais curta**.

Os seis sintomas têm **três causas**, e duas delas explicam mais de um sintoma cada.

---

## 2. As três causas

### Causa 1 — a detecção de altura mente acima do `fmax` da tessitura

A busca de período (`candidato()`, `src/core/dsp.h:59`) percorre a faixa a partir do período
mais curto admitido pela tessitura. Se a altura cantada for **mais aguda** que o teto do
preset, o período real fica invisível para a busca — mas o **dobro** dele não fica, porque um
sinal periódico em T também é periódico em 2T. A busca encontra 2T e reporta **metade da
frequência**.

Comportamento medido com altura conhecida, varrendo 220 a 520 Hz:

| preset (faixa) | 220 | 262 | 294 | 330 | 349 | 392 | 440 | 494 | 520 |
|---|---|---|---|---|---|---|---|---|---|
| Baixo (82–330) | 219 | 263 | 296 | 332 | **174** | **195** | **219** | **246** | **260** |
| Low Male (82–392) | 219 | 263 | 296 | 332 | 348 | 390 | **219** | **246** | **260** |
| Contralto (175–698) | 220 | 262 | 294 | 330 | 350 | 393 | 441 | 495 | 518 |

Em negrito, uma oitava abaixo do real. A quebra cai **exatamente no `fmax`** de cada preset.
Não é intermitente: **100 % dos quadros acima do teto**.

O defeito é **assimétrico**, e é isso que o torna traiçoeiro:

| situação | resultado hoje |
|---|---|
| altura **abaixo** do `fmin` | reporta "sem voz" → não corrige. Silencioso, mas honesto. |
| altura **acima** do `fmax` | reporta **uma oitava abaixo** → corrige para a nota errada. |

No áudio real, medido contra o preset mais largo: `Baixo` diverge por uma oitava em
**34,3 %** dos quadros vozeados, `Low Male` em **16,2 %**, `Tenor` e `Contralto` em 0 %.

Esta causa produz os sintomas **4**, **5** e **6** — e bloqueia o 6, porque expor `Low Male`
hoje troca "grave não corrigido" por "agudo na oitava errada".

### Causa 2 — a nota-alvo pisca

A trilha de altura detectada chega à malha de correção **crua**, sem histerese. Ruído de
poucos cents na estimativa faz a nota-alvo trocar de semitom, e a malha persegue cada troca.

Medido na configuração do usuário (Contralto, tolerância 15, retune 0, Low Latency):

| | medido |
|---|---|
| notas emitidas em 5 s de áudio | 49 |
| duração **mediana** de uma nota | **41 ms** |
| notas com menos de 80 ms | **37 (76 %)** |

O look-ahead do Viterbi foi **descartado como causa**: `look=0` dá 11,0 trocas de nota por
segundo, `look=8` dá 11,6. O ruído está na estimativa em si, não no atraso da decodificação.

**Onde está o piso do ruído.** A trilha de F0 emitida vive numa grade de `RES_CENTS = 20`
cents — é a resolução dos bins do HMM. Uma tremulação de **um único bin** já são 20 cents.
Isso põe um piso na histerese que D2 vai propor: abaixo de ~20 cents ela não faria nada.

Esta causa produz o sintoma **1** e, por interação com o Retune Speed, também o **3**.
Quanto do sintoma 3: medindo o quanto a saída fica *parada* em cima de um semitom,

| Retune Speed | erro médio da saída | saída parada numa nota |
|---|---|---|
| 0 ms | 0,0 ct | **100 %** |
| 25 ms | 11,5 ct | 63 % |
| 100 ms | 20,4 ct | 36 % |
| 200 ms | 22,7 ct | **26 %** |
| 400 ms | 23,8 ct | 28 % |

Com 200 ms, a saída está numa nota apenas 26 % do tempo. Nos outros 74 % ela está *entre*
notas, escorregando. Um filtro com constante de 200 ms **nunca alcança** um alvo que troca a
cada 41 ms — ele corre atrás e não chega. É por isso que no Auto-Tune (alvo estável) um
Retune lento soa como bypass limpo, e aqui soa como enjoo.

Nota-se também que **o controle satura**: de 200 para 400 ms o erro médio quase não muda
(22,7 → 23,8 ct). Estender a faixa sem estabilizar o alvo entrega mais números, não mais
efeito.

### Causa 3 — a síntese TD-PSOLA estoura o orçamento de tempo real

O TD-PSOLA corta o sinal em **grãos**, um por pulso glotal, e reempilha esses grãos num
espaçamento novo. As marcas de pulso são achadas por **recorrência**: a primeira é o primeiro
pico da região vozeada, e cada seguinte é a anterior mais um período, refinada por
correlação. Toda a cadeia depende da **âncora**, que é a primeira marca da região.

A função de síntese é **pura**: cada chamada redescobre a cadeia do zero. Para achar a mesma
âncora que a versão em lote acharia, ela recua até o começo da região vozeada
(`src/c1_streaming/autotune_stream.h:481`):

```c
while (winStart > 0 && f0samp[winStart - 1] > 0.0f) --winStart;
```

Numa nota sustentada de 3 segundos, no instante t = 3 s o código refaz **3 segundos** de
marcas e de grãos para entregar os 128 samples do bloco atual. O custo por bloco cresce
linearmente com a duração da nota e só volta ao normal quando a nota termina.

Cronometrando cada chamada de processamento, com bloco de 128 amostras a 44,1 kHz
(orçamento de 2,90 ms):

| motor | pior bloco | estouros do orçamento |
|---|---|---|
| **TD-PSOLA** | **12,0 ms** (4,1×) | **19,6 % dos blocos** |
| **Ponteiro móvel** (Low Latency) | 0,69 ms (0,2×) | **0 %** |

Perfil no tempo, mostrando o crescimento e o reset ao fim da região vozeada:

```
  t(s)   pior(ms)
  0.00     0.268
  1.00     2.052
  2.00     6.596   <-- estouro
  3.00    12.033   <-- estouro   (nota longa terminando)
  3.49     3.428                  (região não-vozeada: reseta)
  3.99     1.343
```

**Aumentar o buffer do host não resolve**, porque o custo é proporcional às amostras
acumuladas e não aos blocos:

| buffer do host | pior bloco | estouros |
|---|---|---|
| 128 | 11,9 ms | 19,3 % |
| 512 | 22,7 ms | 21,6 % |
| 1024 | 44,4 ms | 22,8 % |

Cada estouro é um *dropout* no host. Um em cada cinco blocos. É o pipoco do sintoma **2**.
O motor de ponteiro não sofre disso porque é O(1) por amostra — anel, interpolação de 4
pontos, sem janelas nem marcas.

---

## 3. A tessitura do autor, medida

A redação anterior deixava isto em aberto e construía a decisão de presets em cima do palpite
de que `Low Male` resolveria o sintoma 5. **Ele não resolve.**

Medição sobre `exemplo-antes.wav` com o preset mais largo (`Instrumento`, 50–2000 Hz), que é
o único que não sofre da Causa 1 neste material:

```
stream_test exemplo-antes.wav out.wav 1.0 crom voz=instrumento block=128 dumpf0=f0.txt
```

```
703 quadros vozeados de 854
min 162,5   p1 172,1   p5 202,3   mediana 282,8   p95 414,1   p99 428,7   max 443,8 Hz
```

Isso é **E3 a A4**. Cobertura por preset, em fração dos quadros vozeados:

| preset | faixa | abaixo do piso | acima do teto | cobre? |
|---|---|---|---|---|
| Baixo | 82–330 | 0,0 % | **37,3 %** | não |
| Baritono | 98–392 | 0,0 % | **18,5 %** | não |
| **Low Male** | 82–392 | 0,0 % | **18,5 %** | **não** |
| Tenor | 131–523 | 0,0 % | 0,0 % | sim |
| **Alto-Tenor** | 131–698 | 0,0 % | 0,0 % | **sim** |
| Contralto *(padrão hoje)* | 175–698 | 1,4 % | 0,0 % | não |
| Mezzo | 220–880 | 24,5 % | 0,0 % | não |

Três leituras que decidem D4:

- **`Low Male` tem o mesmo teto do `Baritono`** (392 Hz) e corta 18,5 % dos quadros vozeados
  do take. Ele é um nome de paridade com o Auto-Tune, não a resposta ao sintoma 5.
- **`Alto-Tenor` cobre 100 %** e é o preset certo para esta voz.
- O padrão de fábrica de hoje, `Contralto`, corta 1,4 % dos graves — a confirmação medida da
  reclamação "o Contralto ignora tudo abaixo de F3" (F3 = 175 Hz, que é o `fmin` dele).

Os 37,3 % e 18,5 % acima batem com os 34,3 % e 16,2 % de divergência de oitava da Causa 1: a
diferença é que nem todo quadro acima do teto vira subharmônico — alguns viram não-vozeado.

---

## 4. Duas armadilhas de parâmetro, que não são bugs

Funcionam como escritas, mas surpreendem, e não há nada na interface que avise.

**A tolerância deixa erro residual permanente.** A zona morta não encaixa a nota: ela
*empurra* o desvio até a borda da zona. Com tolerância 15, uma nota 40 cents desafinada sai
15 cents desafinada. Média medida na configuração do usuário: **11,8 cents fora**.

| desvio cantado | tol=0 | tol=5 | tol=15 | tol=30 |
|---|---|---|---|---|
| −40 ct | 0,0 | −5,0 | **−15,0** | −30,0 |
| +30 ct | 0,0 | +5,0 | **+15,0** | +30,0 |

Isto é **coerente com a [Decisão 2 revista](historico-e-decisoes.md#decisão-2--renomear-tolerancia-para-flex-tune)**,
que estabeleceu que a `Tolerância` não é o Flex-Tune e não deve fingir ser. O que este spec
questiona não é a existência da zona morta, é o **valor padrão** e a **ausência de aviso**.

**Retune Speed em 0 desliga o Natural Vibrato e o Humanize.** Com constante de tempo zero, o
passa-baixa sobre a altura real vira a própria altura real, e o termo do Natural Vibrato vira
exatamente zero para qualquer valor do controle:

```
out = LP(alvo) + k·(real − LP(real))
τ = 0  →  α = 0  →  LP(real) = real  →  out = alvo,  qualquer que seja k
```

O Humanize está atrás de uma guarda de constante de tempo positiva e nem chega a ser
avaliado. Verificado: saída **bit-idêntica** para Natural Vibrato 0 e 1, e para Humanize 0 e
1, quando o Retune Speed é 0.

Não é para ser consertado no DSP. A [Decisão 7](historico-e-decisoes.md#decisão-7--o-deslize-de-entrada-é-fixo-2026-08-26)
fixa o deslize de entrada, e mexer no ataque para dar efeito ao Natural Vibrato em Retune 0 a
contrariaria. É um problema de **comunicação na interface** — e ele fica **mais** agudo depois
de D6, porque o padrão de fábrica passa a ser `retune = 0`, e o plugin passa a nascer com dois
dos quatro controles de expressão inertes.

---

## 5. Solução

Três correções de comportamento, na ordem em que se destravam, mais três de interface.

**Primeiro, a guarda contra a subharmônica.** Acima do teto da tessitura, a detecção passa a
reportar "sem voz", igual ao que já faz abaixo do piso. O usuário deixa de ouvir notas na
oitava errada; no lugar, ouve a nota não corrigida — que é o comportamento honesto e o mesmo
que já acontece no grave. Isso desbloqueia a lista nova de tessituras.

**Segundo, a estabilização da nota-alvo.** A escolha de semitom ganha histerese e tempo mínimo
de permanência. A saída para de piscar entre semitons vizinhos, passa a encaixar, e — como
efeito colateral pretendido — o Retune Speed volta a se comportar como o do Auto-Tune: lento
passa a significar "a correção não chega", não "a correção persegue um alvo que foge".

**Terceiro, o teto na janela de re-síntese do TD-PSOLA.** O custo por bloco deixa de crescer
com a duração da nota. O motor padrão para de pipocar sem que o usuário precise ligar o Low
Latency e abrir mão da preservação de formantes.

**Quarto, a lista de tessituras encolhe** para a do Auto-Tune: quatro itens, com a faixa de
notas no rótulo, e o padrão passando a ser o que cobre a voz do autor.

**Quinto, os padrões de fábrica mudam**: a tolerância vai a zero para que a nota encaixe, e o
Retune Speed vai a zero para que o efeito duro seja a primeira impressão.

**Sexto, a interface comunica** quais controles o Retune Speed 0 torna inertes, e por quê.

---

## 6. Histórias de usuário

### Detecção de altura e oitava

1. Como cantor, quero que uma nota mais aguda que o teto da tessitura escolhida **não** seja
   corrigida para a oitava abaixo, para que eu não ouça a minha voz saltando de oitava no
   meio da frase.
2. Como cantor, quero que uma nota fora da faixa da tessitura escolhida saia **sem correção**
   em vez de sair corrigida errado, para que o pior caso seja "não fez nada" e não "estragou".
3. Como cantor, quero que o comportamento fora da faixa seja o **mesmo no grave e no agudo**,
   para que eu não precise saber de qual lado da faixa eu estou para prever o que vai
   acontecer.
4. Como cantor, quero que trechos não vozeados (consoantes, respiração) continuem passando
   sem correção, para que a guarda nova não introduza cortes onde antes não havia.
5. Como autor do TCC, quero que o defeito de oitava e sua correção fiquem registrados com os
   números medidos, para que eu possa descrevê-los no texto.

### Lista de tessituras (Input Type)

6. Como cantor, quero a **mesma lista de Input Type do Auto-Tune**, para que eu possa comparar
   os dois plugins sem traduzir nomes de tessitura.
7. Como cantor, quero **menos opções** na lista, para que escolher seja rápido e não uma
   pesquisa sobre classificação vocal.
8. Como cantor, quero escolher um preset cuja faixa cubra a minha tessitura inteira, para que
   nem os graves sejam ignorados nem os agudos deslocados.
9. Como cantor, quero que a interface me diga a faixa de **notas** de cada preset, para que eu
   escolha com informação em vez de por tentativa.
10. Como cantor, quero que o preset de fábrica já sirva a uma voz masculina comum, para que a
    primeira execução não precise de configuração.
11. Como autor do TCC, quero que os presets SATB continuem **alcançáveis pela linha de
    comando**, para que as varreduras de medição por tessitura continuem reprodutíveis mesmo
    depois de a interface encolher.

### Encaixe e estabilidade da nota

12. Como cantor, quero que a saída **pare** em cima do semitom escolhido, para que a correção
    seja audível como afinação e não como oscilação.
13. Como cantor, quero que a nota-alvo não troque de semitom por causa de ruído de poucos
    cents na detecção, para que uma nota sustentada continue sendo uma nota só.
14. Como cantor, quero que uma nota que eu realmente mudei seja seguida sem atraso perceptível,
    para que a estabilização não transforme a melodia em arrasto.
15. Como cantor, quero que passagens rápidas e legítimas continuem sendo acompanhadas, para
    que o remédio contra o piscar não engula ornamentos reais.
16. Como cantor, quero que um glissando intencional seja seguido, para que a estabilização não
    o quebre em degraus.
17. Como cantor, quero que a estabilização funcione igual nos dois motores de síntese, para
    que ligar o Low Latency não mude quais notas o plugin escolhe.
18. Como autor do TCC, quero uma métrica objetiva do piscar (duração mediana de nota, fração
    de notas curtas), para que a melhora seja demonstrável e não apenas relatada.

### Retune Speed e expressão

19. Como cantor, quero que o **padrão de fábrica seja o efeito duro**, para que a primeira
    impressão seja de um corretor que encaixa a nota.
20. Como cantor, quero **poder trocar isso nos controles** para um deslize mais vivo e
    humanizado, para que a naturalidade seja alcançável sem eu sair do plugin.
21. Como cantor, quero que o Retune Speed no máximo soe como **ausência de correção**, para
    que ele bata com a expectativa formada pelo Auto-Tune.
22. Como cantor, quero que valores intermediários produzam um deslize limpo até a nota, para
    que a naturalidade seja um contínuo e não um interruptor.
23. Como cantor, quero que a faixa do controle vá até 400 ms como no Auto-Tune, para que os
    valores sejam comparáveis entre os dois plugins.
24. Como cantor, quero que mover o controle na metade superior da faixa produza diferença
    audível, para que ele não sature.
25. Como cantor, quero saber quando o Natural Vibrato e o Humanize estão inertes, para que eu
    não passe minutos mexendo em controles que não fazem nada.
26. Como cantor, quero que os controles inertes fiquem **visivelmente** desabilitados e
    digam **o que destrava eles**, para que eu descubra pela interface e não pelo manual.

### Tolerância

27. Como cantor, quero que a configuração padrão do plugin encaixe as notas, para que a
    primeira impressão seja de um corretor que funciona.
28. Como cantor, quero entender que a tolerância deixa um resíduo proporcional ao seu valor,
    para que eu escolha o número sabendo o que ele custa.
29. Como cantor, quero poder zerar a tolerância e obter encaixe exato, para que eu tenha o
    comportamento duro disponível.

### Desempenho em tempo real

30. Como cantor, quero que o motor padrão **não estale** durante a performance, para que eu
    possa usar a preservação de formantes sem escolher entre qualidade e estabilidade.
31. Como cantor, quero que o custo por bloco não cresça durante notas longas, para que uma
    nota sustentada não fique cada vez pior conforme dura.
32. Como cantor, quero que o plugin seja utilizável no tamanho de buffer que eu já uso, para
    que eu não precise sacrificar latência do sistema inteiro para compensar um motor caro.
33. Como cantor, quero que o Low Latency continue sem estalos como está hoje, para que a
    correção do motor padrão não regrida o motor que já funciona.
34. Como autor do TCC, quero o custo por bloco dos dois motores medido e registrado, para que
    o compromisso entre preservação de formantes e custo esteja documentado com números.

### Não regressão

35. Como autor do TCC, quero que os quatro caminhos de identidade continuem devolvendo áudio
    bit-idêntico à entrada, para que a garantia central do projeto sobreviva às mudanças.
36. Como autor do TCC, quero que a saída continue idêntica para qualquer tamanho de bloco do
    host, nos dois motores, para que a invariância estrutural não seja perdida.
37. Como autor do TCC, quero que a linha de base seja re-gravada de forma **deliberada e
    documentada**, com a razão de cada checksum que mudou, para que uma mudança silenciosa não
    se esconda numa regravação em massa.
38. Como autor do TCC, quero saber quais casos da linha de base mudaram e por quê, para que eu
    possa defender cada mudança na banca.

---

## 7. Decisões de implementação

### D1 — A guarda contra a subharmônica vive na seleção de candidato de período

A função que escolhe o período a partir da CMNDF percorre a faixa a partir do período mais
curto admitido pela tessitura e aceita o primeiro vale abaixo do limiar. A correção **não** é
mudar a faixa percorrida: é rejeitar o candidato quando houver evidência de que o período real
está fora dela.

Regra, destilada do protótipo que reproduziu o defeito:

```
se existe tau < tauMin com dp[tau] < limiar:
    o periodo real esta ACIMA do fmax da tessitura -> reportar NAO-VOZEADO
senao:
    seguir com a busca atual a partir de tauMin
```

Três consequências que decidem o desenho:

- **Custo zero.** A CMNDF já é calculada sobre a faixa inteira de períodos, de 1 até o
  máximo. Os dados abaixo do limite inferior já existem; hoje simplesmente não são
  consultados. A guarda é uma leitura a mais, não um cálculo a mais.
- **Simetria deliberada.** O resultado passa a espelhar o que já acontece abaixo do piso da
  tessitura: fora da faixa, "sem voz". Um único comportamento para lembrar.
- **Piso de busca.** A varredura da guarda precisa de um piso próprio, abaixo do `tauMin` da
  tessitura mais aguda. Ele é uma constante do detector, não um parâmetro de usuário.

**A alternativa foi considerada e rejeitada por um obstáculo estrutural.** Em vez de descartar
o quadro, poder-se-ia usar o vale abaixo de `tauMin` como estimativa e reportar a altura
**real**. Isso é impossível sem aumentar o HMM: o espaço de estados tem `nBins = binDe(FMAX)+1`
posições, e o código já descarta candidatos fora disso (`if (b >= 0 && b < nBins)`). Reportar
acima do `fmax` exige mais bins, ou seja, mais custo de Viterbi por quadro — na mesma passada
em que D3 existe para **cortar** custo. Rejeitada.

### D2 — A estabilização da nota-alvo vive na escolha de semitom, não na trilha de altura

A alternativa seria filtrar a trilha de altura detectada. Fica rejeitada: filtrar a altura
destruiria o vibrato do cantor, que é exatamente o que a malha de correção existe para
preservar, e desfaria o trabalho da Etapa 3.

A histerese entra na **escolha de qual semitom é o alvo** (`notaMaisProximaMidi()`), ponto por
onde os dois motores de síntese já passam. Uma implementação, os dois motores herdam.

Dois mecanismos, combinados:

- **Histerese em cents.** Trocar de semitom exige que o novo esteja mais próximo que o atual
  por uma margem, não apenas mais próximo. Mata a oscilação em torno da fronteira entre dois
  semitons.
- **Permanência mínima.** Um semitom recém-escolhido não pode ser trocado antes de um tempo
  mínimo. Mata a troca rápida que a histerese sozinha não pega, quando o cantor está de fato
  perto do meio do caminho.

**A histerese tem um piso imposto pela análise.** A trilha de F0 vive numa grade de
`RES_CENTS = 20` cents; uma histerese menor que isso não filtraria nem uma tremulação de um
bin. A calibração parte daí — a faixa a explorar é **25 a 35 cents**, num semitom de 100.

Baixar `RES_CENTS` para 10 foi considerado e rejeitado: `W_TRANS` (12) e `SIGMA_TRANS` (2)
estão em **bins e não em cents**, então reescalar os dois junto dobraria o número de bins *e*
a largura da janela de transição — custo de Viterbi por quadro da ordem de **4×**, na mesma
passada em que D3 corta custo. Fica registrado como caminho conhecido, não tomado.

Histerese e permanência mínima são **constantes do desenho**, não controles de usuário, pela
mesma razão registrada para as constantes do Humanize: eles definem o que "nota" quer dizer, e
isso é decisão de projeto. Ficam nomeados no código para poderem ser discutidos no texto.

A permanência mínima tem um teto óbvio: precisa ser curta o bastante para não arrastar melodia
rápida. A história 15 é o contraponto da 13, e o valor escolhido é o compromisso entre as duas.
**Os dois valores precisam ser calibrados com medição**, não escolhidos de antemão.

### D3 — A janela de re-síntese do TD-PSOLA ganha teto fixo, e isso muda o áudio

Aqui havia uma tensão real, e ela foi decidida.

A janela recua até o início da região vozeada **de propósito**: é o que garante que a cadeia de
marcas comece na mesma âncora que o TD-PSOLA em lote usaria, e é o que sustenta a equivalência
entre a síntese incremental e a síntese em lote. Pôr um teto na janela **quebra essa
equivalência** para notas mais longas que o teto.

**Decisão: teto fixo.** A equivalência incremental ≡ lote é um critério de **teste**, não um
requisito de produto — ninguém canta pedindo que o incremental bata com o offline. Ela existia
porque era barata, e deixou de ser. O TD-PSOLA vale pela preservação de formantes.

A alternativa considerada — **cadeia de marcas incremental**, mantendo as marcas entre chamadas
em vez de redetectá-las — preserva a equivalência e é a resposta certa em DSP. Fica registrada
como **trabalho futuro**, não tomada agora, por uma razão concreta: ela transforma uma função
pura numa função com estado que precisa ser zerado na fronteira exata de cada região vozeada e
de cada `reset()` do host, sem depender de onde o host cortou o bloco. É exatamente a classe de
bug que o commit `e1ffd1d` caçou em agosto, quando um deslocamento de 1 a 7 amostras numa marca
se propagou pela cadeia inteira.

**Como o teto é derivado.** Em **períodos de `FMIN`**, não em amostras absolutas: a cadeia de
marcas precisa de contexto medido em períodos, e um teto de 4096 amostras são 12 períodos num
baixo e 33 num soprano. A regra é

```
teto = arredondar_para_multiplo_de(nHop, 12 * fs / FMIN)
```

com piso na `margem` atual (`nFrame`, 1024 amostras). Exemplos a 44,1 kHz: `Alto-Tenor`
(131 Hz) dá 4096; `Low Male` (82 Hz) dá 6400; `Soprano` (262 Hz) dá 2048.

Como `synthFront` anda numa grade de `k·nHop` e o teto é constante para um dado preset,
`winStart` continua sendo **função pura de `synthFront`** — a invariância ao tamanho de bloco
se preserva de graça. Isso é **inegociável**: o teto tem de sair de uma grade fixa, nunca do
que chegou no bloco atual.

*Estimativa, a confirmar com medição:* o pior bloco medido acumulava ~3 s (132 mil amostras)
para produzir 12,0 ms. Com teto de ~4 mil amostras, o custo cai da ordem de 30×, para a casa
de **0,4 ms** contra os 2,90 ms de orçamento. É estimativa por proporcionalidade, não medição.

**Contingência registrada:** se o teto degradar a qualidade mais do que o esperado, o plano B é
promover o motor de ponteiro a padrão. O custo seria entregar de fábrica o motor que **não**
preserva formantes, e a preservação de formantes é a única razão de o TD-PSOLA existir neste
projeto. Por isso é plano B, não primeira escolha.

### D4 — A lista de tessituras ENCOLHE para a do Auto-Tune, depois de D1

Esta decisão **inverte** a redação anterior, que acrescentava dois presets ao fim de uma lista
de sete. O pedido do autor é a lista do Auto-Tune, e **mais curta**.

A lista do Auto-Tune é `Soprano / Alto-Tenor / Low Male / Instrument / Bass Instrument`.
`Bass Instrument` **não existe** no núcleo de DSP e está fora de escopo. Sobram quatro, e todos
os quatro já existem em `presetVoz()`:

| item do combo | faixa | notas |
|---|---|---|
| `Soprano (C4–C6)` | 262–1047 Hz | C4–C6 |
| `Alto-Tenor (C3–F5)` | 131–698 Hz | **padrão de fábrica** |
| `Low Male (E2–G4)` | 82–392 Hz | |
| `Instrument (G1–B6)` | 50–2000 Hz | escape para fora das três faixas |

Quatro consequências:

- **A ordem importa: `Low Male` só entra depois de D1.** Sem a guarda, expô-lo troca um defeito
  silencioso (grave não corrigido) por um defeito audível (agudo na oitava errada).
- **O padrão de fábrica passa de `Contralto` para `Alto-Tenor`.** A medição da §3 mostra que o
  `Contralto` corta 1,4 % dos graves do take do autor e o `Alto-Tenor` cobre 100 %. Preço a
  registrar: preset mais largo significa `fs/FMIN` maior, logo **mais latência** no PSOLA —
  337 amostras de guarda contra 252 do `Contralto`.
- **Só a GUI encolhe.** `presetVoz()` mantém os nove nomes, e `voz=baritono` continua
  funcionando nos CLIs e no `stream_test`. Os presets SATB são **dados de medição** antes de
  serem itens de menu: foi com eles que a varredura da Causa 1 e a tabela da §3 foram feitas, e
  jogá-los fora do núcleo destruiria a capacidade de reproduzir os números que este spec cita.
  Atende a história 11.
- **Os rótulos trazem a faixa em notas**, não em Hz. É a leitura que o cantor consegue usar
  ("minha nota mais grave é um Mi2, então Low Male serve"). Atende a história 9, que na redação
  anterior estava órfã — nenhuma decisão a cobria.

**Quebra de compatibilidade, aceita e documentada.** A lista vai de 7 itens para 4, então os
índices renumeram: um projeto de DAW salvo com `Contralto` (índice 3) reabre com o que estiver
no índice 3 da lista nova. Não há como preservar quando a lista encolhe. Há precedente
documentado: a Etapa 1 quebrou o parâmetro `escala` do mesmo jeito, de 7 opções para 3
(`docs/execucao-do-plano.md:192`). O diário registra a quebra e a necessidade de reajustar a
tessitura na mão.

**Uma afirmação do TCC inverte.** `docs/comparacao-antares.md:31` registra hoje que o protótipo
é *"mais granular (7 contra 5)"*, listado como vantagem sobre o Auto-Tune. Com a lista curta
isso vira paridade, não vantagem. A linha precisa ser corrigida na mesma etapa, e a
granularidade passa a ser descrita como capacidade da **linha de comando**, que é onde ela
continua existindo.

### D5 — O padrão da tolerância vai a zero; a semântica não muda

A zona morta continua empurrando o desvio até a borda, e continua se chamando `Tolerância`.
A [Decisão 2 revista](historico-e-decisoes.md#decisão-2--renomear-tolerancia-para-flex-tune) já
resolveu isso: o mecanismo é diferente do Flex-Tune, e o texto do TCC descreve o que o
protótipo tem em vez de reivindicar paridade. Implementar Flex-Tune de verdade é o item K5 e
está fora deste spec.

O que muda é o **valor padrão**, de 15 para 0, para que a configuração de fábrica encaixe as
notas. Quem quiser a zona morta a liga.

A razão de os dois controles não se confundirem, e que vale para o texto: a **Tolerância**
decide *se* a nota chega no lugar certo; o **Retune Speed** decide *em quanto tempo*. Com
`tol = 0` a nota chega exata em qualquer valor de Retune. Suavidade é trabalho do controle que
tem dimensão de tempo — a Tolerância não suaviza nada, só deixa erro estático.

### D6 — A faixa do Retune Speed vai a 400 ms e o padrão vai a 0, depois de D2

A faixa alinha com o Auto-Tune e atende a história 23. **Depois de D2**, porque hoje o controle
satura antes dos 200 ms — a faixa maior só passa a significar algo quando o alvo parar de
piscar.

O **padrão vai de 25 ms para 0** (efeito duro), por decisão do autor: a primeira impressão do
plugin deve ser a de um corretor que encaixa a nota. O deslize continua alcançável no próprio
controle, que é como o autor quer chegar nele (histórias 19 e 20) — sem sistema de presets.

Preço a assumir, e é o que motiva D7: com `retune = 0` de fábrica, o `Natural Vibrato` e o
`Humanize` nascem inertes, e o plugin sai da caixa com dois dos quatro controles de expressão
desabilitados.

### D7 — Os controles inertes ficam desabilitados E dizem o que os destrava

Quando o Retune Speed está em zero, `Natural Vibrato` e `Humanize` não têm efeito algum. O
editor passa a desabilitá-los visivelmente nessa condição **e a exibir um texto curto** do tipo
`requer Retune Speed > 0`. Só acinzentar não basta depois de D6: o padrão de fábrica passa a
ser justamente a condição em que eles ficam mudos, e um controle apagado sem explicação vira
"o plugin está quebrado" em vez de "falta destravar".

Sem mudança de DSP e sem mudança na árvore de parâmetros: o host continua podendo automatizar
os dois, e o comportamento com Retune Speed positivo é o de hoje. É estritamente uma correção
de comunicação, e respeita a [Decisão 7](historico-e-decisoes.md#decisão-7--o-deslize-de-entrada-é-fixo-2026-08-26),
que fixa o deslize de entrada.

**Sistema de presets de fábrica foi considerado e rejeitado.** Dois presets nomeados (`Duro` e
`Natural`) tornariam a troca mais descobrível, mas exigiriam implementar
`getNumPrograms/setCurrentProgram/getProgramName`, que hoje são stubs (`PluginProcessor.h:55`).
O autor prefere alcançar o efeito **mexendo nos parâmetros**. Fica fora de escopo.

### D8 — A linha de base é regravada por etapa, com justificativa por caso

D1, D2 e D3 mudam o áudio. Os 37 casos vão mudar de checksum, e uma regravação em massa
esconderia exatamente o tipo de erro que a linha de base existe para pegar.

A regra: **uma regravação por etapa**, com a lista dos casos que mudaram e a razão de cada
mudança registrada no diário de execução. Casos que mudarem sem explicação são defeito, não
resultado.

Os quatro casos de identidade — os dois do TD-PSOLA e os dois do ponteiro — **não podem mudar
em nenhuma etapa**. Eles são o controle do experimento.

D5, D6 e D7 **não** mexem na linha de base: os CLIs recebem `tol=` e `retune=` explicitamente,
então mudar o padrão do plugin não muda um checksum sequer.

---

## 8. Etapas de entrega

Três etapas, agrupadas por **regravação de linha de base**: cada uma é uma regravação com
justificativa, como D8 exige.

| etapa | conteúdo | muda áudio? | depende de |
|---|---|---|---|
| **A** | D1 (guarda) + D4 (lista curta, rótulos, padrão `Alto-Tenor`) | **sim** | — |
| **B** | D2 (estabilização) + D5 + D6 + D7 (padrões e interface) | **sim** (D2) | A, para o padrão novo |
| **C** | D3 (teto na janela) | **sim** | nenhuma |

**A etapa C é independente** e pode ser feita em paralelo, por outra pessoa ou noutro momento.
A ordem A → B existe porque a faixa maior do Retune Speed não significa nada com o alvo
piscando, e porque o padrão de tessitura novo precisa da guarda de D1 para ser seguro.

Cada etapa fecha com: `./baseline.sh conferir` antes e depois, a lista dos casos que mudaram
com a razão de cada um, e o registro no `docs/execucao-do-plano.md`.

---

## 9. Decisões de teste

### O que faz um teste bom aqui

Um teste bom afirma sobre **o que o motor decide e entrega**, não sobre como ele chega lá.
Concretamente: alimenta áudio com altura conhecida e afirma sobre a trilha de altura detectada
e a trilha de nota-alvo, que já são interfaces públicas do núcleo de streaming. Não afirma
sobre marcas de período, colunas do Viterbi, posição de ponteiro ou conteúdo de buffer
interno — nada disso é comportamento observável, e um teste que os trave impede a próxima
correção legítima.

Onde há defeito com número medido, o teste **assevera sobre o número**, não sobre a existência
do defeito. "Zero quadros na oitava errada" e "mediana de duração de nota acima de X ms" são
asserções; "a detecção melhorou" não é.

### A seam

**Uma seam nova, no ponto mais alto possível:** um teste que alimenta o núcleo de streaming com
áudio sintético de altura conhecida e assevera sobre a trilha de altura detectada e a trilha de
nota-alvo, ambas já expostas pelo núcleo.

Uma seam cobre D1 e D2 juntos, e cobre os dois motores de síntese, porque os dois consomem as
mesmas duas trilhas. Confirmada com o autor antes da redação deste spec.

Ela é mais alta que a alternativa óbvia (testar a seleção de candidato de período direto no
núcleo de DSP, no estilo dos testes unitários que já existem), e a diferença importa: o piscar
da nota-alvo **não é visível** na função de seleção isolada. Ele só aparece na trilha completa.

O gerador de áudio sintético é parte da seam: altura conhecida, perfil harmônico de voz,
envelope de ataque e queda. Ele já foi escrito e usado para produzir as tabelas de detecção
deste documento — vale portá-lo do harness de diagnóstico para o teste definitivo em vez de
reescrevê-lo.

### O que cada correção assevera

**D1 — guarda contra a subharmônica.** Varredura de altura conhecida cruzando o teto de cada
preset. Acima do teto: nenhum quadro reporta altura, e nenhum reporta uma oitava abaixo. Abaixo
do teto: a altura reportada bate com a real dentro da tolerância do detector. A tabela de
varredura da §2 é o oráculo — ela hoje falha, e passa a passar.

Um segundo caso, sobre o áudio real: a divergência por uma oitava contra o preset mais largo
cai a zero nos presets graves, contra os 34,3 % e 16,2 % medidos hoje.

**D2 — estabilização do alvo.** Sobre o áudio real, na configuração do usuário: a duração
mediana de nota sobe de 41 ms, e a fração de notas com menos de 80 ms cai dos 76 % medidos. Os
limiares exatos ficam para a calibração, mas a asserção é sobre a métrica, não sobre a sensação.

Contraprova obrigatória, pela história 15: uma melodia sintética com trocas de nota rápidas e
legítimas continua sendo seguida. Sem esse caso, o teste da estabilidade é passável trancando a
nota-alvo, que seria pior que o defeito.

**D3 — teto na janela.** Uma asserção de tempo por bloco, cronometrando cada chamada de
processamento sobre áudio com notas longas: o custo por bloco **não cresce** com a duração da
nota. Não há precedente disto no repositório — os scripts de medição existentes medem taxa
agregada, não pior caso por bloco, e é o pior caso que produz o estalo.

Este é o único teste do conjunto sensível à máquina. Ele deve asseverar sobre a **ausência de
crescimento** (comparando o custo no fim de uma nota longa com o do início), que é uma
propriedade estrutural, e tratar o limite absoluto em milissegundos como diagnóstico e não como
critério de falha.

Um segundo caso, estrutural e não temporal: a invariância ao tamanho de bloco continua valendo
com o teto ativo, verificada de 1 a 4096 como o `baseline.sh` já faz.

**D4 — lista curta.** Um teste sobre `presetVoz()` afirmando que os nove nomes continuam
resolvendo para as mesmas faixas (a história 11: a GUI encolhe, o núcleo não). A lista da GUI e
os rótulos são **verificação visual**, registrada no diário — assim como a quebra de índices em
projetos salvos, que não tem teste automatizado possível.

**D5, D6, D7.** A mudança de padrão de tolerância e a de faixa do Retune Speed são cobertas
pelos testes de expressão que já existem, estendidos aos novos limites. A desabilitação de
controle e o texto explicativo na interface não recebem teste automatizado — é verificação
visual, registrada no diário.

### Prior art no repositório

Os testes unitários existentes sobre a malha de correção são o modelo de estilo: oráculo
congelado do comportamento anterior, comparação exata onde a etapa promete não regredir,
comparação por métrica onde ela promete melhorar. A técnica de congelar o comportamento
anterior como oráculo é diretamente aplicável a D2 — o comportamento de hoje, com o alvo
piscando, é o oráculo do que **não** deve mais acontecer.

Os quatro casos de identidade da linha de base são a rede de segurança de tudo, e rodam antes e
depois de cada etapa.

---

## 10. Fora de escopo

- **Escalas.** Cromática, maior e menor natural são as três que o plugin tem e as três que o
  autor quer. O pedido que parecia ser de escalas era de **Input Type**, e está em D4. Menor
  harmônica e melódica ficam registradas como possibilidade futura, sem demanda.
- **Sistema de presets de fábrica.** Considerado em D7 e rejeitado: o autor prefere alcançar o
  efeito mexendo nos parâmetros.
- **`Bass Instrument`.** Não existe no núcleo de DSP. Entra num spec de presets, se entrar.
- **Implementar o Flex-Tune de verdade.** É o item K5, com mecanismo oposto ao da `Tolerância`
  atual, e tem spec próprio a escrever.
- **A cadeia de marcas incremental do TD-PSOLA.** Registrada em D3 como a solução tecnicamente
  superior e como trabalho futuro. Não é feita agora pelo risco descrito lá.
- **Baixar `RES_CENTS` de 20 para 10.** Registrado em D2 com o custo (~4× no Viterbi). Não
  feito.
- **A alocação dentro do callback de áudio.** Real e já documentada, mas **o impacto isolado
  não foi medido**. Está na mesma thread do custo do TD-PSOLA e provavelmente é secundária a
  ele. Deve ser medida depois de D3, quando o ruído maior sair da frente, e tratada com o
  número em mãos.
- **A parte variável da latência do motor de ponteiro** (a distância entre ponteiros, que não é
  declarada ao host). Comportamento conhecido e registrado na especificação do v3.
- **O filtro-pente do mix intermediário no motor de ponteiro.** Consequência estrutural da
  distância variável, sem correção que preserve a baixa latência. Registrado, não resolvido
  aqui.
- **Escolher os valores finais de histerese, permanência mínima e teto de janela.** Este spec
  fixa os mecanismos, as faixas de partida e as asserções; os números saem da calibração com
  medição e escuta.
- **Revalidar a naturalidade e a latência com usuário.** As duas continuam pendentes de escuta
  e são item próprio.

---

## 11. Notas adicionais

**A pergunta aberta sobre a tessitura do autor está fechada.** A §3 traz a medição: E3–A4,
`Alto-Tenor` cobre 100 %, `Low Male` não cobre. Ela era a única incógnita da redação anterior
que bloqueava uma decisão.

**Sobre a ordem.** D1 antes de D4 porque D4 depende dela. D2 antes de D6 porque a faixa maior do
Retune Speed não significa nada com o alvo piscando. D3 é independente das outras duas.

**Um documento precisa ser corrigido junto.** `docs/comparacao-antares.md:31` afirma que o
protótipo é mais granular que o Auto-Tune em tessituras (7 contra 5). Com D4 isso deixa de
valer na interface e passa a valer só na linha de comando. Corrigir na Etapa A.

**Sobre o valor para o texto do TCC.** As três causas têm a mesma forma, e vale que o texto a
explore: um mecanismo correto dentro da sua faixa de validade, aplicado fora dela sem guarda.
A busca de período é correta acima de `tauMin` e mente abaixo. O filtro do Retune Speed é
correto quando o alvo é estável e enjoa quando não é. A janela de re-síntese é correta enquanto
a região vozeada é curta e explode quando não é. Nenhuma das três é um erro de fórmula, e é por
isso que as três passaram pela linha de base sem serem detectadas — a linha de base verifica
reprodutibilidade, não validade.

**Um segundo achado que vale o texto.** A resolução da **análise** põe um piso na resolução da
**decisão**: com bins de 20 cents, uma histerese menor que 20 cents é inoperante por
construção. É uma amarração não óbvia entre duas camadas que o desenho tratava como
independentes, e ela reaparece no custo — melhorar a resolução da análise custa ~4× no Viterbi.

**Sobre o que a linha de base não pega.** Vale registrar no diário: 37 casos passando por
checksum nunca teriam encontrado nenhum destes três defeitos, porque todos os três são
**estáveis e reprodutíveis**. Um teste de regressão prova que o comportamento não mudou; ele
não prova que o comportamento está certo. As asserções sobre altura conhecida deste spec são o
primeiro teste do repositório que verifica a segunda coisa.
