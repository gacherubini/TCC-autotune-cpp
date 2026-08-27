# Como o Auto-Tune consegue 0,84 ms — e o que disso é alcançável aqui

> **Data:** 2026-08-27
> **Status:** 🔬 **PESQUISA.** Nada implementado. Este documento existe para responder a uma
> pergunta específica levantada pela medição do TCC 1: *como um produto comercial declara
> 37 amostras de latência fazendo a mesma tarefa que aqui custa 2552?*
> **Origem:** a comparação medida em `tcc1-sections.tex` §"Comparação medida com o Auto-Tune"
> **Consequência:** a §6 abre uma terceira alternativa que a
> [especificação do modo de baixa latência](modo-baixa-latencia.md) **não contemplava**.

---

## 1. O número medido, e o que ele já prova sozinho

A leitura feita no Ableton Live foi **37 amostras (0,84 ms)** para o Auto-Tune Pro contra
**2552 amostras (57,9 ms)** para o protótipo, na mesma sessão a 44,1 kHz.

O material da Antares publica a latência do modo de baixa latência do AutoTune 2026 como
**"2,3 ms Modern / 0,77 ms Classic a 48 kHz"**. Convertendo:

| Fonte | Taxa | Latência declarada | Em amostras |
|---|---|---|---|
| Medição do TCC (Auto-Tune Pro, Classic) | 44,1 kHz | 0,84 ms | 0,84 × 44100 ≈ **37** |
| Antares (AutoTune 2026, Classic) | 48 kHz | 0,77 ms | 0,77 × 48000 ≈ **37** |
| Antares (AutoTune 2026, Modern) | 48 kHz | 2,3 ms | ≈ 110 |

> ### 🎯 A dedução que interessa
> **O mesmo número de amostras nas duas taxas.** A latência do modo Classic não é 0,84 ms nem
> 0,77 ms — é **37 amostras, fixas**, e o milissegundo apenas segue a taxa de amostragem.
>
> Isso é uma afirmação forte, e ela é verificável sem acesso ao código: uma latência **fixa em
> amostras** não pode ter sido derivada do período do sinal. Se dependesse da nota mais grave
> tratável, mudaria com o preset de voz, como muda aqui. Se dependesse de um quadro de análise
> definido em milissegundos, mudaria com a taxa. Não muda com nenhum dos dois.
>
> **Logo: a latência declarada pelo Auto-Tune Classic não contém nenhum quadro de análise e
> nenhuma guarda proporcional ao período.** É o atraso de um filtro de comprimento fixo.

Essa única observação é o que reorienta toda a discussão abaixo. A pergunta deixa de ser
"como eles detectam pitch tão rápido?" — que é a pergunta errada, e que não tem resposta boa —
e passa a ser **"por que a detecção de pitch deles não atrasa o áudio?"**

---

## 2. Por que o protótipo não consegue chegar perto, hoje

A latência do protótipo é calculada explicitamente em `autotune_stream.h:82-86`:

```cpp
psolaGuard = 2 * (int)std::llround((double)fs / FMIN);
latSamples = p.nFrame + p.look * p.nHop + psolaGuard;
```

Três parcelas, com naturezas **diferentes**:

| Parcela | Valor (FMIN 80 Hz) | Valor (contralto, FMIN 175 Hz) | Natureza |
|---|---:|---:|---|
| `nFrame` — quadro de análise | 1024 | 1024 | atraso de **bloco**: o quadro precisa fechar antes de haver f0 |
| `look · nHop` — look-ahead | 4 × 256 = 1024 | 1024 | **não-causalidade pura**: usa futuro para suavizar a trilha |
| `psolaGuard` — 2 períodos | 1102 | 504 | **estrutural do PSOLA**: a janela de síntese precisa de um período *depois* da marca |
| **Total** | **3150 (71,4 ms)** | **2552 (57,9 ms)** | |

E aqui está a diferença de espécie, em uma linha:

```
Auto-Tune Classic:  latência = 37 amostras                    → constante
Protótipo:          latência = 2048 + 2·fs/FMIN               → cresce quando a voz é mais grave
```

Cantar mais grave **piora** a latência do protótipo e **não muda** a do Auto-Tune. Não é uma
diferença de ajuste; é a assinatura de duas arquiteturas distintas.

