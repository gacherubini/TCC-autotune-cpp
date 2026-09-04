# Teste de usuário — protótipo de autotune em tempo real

**TCC · PUCRS · Desenvolvimento de um protótipo gratuito de correção automática de afinação vocal**

Registro do teste de validação do protótipo com usuário real. Este documento descreve
**o que foi testado, o que foi observado e quais requisitos não foram atendidos**. Ele não
propõe soluções — o encaminhamento técnico está em
[`documentacao-tecnica.md`](documentacao-tecnica.md).

### Posição no cronograma do TCC

Este documento é a materialização da entrega da **Sprint 10** do cronograma do próximo
semestre (Tabela `tab:cronograma_proximo`, Capítulo `chap:cronograma` do TCC 1):

> *Sprint 10 · 10/08 – 23/08 · Estudo com usuários (sessões) — Sessões de uso do protótipo
> com os participantes; comparação com ferramentas consolidadas (Auto-Tune, Melodyne);
> registro das percepções de uso.*
> **Entrega esperada: sessões realizadas e dados qualitativos coletados.**

A sistematização do feedback e a priorização das melhorias — entrega da **Sprint 11**
(24/08 – 06/09) — estão no documento técnico complementar.

O teste também dá continuidade direta ao que o TCC 1 já havia registrado como pendência:
a Seção *Continuidade do trabalho* previa "(i) a avaliação com usuário, conduzindo um estudo
qualitativo de uso" e "(ii) o refinamento da naturalidade sonora e da usabilidade, a partir
do retorno obtido".

---

## 1. Objetivo do teste

O protótipo havia sido validado até então apenas por **métricas objetivas offline**:
correlação com a saída de referência, ausência de cliques, identidade em `forca = 0`,
preservação de formantes e fator de tempo real (xRT). Todas essas verificações passaram.

O teste de usuário existia para responder a uma pergunta que nenhuma dessas métricas
responde:

> **O protótipo é utilizável por um cantor, ao vivo, dentro de uma DAW?**

Essa é a pergunta que o TCC se propôs a responder, e é uma pergunta **perceptual**, não
numérica. Foi por isso que o teste foi incluído no plano de trabalho.

---

## 2. Configuração do teste

### Ambiente

| Item | Valor |
|---|---|
| DAW / host | Ableton Live (Windows) |
| Formato do plugin | VST3 (`TCC Autotune.vst3`) |
| Modo de uso | monitoração **ao vivo**, cantando com fone |
| Fonte | voz cantada, microfone → interface de áudio |

> **A preencher no texto final do TCC:** data do teste, modelo da interface de áudio,
> driver utilizado (ASIO/WASAPI), *buffer size* configurado no host, repertório cantado
> e perfil do usuário de teste (classificação vocal, experiência).

### Parâmetros do plugin

O teste foi conduzido com os **valores de fábrica** do plugin:

| Parâmetro | Valor | Tipo |
|---|---|---|
| Forca | 1,0 | ao vivo |
| Tolerancia | 15 cents | ao vivo |
| Glide | 40 ms | ao vivo |
| Look-ahead | 4 quadros | estrutural |
| Voz (tessitura) | Contralto (FMIN 175 Hz, FMAX 698 Hz) | estrutural |
| Escala | Cromática | estrutural |

Esses valores correspondem ao "preset natural" recomendado no README do repositório C++
(`forca 1.0  tol=15  glide=40`), acrescido do look-ahead e da tessitura padrão do plugin.

---

## 3. O que funcionou

O teste **não** foi um fracasso: o protótipo executou a tarefa a que se propunha. Registrado
para que o texto do TCC não perca esses resultados:

- **O plugin carrega, instancia e roda de forma estável no Ableton Live**, em VST3, sem
  travar o host e sem exigir reinicialização.
- **A correção de afinação acontece e é audível.** As notas são efetivamente levadas para
  a grade temperada, com a escala e a força configuradas.