> ⚠️ **Consequência para o texto do TCC.** A frase "o protótipo introduz cerca de 69 vezes a
> latência do produto comercial" está certa, mas é fraca: ela compara dois números quando o que
> existe é uma diferença de **forma**. O fator 69 vale para o preset contralto; com o FMIN
> padrão de 80 Hz o fator é **85**. Um número que muda conforme o preset não deveria ser o
> centro da comparação — a comparação forte é *constante versus proporcional a `fs/FMIN`*.

---

## 3. Como a Antares faz — a patente diz, e a matemática fecha

A fundamentação está na patente do próprio Auto-Tune, **US 5.973.252** (Hildebrand, 1999), já
catalogada em [pesquisa-bibliografica.md](pesquisa-bibliografica.md). Dois mecanismos, e é a
**combinação** deles que produz as 37 amostras.

### 3.1 Detecção: autocorrelação recursiva, atualizada por amostra

A patente não computa uma função de diferença por quadro. Ela mantém dois acumuladores que se
atualizam **a cada amostra**, para cada lag `L` candidato:

```
E_i(L) = E_{i-1}(L) + x_i²        − x_{i-2L}²
H_i(L) = H_{i-1}(L) + x_i·x_{i-L} − x_{i-L}·x_{i-2L}
```

O período é o `L` que minimiza `E(L) − 2·H(L)`, refinado por interpolação quadrática. A busca
inicial roda em sinal decimado 8:1 (44,1 kHz → ~5,5 kHz, cobrindo 50 Hz a 2756 Hz) e, uma vez
travada, o refino roda em taxa cheia numa janela estreita de `N = 8` lags em volta do período
já conhecido.

Isso é **exatamente** o item **L6** já previsto na
[especificação do modo de baixa latência §6](modo-baixa-latencia.md) — "CMNDF recursivo em vez
de CMNDF por quadro". O que a patente acrescenta é a forma fechada da recursão e a estratégia
de duas fases (aquisição decimada, rastreio em taxa cheia).

> **O que isso resolve e o que não resolve.** Resolve o custo de CPU e o **atraso de bloco**:
> não existe mais "esperar o quadro fechar", porque a estimativa é atualizada continuamente.
> **Não resolve** — e não poderia — a necessidade física de observar ~2 períodos de sinal antes
> de afirmar que ele é periódico. A janela de 2 períodos continua lá, dentro dos acumuladores.
> **A estimativa continua ~2 períodos atrasada.** O que muda é *quem paga* esse atraso.

### 3.2 Correção: ponteiro de leitura móvel, não análise-e-ressíntese

Este é o ponto que o projeto ainda não tinha registrado, e é o que realmente explica o número.

A patente **não faz PSOLA**. Ela não segmenta o sinal em quadros, não sintetiza janelas e não
as soma com sobreposição. Ela faz o seguinte:

1. O sinal de entrada é escrito continuamente num **buffer circular** (ponteiro de escrita).
2. Um **ponteiro de leitura** percorre esse mesmo buffer a uma taxa `Resample_Rate` ≠ 1.
   Reamostrar a leitura é o que desloca a altura — é um efeito Doppler controlado.
3. Como os dois ponteiros andam a taxas diferentes, eles se aproximam ou se afastam. Quando a
   distância chega ao limite, **soma-se ou subtrai-se exatamente um período** do ponteiro de
   leitura: um ciclo é repetido (para subir a altura) ou descartado (para baixá-la).
4. O salto é de **um período inteiro**, então as duas pontas da emenda estão em fase — é por
   isso que não estala. É a mesma ideia que sustenta o PSOLA, aplicada sem precisar de quadros.
5. Entre amostras do buffer, interpola-se (a patente lê em `Output_addr − 5`).

O ponto crucial de arquitetura:

> **O áudio nunca espera pela análise.** Ele atravessa o buffer continuamente. A estimativa de
> período **não está no caminho do áudio** — ela apenas *dirige* o ponteiro de leitura, como um
> volante. Um volante que responde 20 ms atrasado não atrasa o carro; ele o faz curvar tarde.

### 3.3 De onde vêm, então, as 37 amostras

Sobra apenas o que é irredutível num caminho de áudio causal com reamostragem:

| Componente | Ordem de grandeza |
|---|---|
| Atraso de grupo do interpolador fracionário / filtro anti-imagem da reamostragem | ~16 a 32 amostras |
| Atraso do filtro de decimação usado na fase de aquisição | ~4 amostras |
| Margem para o ponteiro de leitura não ultrapassar o de escrita ao **subir** a altura | poucas amostras |
| **Total** | **~37** |