- **Não há cliques nem "pipoco".** O bug de descontinuidade na síntese, documentado e
  resolvido durante o desenvolvimento, não reapareceu em uso real.
- **O timbre da voz é preservado.** Não há efeito "esquilo" nem alteração perceptível de
  identidade vocal, coerente com a propriedade do TD-PSOLA de preservar formantes.
- **Sobra capacidade de processamento.** O host não reportou sobrecarga de CPU durante o
  teste, coerente com o xRT de 0,025 medido offline.

---

## 4. Achado 1 — Latência alta demais para monitoração ao vivo

### Observação

Ao cantar monitorando pelo fone com o plugin inserido na cadeia, o usuário de teste
relatou **atraso claramente perceptível entre a emissão da voz e o retorno no fone**, a
ponto de **atrapalhar a execução**. O atraso interfere no controle motor do canto: o
cantor ouve a si mesmo fora de sincronia e tende a desestabilizar a afinação e o tempo —
o oposto do que a ferramenta deveria produzir.

### Quantificação

| Fonte do número | Valor |
|---|---|
| Relato do usuário no teste | ~60 ms |
| Orçamento algorítmico calculado pelo próprio núcleo | **57,9 ms** (2552 amostras @ 44,1 kHz) |

O orçamento algorítmico é reportado pelo próprio motor de streaming e declarado ao host
via `setLatencySamples()`. Ele decompõe-se em três parcelas:

```
latência = frame + look·hop + 2·fs/FMIN
         = 1024  + 4·256    + 2·(44100/175)
         = 1024  + 1024     + 504
         = 2552 amostras = 57,9 ms
```

O relato do usuário (~60 ms) é **consistente** com o número calculado, o que valida a
contabilidade de latência implementada no núcleo.

### Ressalva metodológica importante

O número acima é a **latência algorítmica**. O que o cantor efetivamente ouve é a
**latência de ida-e-volta (round-trip)**:

```
round-trip = driver_entrada + bloco_do_host + latência_algorítmica + driver_saída
```

Ou seja, o valor percebido é necessariamente **maior** que 57,9 ms. As parcelas de driver e
de bloco do host não foram medidas neste teste.

Além disso, o *Plugin Delay Compensation* (PDC) do Ableton, alimentado por
`setLatencySamples()`, **não resolve este problema**: o PDC alinha o material **gravado**
com as demais faixas da sessão, mas não pode remover um atraso que o cantor escuta em tempo
real. Essa distinção precisa estar explícita no texto do TCC, porque é uma fonte comum de
confusão.

### Referência de comparação

O usuário de teste comparou espontaneamente o comportamento com o do **Antares Auto-Tune**,
relatando latência de ordem de grandeza muito menor (impressão relatada: poucos
milissegundos, "cerca de 3 ms").

> **Status:** este número é um **relato**, não uma medição. Nenhuma medição instrumentada
> do Auto-Tune foi realizada. Para uso no texto do TCC, o valor precisa ser (a) medido
> diretamente por *loopback* na mesma cadeia de áudio, ou (b) citado com referência à
> documentação do fabricante. Comparar o protótipo com um número não verificado
> enfraqueceria a discussão.

### Critério de referência

O comentário registrado no script `06_realtime_benchmark.py` do repositório Python já
estabelecia a meta durante o desenvolvimento:

> *"autotune ao vivo tolera no máximo ~20–30 ms antes de incomodar o cantor"*

**O protótipo entrega aproximadamente o dobro desse limite.**

> **A preencher no texto final do TCC:** este limiar de 20–30 ms precisa de citação
> bibliográfica (literatura de percepção de latência em monitoração vocal). Hoje ele
> aparece apenas como comentário de código.

---

## 5. Achado 2 — Resultado sonoro estático, duro e robótico

### Observação

O segundo achado é qualitativo e independente do primeiro. O usuário de teste descreveu a
saída corrigida como:

- **"sem cor"** — ausência de variação expressiva;
- **"muito estático e duro"** — a afinação de saída não se move como a de um cantor;
- **"bem robótico"** — o resultado soa sintético, mesmo com `tolerancia = 15` e
  `glide = 40 ms` ativos, que são justamente os parâmetros criados para evitar isso.

Este é o achado mais relevante do teste, porque **contradiz uma verificação anterior**: os
parâmetros `tol` e `glide` foram desenvolvidos e validados especificamente para "tirar o
efeito robô", e as verificações objetivas indicavam sucesso (com `tol = 15`, um desvio de
−14 cents permanece praticamente intacto; o glide produz portamento mensurável entre
notas). **A validação objetiva passou e a validação perceptual falhou.**

Essa divergência é, por si só, um resultado metodológico do trabalho: as métricas usadas
até então — correlação com a saída offline, contagem de cliques, identidade em `forca = 0` —
**medem fidelidade a uma referência, não qualidade percebida**. Um sistema pode reproduzir
perfeitamente sua própria referência e ainda assim soar mal.

### Hipótese levantada durante a sessão

O usuário de teste formulou a seguinte hipótese sobre a causa:

> *"Ele bate direto na nota em vez de dar uma mudada."*

Isto é: o sistema levaria a afinação ao valor alvo **instantaneamente**, sem reproduzir o
percurso que um cantor real faz até a nota (ataque por baixo, acomodação, vibrato). A
ausência desse percurso seria percebida como rigidez.

> **Status:** hipótese formulada durante a sessão de teste, a partir da escuta. A
> verificação dessa hipótese contra o código-fonte, e a identificação dos mecanismos
> responsáveis, estão documentadas em
> [`documentacao-tecnica.md`](documentacao-tecnica.md), seção "Diagnóstico".

### Nota sobre escopo

Cabe registrar uma distinção conceitual que o teste evidenciou. O que produtos comerciais
chamam de "cor" (*Throat Length*, *Formant*, *Humanize* no Auto-Tune) são **processos
adicionais de timbre**, não subprodutos da correção de afinação. O protótipo é, por
escopo declarado, um **corretor de afinação** — ele não contém nenhum estágio de
processamento de timbre. Parte da percepção de "falta de cor" pode decorrer dessa
diferença de escopo, e não de um defeito de implementação. Separar as duas coisas é
necessário antes de decidir o que muda no TCC 2.

---

## 5-bis. Achado 3 — Cobertura de tonalidades insuficiente

> **Registrado retroativamente em 2026-08-26**, a partir do relato do autor sobre a
> preparação da sessão. Não foi anotado na redação original deste documento porque, no
> momento do teste, foi tratado como inconveniência de configuração e não como resultado.
> **É um resultado** — e provavelmente o mais concreto dos três.

### Observação

Para realizar o teste foi necessário **procurar um instrumental que estivesse em uma das
tonalidades disponíveis no plugin**, em vez de escolher livremente o material musical e
configurar o plugin para acompanhá-lo.

Isto é uma inversão da relação normal entre ferramenta e uso: a ferramenta impôs uma
restrição ao repertório em vez de se adaptar a ele.

### Causa

O combo `Escala` do plugin oferece **7 opções fixas**
(`plugin/PluginProcessor.cpp:30`):

```
Cromatica · Do maior (C) · La menor (Am) · Sol maior (G)
Mi menor (Em) · Fa maior (F) · Re menor (Dm)
```

São **6 tonalidades** mais o modo cromático. Nenhuma tonalidade com mais de um sustenido
ou bemol está disponível — ficam de fora Ré maior, Lá maior, Mi maior, Si bemol maior,
Mi bemol maior e todas as menores correspondentes.

**O motor não tem essa limitação.** `definirEscala()` (`src/core/dsp.h:112`) calcula as
classes de nota permitidas para **qualquer** tônica. A restrição é exclusivamente da
interface.