A terceira linha merece atenção porque é a única sutileza real: **subir a altura faz o ponteiro
de leitura andar mais rápido que o de escrita**, e ele tentaria ler amostras que ainda não
chegaram. A patente resolve subtraindo um período inteiro assim que isso ameaça acontecer — o
ponteiro nunca alcança a escrita, e a margem necessária é de **poucas amostras**, não de um
período. É isto que impede a latência de escalar com `1/FMIN`.

---

## 4. A ideia central, isolada

Vale destacá-la porque é a contribuição conceitual desta pesquisa, e porque ela **não aparece**
em nenhum documento anterior do projeto:

> ### Atraso de detecção ≠ atraso do áudio
>
> Os dois sistemas precisam observar ~2 períodos antes de conhecer o pitch. A diferença é
> **onde esse tempo é cobrado**:
>
> | | O que a demora na detecção causa |
> |---|---|
> | **PSOLA por quadros** (protótipo) | a **saída de áudio** é adiada — a ressíntese depende do quadro analisado |
> | **Ponteiro móvel** (Auto-Tune) | a **correção** chega atrasada — o ataque da nota é corrigido com o período da nota anterior |
>
> São dois defeitos diferentes, e o segundo é muito mais barato: um erro de afinação
> transitório de ~20 ms no ataque é **mascarado pelo próprio transiente**, ao passo que 58 ms
> de atraso são audíveis o tempo todo, em tudo.
>
> Esse é, provavelmente, o motivo real de o Auto-Tune "errar" o começo de notas atacadas de
> longe — um comportamento conhecido dos usuários e que costuma ser atribuído ao Retune Speed.
> Parte dele é o preço arquitetural da latência baixa.

---

## 5. A comparação não é gratuita — o que o modo Classic abre mão

Para que a comparação não fique injusta com o protótipo nem generosa demais com o produto:

| | |
|---|---|
| O modo Classic é o motor do **Auto-Tune 5** (1998–2005), reeditado como emulação vintage. É a patente de 1999, não o estado da arte da própria Antares. | |
| **Flex-Tune e o modo HQ não existem no Classic.** O HQ só roda em Modern. | |
| O modo Modern de baixa latência custa **~110 amostras** (2,3 ms a 48 kHz) — três vezes o Classic. | |
| O modo de **alta qualidade**, que a Antares não publica, é a configuração que o produto usa em mixagem. O Auto-Tune EFX+ declara **58,2 ms** fora do modo de baixa latência. | |

> 📌 **Uma coincidência que vale citar no TCC.** Os **58,2 ms** do Auto-Tune EFX+ em modo normal
> e os **57,9 ms** do protótipo são praticamente o mesmo número. Isso é evidência a favor, não
> contra, o trabalho: uma arquitetura de análise-e-ressíntese por quadros com qualidade de
> mixagem **converge para essa ordem de grandeza**, e o protótipo está na faixa certa **para o
> que ele é**. O produto comercial não é mais rápido por ser melhor no mesmo jogo — ele tem
> **dois motores**, e troca de motor quando a latência importa.
>
> Essa é uma leitura muito mais defensável na banca do que "somos 69 vezes mais lentos".

---

## 6. O que é alcançável aqui — três cenários, agora com um terceiro

A [especificação do modo de baixa latência](modo-baixa-latencia.md) contemplava v1 e v2. Esta
pesquisa mostra que **os dois têm um piso** que nenhum ajuste de parâmetro atravessa, e revela
um v3 que a especificação não considerava.

| | v1 (parâmetros) | v2 (detecção) | **v3 (síntese)** |
|---|---|---|---|
| Mudanças | `look=0`, `nFrame` por preset, guarda 2→1 período | v1 + CMNDF recursivo (L6) | v2 + **trocar o PSOLA por ponteiro móvel** |
| `nFrame` no caminho do áudio | 512 | **0** | 0 |
| `look` | 0 | 0 | 0 |
| Guarda | 1 período | 1 período | **~32 amostras fixas** |
| Latência (contralto, FMIN 175) | 17,3 ms | 5,7 ms | **~0,8 ms** |
| Latência (padrão, FMIN 80) | ~23 ms | **12,5 ms** | **~0,8 ms** |
| Cruza o limiar de coloração (7–13 ms)? | ❌ | ⚠️ **só no contralto** | ✅ sempre |
| Muda o DSP? | não | sim (detecção) | **sim (síntese — o núcleo)** |
| Esforço | baixo | alto | **alto** |

### 6.1 O achado que muda o planejamento

Repare na linha do FMIN padrão. **O v2 chega a 12,5 ms com voz grave** — dentro da faixa
7–13 ms onde Marentakis *et al.* situam o início da coloração tímbrica, e portanto **na
fronteira, não abaixo dela**. Os 5,7 ms da especificação são do preset contralto.

A razão é aritmética e não tem saída dentro da arquitetura atual: depois que o `nFrame` e o
`look` saem, **o que sobra é a guarda do PSOLA**, e ela é `fs/FMIN` por construção — 551
amostras a 80 Hz. Enquanto a síntese for PSOLA, **a latência será proporcional ao período da
nota mais grave que o sistema aceita**. É um piso arquitetural, não um parâmetro.

> **Portanto:** o v2 entrega a maior parte do ganho e é o teto do caminho já planejado. Passar
> dele **exige trocar o motor de síntese** — e é aí, e só aí, que o número da Antares vira
> alcançável.

### 6.2 O que o v3 custaria de verdade

Não é uma otimização; é substituir o estágio de correção inteiro. Honestamente:

**A favor**
- O ponteiro móvel é **mais simples** que o PSOLA — um buffer circular, um ponteiro fracionário,
  um interpolador e um teste de distância. Não há marcas de análise, busca por correlação,
  janelamento, sobreposição-soma nem normalização por pico. **Vários dos bugs já caçados neste
  projeto deixam de existir por construção**: o drift de fase, a normalização por janela
  (Achado 3, §8.3) e a janela de re-síntese que cresce sem limite (Achado 1) são todos
  patologias de análise-e-ressíntese.
- Também elimina a alocação dentro do callback de áudio (Achado 2): o buffer circular é
  pré-alocado por definição.
- A detecção de pitch **não muda de algoritmo** — o pYIN continua servindo, só passa a rodar ao
  lado do caminho de áudio em vez de à frente dele.

**Contra**
- **Perde a preservação de formantes**, que é hoje a justificativa declarada da escolha do
  TD-PSOLA. Reamostrar desloca o envelope espectral junto com a altura. Para as correções
  pequenas de um autotune (< 1 semitom quase sempre) o deslocamento de formante é da ordem de
  6 %, provavelmente inaudível — **mas isso precisa ser medido, não suposto**, e o repositório
  já tem `formantes.py` para medir.
- **Invalida toda a linha de base.** Os 17 casos do `baseline.sh`, as comparações C1 × gold e a
  trilha de F0 comparam contra um motor que deixaria de existir nesse caminho. O v3 teria de
  ser um **motor paralelo**, selecionável, não uma substituição — o que aliás é exatamente o
  que a Antares faz ao ter Classic e Modern.
- **Escopo.** Isto é um segundo algoritmo de correção de altura completo. Como trabalho de TCC
  é defensável e até atraente — a comparação medida entre **duas** arquiteturas de síntese, na
  mesma base de código e com o mesmo detector, é uma contribuição bem mais forte do que
  otimizar parâmetros de uma só. Mas é escopo de capítulo, não de sprint.

---

## 7. O que ainda não está respondido

Nenhuma destas perguntas foi resolvida por esta pesquisa, e todas afetam a decisão:

1. **As 37 amostras são mesmo atraso de interpolador?** A decomposição da §3.3 é inferência a
   partir da patente e do valor fixo em amostras, **não** um dado publicado. Verificável por
   *loopback* no próprio Auto-Tune (clique → gravar entrada e saída → correlacionar), que é a
   questão 5 já aberta na [especificação §8](modo-baixa-latencia.md). **Vale fazer:** confirma
   ou derruba a tese central deste documento, e dá um número medido pelo autor em vez de citado.
2. **Quanto de formante o ponteiro móvel desloca, na faixa de correção real deste projeto?**
   Mensurável hoje, sem implementar nada: basta reamostrar um trecho vozeado por ±100 cents e
   rodar `formantes.py`. **É o teste mais barato de todos e destrava a decisão do v3.**
3. **O erro de ataque do v3 é aceitável?** A correção chega ~2 períodos atrasada. Só a escuta
   responde — e é a mesma escuta que as Etapas 3 a 5 já estão esperando.
4. **O v2 sozinho basta?** Se a resposta da escuta for que 12,5 ms já serve para monitoração
   com fone fechado, o v3 vira trabalho futuro e o TCC fecha com o v2. A literatura citada no
   texto **não decide isso**: mediu intra-auricular e caixa, não fone fechado circum-auricular.