### Por que isto importa mais do que parece

1. **É o único dos três achados com causa trivial e conserto sem risco.** Não envolve DSP,
   não envolve trade-off, não precisa de teste de escuta para validar.
2. **Afeta a validade do próprio teste.** O material musical não foi escolhido pelo mérito
   musical, mas pela compatibilidade com a ferramenta — o que é, em si, uma limitação
   metodológica da sessão (ver §6, item 7).
3. **É evidência de uso real, não de análise.** Os outros dois achados vieram da escuta e
   do código; este veio de uma pessoa tentando usar o plugin e esbarrando nele.

### Encaminhamento

Decisão registrada em
[`historico-e-decisoes.md`](historico-e-decisoes.md#redesenho-da-interface-para-paridade-com-o-auto-tune-2026-08-26),
Decisão 4: separar em `Key` (12 tônicas) × `Scale` (cromática / maior / menor natural),
cobrindo as 24 tonalidades.

---

## 6. Limitações do teste

Registradas explicitamente para que o TCC não superestime o alcance destes resultados:

1. **Amostra de um único usuário.** Não há base para generalizar a percepção.
2. **Uma única voz e uma única tessitura.** O preset utilizado (Contralto) foi adequado
   para essa voz; o comportamento com outras classificações vocais não foi observado.
3. **Sem protocolo formal.** Não houve roteiro de tarefas, escala de avaliação estruturada
   (tipo Likert ou MOS), nem teste cego A/B contra a saída seca ou contra um produto
   comercial.
4. **Sem gravação de material comparativo.** Não foram guardados áudios de entrada e saída
   da sessão que permitissem reanálise posterior.
5. **Sem medição instrumentada de latência de ida-e-volta.** O único número de latência
   disponível é o orçamento algorítmico calculado, não o atraso efetivamente medido na
   cadeia completa.
6. **Sem medição de carga de CPU ao longo do tempo.** A ausência de sobrecarga foi
   observada subjetivamente, não registrada.
7. **O material musical foi escolhido pela ferramenta, não pelo teste.** Como o plugin só
   oferecia 6 tonalidades, foi preciso procurar um instrumental compatível — ver o
   Achado 3. O repertório testado é, portanto, enviesado pela limitação da interface.

Essas limitações não invalidam os três achados — os dois primeiros foram inequívocos na
escuta, o primeiro é confirmado por cálculo, e o terceiro é verificável por inspeção do
código. Mas elas definem o que precisa ser feito melhor no TCC 2.

---

## 7. Conclusão do teste

O protótipo **atende aos requisitos funcionais** (corrige afinação, preserva timbre, não
produz artefatos, roda em tempo real numa DAW) e **não atende a três requisitos de
usabilidade**:

| # | Requisito | Situação | Evidência |
|---|---|---|---|
| R1 | Latência compatível com monitoração ao vivo | **Não atendido** | 57,9 ms de latência algorítmica; relato de ~60 ms; interferência na execução |
| R2 | Resultado sonoro natural / não-robótico | **Não atendido** | Avaliação perceptual do usuário de teste: "estático", "duro", "robótico" |
| R3 | Cobrir as tonalidades de uso musical corrente | **Não atendido** | 6 das 24 tonalidades disponíveis; foi preciso escolher o instrumental em função do plugin (§5-bis) |

> **Sobre o teto de latência do R1.** A redação original citava "≤ ~20–30 ms". A revisão
> bibliográfica de 2026-08-26 mostrou que esse número **não tem respaldo revisado por
> pares** — ele rastreia até material de marketing. Os limiares medidos para voz com
> monitoração in-ear são bem mais rigorosos. O requisito precisa ser reescrito com um
> limiar nomeado e citado; ver
> [`modo-baixa-latencia.md` §5](modo-baixa-latencia.md). **O texto original foi mantido
> acima como registro.**

### Correspondência com os requisitos formais do TCC

Os dois achados **confirmam empiricamente** o que a Tabela `tab:requisitos_atingidos` do
TCC 1 já classificava como *Parcial*. Nenhum requisito mudou de status para pior: o teste
converteu duas ressalvas teóricas em evidência de uso.

| Requisito (TCC 1) | Status declarado no TCC 1 | O que o teste de usuário acrescenta |
|---|---|---|
| **RNF01** — operar com baixa latência em contexto experimental | *Parcial* — "cadeia completa ≈ 58 ms, acima do teto de monitoração ao vivo, mas parametrizável e compensada pelo host" | **Confirmado em uso real.** O valor de 58 ms não é apenas "acima do teto": ele **impede** a execução ao vivo. A ressalva "compensada pelo host" precisa ser corrigida no texto — o PDC não compensa monitoração (ver §4). |
| **RNF03** — preservar naturalidade sonora sempre que possível | *Parcial* — "tolerância e glide implementados; validação perceptual com usuário prevista" | **Validação perceptual realizada e reprovada.** Tolerância e glide estão implementados e verificados objetivamente, mas **não produzem o efeito perceptual pretendido** (ver §5). |
| **RNF05** — simples de operar em contexto de validação | *Parcial* — "interface gráfica funcional; validação de usabilidade prevista" | **Achado negativo registrado retroativamente:** a cobertura de tonalidades obrigou a adaptar o repertório à ferramenta (Achado 3). É uma falha de usabilidade, não de DSP. |
| **RF01–RF05** (funcionais) | *Atendidos* | Mantidos. Confirmados em uso real (ver §3). |

Também se confirma, do levantamento de requisitos original (Seção *Levantamento das
necessidades do usuário*), a hierarquia declarada pelo usuário: **latência baixa** e
**naturalidade** foram apontadas desde o início como as duas prioridades — e são exatamente
as duas que falharam. O achado não é uma surpresa metodológica; é a confirmação de que o
levantamento inicial identificou corretamente o que importava.

### Encaminhamento

Esses requisitos passam a ser o **objeto do TCC 2**. O diagnóstico técnico das causas e as
alternativas de solução estão documentados separadamente em
[`documentacao-tecnica.md`](documentacao-tecnica.md); as decisões de projeto derivadas
estão em [`historico-e-decisoes.md`](historico-e-decisoes.md) e a comparação com o produto
de referência em [`comparacao-antares.md`](comparacao-antares.md).

Os três achados têm perfis de esforço muito diferentes, e vale registrar isso:

| Achado | Causa | Conserto | Risco |
|---|---|---|---|
| 1 · Latência | arquitetural (detecção por quadro) | mudança de arquitetura para chegar à faixa defensável | alto |
| 2 · Naturalidade | filtro aplicado ao sinal errado na cadeia | reposicionar o polo + acrescentar controles | médio |
| 3 · Tonalidades | montagem de combo na interface | expor o que o motor já faz | **nenhum** |

---

## 8. Recomendações para o próximo teste de usuário

Para que o teste do TCC 2 produza evidência utilizável na defesa:

- **Definir um protocolo escrito** antes da sessão: tarefas, ordem, duração.
- **Gravar entrada e saída** de todas as passagens, para reanálise e para anexar ao TCC.
- **Teste cego A/B/X**: seco × protótipo × produto comercial, com o avaliador sem saber
  qual é qual.
- **Escala de avaliação estruturada** para naturalidade e para incômodo de latência
  (por exemplo, MOS de 1 a 5 por dimensão).
- **Medir a latência de ida-e-volta** por *loopback*, com a mesma interface e o mesmo
  *buffer size*, antes da sessão.
- **Registrar carga de CPU** ao longo da sessão, especialmente durante notas longas
  sustentadas.
- **Mais de um usuário** e, se possível, mais de uma tessitura.

---

*Documento de registro do teste de usuário — TCC PUCRS, 2026.*