> **Ordem sugerida:** questão 2 (uma tarde, e é a que pode matar o v3), depois questão 1
> (uma sessão de medição, e rende figura para o texto), e só então decidir entre v2 e v3.

---

## 8. Fontes

| Fonte | Onde | O que sustenta |
|---|---|---|
| Hildebrand, H. A. **US 5.973.252** — *Pitch detection and intonation correction apparatus and method* (1999) | [patents.google.com](https://patents.google.com/patent/US5973252A/en) | §3.1 (recursão de `E` e `H`, decimação 8:1, `N = 8`), §3.2 (ponteiro móvel, inserção/remoção de ciclo, `Output_addr − 5`) |
| Antares — especificação de latência do AutoTune 2026 | [help.antarestech.com — Best Practices](https://help.antarestech.com/hc/en-us/articles/42858099043092-AutoTune-2026-Best-Practices) · [FAQ](https://help.antarestech.com/hc/en-us/articles/42855736822932-AutoTune-2026-FAQ) | §1 (2,3 ms Modern / 0,77 ms Classic a 48 kHz; HQ só em Modern) |
| Antares — AutoTune 2026, página de produto | [antarestech.com](https://www.antarestech.com/products/pitch-correction/at2026) | §1, §5 (dois motores; troca de modo) |
| Antares — Auto-Tune EFX+, especificação | [KVR Audio, tópico de latência](https://www.kvraudio.com/forum/viewtopic.php?t=535495) | §5 (58,2 ms em modo normal, 2,5 ms em baixa latência) |
| Juillerat, N.; Schubiger-Banz, S.; Arisona, S. M. — *Low latency audio pitch shifting in the time domain*, **ICALIP 2008** | [Semantic Scholar](https://www.semanticscholar.org/paper/Low-latency-audio-pitch-shifting-in-the-time-domain-Juillerat-Schubiger-Banz/9fa827c688688157cc93ec9b296f24a45c6dfdf6) · [PDF](https://scholar.archive.org/work/fq6pmvqqyjbdhogunszgkefvu4/access/wayback/http://diuf.unifr.ch/main/pai/sites/diuf.unifr.ch.main.pai/files/publications/2008_Juillerat_Schubiger-Banz_Mueller_Low_Latency.pdf) | §6 — **fonte revisada por pares** para a tese de que deslocamento de altura no domínio do tempo atinge latência muito menor que o vocoder de fase |
| van Aerde, K. — *Low latency pitch shifting* | [katjaas.nl](https://www.katjaas.nl/pitchshiftlowlatency/pitchshiftlowlatency.html) | §3.2, §3.3 (por que o ponteiro fica perto da escrita; a margem para subir a altura; o artefato ser modulação de amplitude, não clique) |
| Discussão técnica sobre o piso da detecção (~2 períodos) | [JUCE Forum](https://forum.juce.com/t/lowest-latency-real-time-pitch-detection/51741) | §3.1, §4 (o piso de ~2 períodos vale para **qualquer** detector; é o que torna a §4 não-trivial) |

> ⚠️ **Grau de evidência, declarado como manda o padrão deste projeto.** §1, §2 e §3 são
> **fortes**: número medido pelo autor, especificação do fabricante e patente. §3.3 e §4 são
> **inferência fundamentada** — a decomposição das 37 amostras não é publicada, e a §7.1 diz
> como confirmá-la. §6 é **projeção**: os números do v3 são estimativa de ordem de grandeza,
> não medição.

---

## 9. Resumo em cinco linhas

1. As 37 amostras são **fixas em amostras**, nas duas taxas — logo não contêm quadro de análise
   nem guarda proporcional ao período.
2. A patente do Auto-Tune corrige altura com **ponteiro de leitura móvel sobre buffer
   circular**, não com análise-e-ressíntese; o áudio nunca espera pela detecção.
3. Os dois sistemas têm o mesmo piso físico de ~2 períodos para detectar pitch. A diferença é
   que ali ele vira **erro de ataque** e aqui vira **atraso**.
4. O plano v1/v2 já existente tem um teto: sem o `nFrame` e o `look`, sobra a guarda do PSOLA,
   que é `fs/FMIN` — **12,5 ms com FMIN de 80 Hz**, e isso não é ajustável.
5. Chegar a ~1 ms exige trocar o motor de síntese (v3). Antes de decidir, medir o deslocamento
   de formante da reamostragem — é uma tarde de trabalho e pode encerrar a questão.
