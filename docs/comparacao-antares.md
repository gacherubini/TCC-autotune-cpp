# Comparação com o Antares Auto-Tune — análise de paridade de funcionalidades

> **Data:** 2026-08-26
> **Referência:** Antares Auto-Tune **Artist**, view ADVANCED (captura de tela analisada
> em 2026-08-26).
> **Status:** análise concluída. As decisões que saíram dela estão registradas em
> [historico-e-decisoes.md](historico-e-decisoes.md#redesenho-da-interface-para-paridade-com-o-auto-tune-2026-08-26).
> **Este documento não propõe implementação** — ele estabelece *o que existe hoje* e *o que falta*.

---

## 1. Por que esta comparação existe

O teste de usuário (ver [teste-de-usuario.md](teste-de-usuario.md)) reprovou dois requisitos:
latência e naturalidade. Ao investigar o segundo, apareceu uma pergunta anterior a "como
melhorar o som": **o protótipo tem os controles que um corretor de afinação precisa ter?**

A resposta curta é: **o núcleo de DSP está completo, a interface não está.** Nada do que falta
é o autotune em si — o que falta é higiene de plugin e controles de expressão. Este documento
detalha isso controle a controle.

---

## 2. Comparação controle a controle

Controles do Auto-Tune Artist (view ADVANCED) contra os do protótipo
(`plugin/PluginProcessor.cpp:57-73`).

| Auto-Tune Artist | Protótipo hoje | Situação |
|---|---|---|
| **INPUT TYPE** (Soprano / Alto-Tenor / Low Male / Instrument / Bass Inst.) | `Voz` — 7 presets SATB | ✅ equivalente, e o protótipo é **mais granular** (7 contra 5) |
| **KEY** (12 tônicas) + **SCALE** | `Escala` — 7 combos fixos | ⚠️ **lacuna** — ver §3 |
| **RETUNE SPEED** (ms) | `Glide` (ms) | ⚠️ estrutura certa, sinal errado — ver §4 |
| **FLEX-TUNE** (cents) | `Tolerancia` (cents) | 🔴 **NÃO são equivalentes** — mecanismos opostos, ver nota abaixo |
| **HUMANIZE** | — | ❌ ausente (é o item C1-b do backlog) |
| **NATURAL VIBRATO** | — | ❌ ausente |
| **TRACKING** | fixo no HMM | ❌ não exposto |
| **TRANSPOSE** (semitons) | — | ❌ ausente (barato: o PSOLA já desloca altura) |
| **DETUNE** (cents, afinação de referência) | — | ❌ ausente (A4 = 440 Hz cravado em `dsp.h:160`) |
| **THROAT** (modelagem de trato) | — | 🔵 fora do escopo — e **comprovadamente inútil aqui** (ver pesquisa) |
| **CLASSIC / FORMANT** | TD-PSOLA preserva formantes por construção | 🔵 fora do escopo declarado |
| **HOLD** | — | 🔵 fora do escopo declarado |
| **Teclado MIDI + edição por nota** (REMOVE / BYPASS, LATCH / MOMENTARY) | — | 🔵 `acceptsMidi() = false` (`PluginProcessor.h:48`) |
| Display de nota + medidor de cents | `TunerDisplay`, **duas agulhas** | ✅ o protótipo mostra *antes* **e** *depois* |
| — | `Look-ahead` (0–16 quadros) | ➕ o Auto-Tune **não expõe** isso |
| Mix seco/molhado | — | ❌ ausente (item C4 do backlog) |
| — | `Forca` (0–1) | ➕ o Auto-Tune **não tem** equivalente — ver §4. ⚠️ **decidido remover** |

Legenda: ✅ paridade · ⚠️ existe com ressalva · 🔴 erro corrigido · ❌ ausente e relevante ·
🔵 ausente e fora do escopo · ➕ o protótipo tem e o Auto-Tune não.

> 🔴 **Correção de 2026-08-26 — `Tolerancia` ≠ Flex-Tune.**
> A versão anterior desta tabela afirmava que os dois eram o mesmo controle com nomes
> diferentes. **Está errado.** A leitura do manual oficial mostrou que são mecanismos
> **opostos**: o `tol` não corrige *perto* da nota; o Flex-Tune não corrige *longe* dela.
> Detalhes e citação verbatim em
> [pesquisa-retune-speed-e-cor.md §2](pesquisa-retune-speed-e-cor.md).
> A decisão de renomear foi **cancelada** por causa disso.

---

## 3. A lacuna das tonalidades

**O motor já suporta as 24 tonalidades. A interface expõe 6.**

Em `src/core/dsp.h:112`, `definirEscala()` aceita qualquer tônica — `"C"`, `"G"`, `"F#"` para
maior, `"Am"`, `"C#m"` para menor — e calcula as classes de nota permitidas para qualquer
tônica:

```c
int maior[7]  = {0, 2, 4, 5, 7, 9, 11};
int menorN[7] = {0, 2, 3, 5, 7, 8, 10};
for (int i = 0; i < 12; ++i) g_permitida[i] = false;
for (int i = 0; i < 7;  ++i) g_permitida[(pc + iv[i]) % 12] = true;
```

Mas `plugin/PluginProcessor.cpp:30` passa exatamente sete strings fixas:

```cpp
const juce::StringArray kEscalas {
    "Cromatica", "Do maior (C)", "La menor (Am)", "Sol maior (G)",
    "Mi menor (Em)", "Fa maior (F)", "Re menor (Dm)" };
```

**Consequência:** não é possível corrigir em Ré maior, Si bemol maior, Mi maior — nenhuma
tonalidade com mais de um sustenido ou bemol. São **6 de 24** tonalidades disponíveis.

É a maior melhoria por linha de código do projeto: o DSP não muda, só a montagem do combo.

> 🔴 **Esta lacuna não é hipotética — ela bloqueou o teste de usuário.** Para realizar a
> sessão foi preciso procurar um instrumental que estivesse numa das 6 tonalidades
> disponíveis, em vez de escolher o material livremente. Registrado como **Achado 3** em
> [teste-de-usuario.md §5-bis](teste-de-usuario.md), com a consequência metodológica de que o
> repertório do teste ficou enviesado pela limitação da ferramenta.
>
> Isso muda o peso do item: ele deixa de ser "paridade com o concorrente" e passa a ser
> **correção de uma falha de usabilidade observada em uso real**. É o argumento mais forte, e
> é o que deve ir para o texto do TCC.

---

## 4. `Forca` não é Retune Speed — os três eixos

Esta foi a confusão conceitual mais importante desfeita na análise, e ela **corrige a §8.2 da
documentação técnica**.

### 4.1 O que a `forca` faz

`src/core/dsp.h:165`:

```c
double corrMidi = midi + (forca * mov) / 100.0;
```

A `forca` é um **escalar estático multiplicando a correção**. Não tem estado, não tem memória,
não tem dimensão temporal. É uma interpolação linear entre a altura que o cantor produziu e a
nota da escala — na prática, um **dry/wet no domínio da afinação**.

### 4.2 O teste que separa os dois conceitos

Cante uma nota sustentada **50 cents abaixo** da nota-alvo:

| Controle | Onde a saída termina |
|---|---|
| `forca` = 0,5 | **25 cents abaixo — permanentemente** |
| Retune Speed = 50 ms | **0 cents abaixo, após ~50 ms** |

A `forca` reduz a dureza **não chegando na nota**. O Retune Speed reduz a dureza **demorando
para chegar**. A primeira deixa o cantor desafinado em regime permanente; a segunda o deixa
afinado, com o trajeto audível.

É por isso que o Auto-Tune **não tem** um controle equivalente à `forca`: a filosofia do produto
é ir sempre 100% até a nota e controlar apenas o *tempo*.

### 4.3 Os três eixos independentes

| Eixo | Pergunta que responde | Auto-Tune | Protótipo |
|---|---|---|---|
| **Profundidade** | quão *longe* a correção vai | — | `Forca` ✅ |
| **Limiar** | quão *grande* o erro precisa ser para corrigir | Flex-Tune | `Tolerancia` ✅ |
| **Tempo** | quão *rápido* a correção chega | Retune Speed | ❌ **ausente** |

### 4.4 O `Glide` tem a estrutura certa e o sinal errado

O `Glide` **é** um filtro de constante de tempo — um polo, exatamente o que o Retune Speed
precisa. O problema é *o que* ele filtra (`src/c1_streaming/autotune_stream.h:434`):

```cpp
glideEstado = tinhaNota ? (alpha*glideEstado + (1.0-alpha)*alvoCents) : alvoCents;
```

Ele suaviza `alvoCents` — a saída de `notaAlvo()`, isto é, o **destino**. Mas dentro de uma nota
sustentada o destino já está preso à nota (a correção o prende ali, com a zona morta de ±`tol`
em volta). O filtro recebe uma entrada praticamente constante, converge para ela em poucos
milissegundos e **deixa de ter efeito**. Ele só age nas transições de nota — e, por causa do
reset `tinhaNota`, só em legato.

**A infraestrutura existe. Está ligada no sinal errado.**

### 4.5 A formulação correta

A patente do Auto-Tune (Hildebrand, US 5.973.252 — ver
[pesquisa-bibliografica.md §2.5](pesquisa-bibliografica.md)) suaviza a **razão de correção**,
não o destino. Traduzido para as variáveis do protótipo:

```c
// HOJE — filtra o destino (converge e morre dentro da nota)
glideEstado = α*glideEstado + (1−α)*alvoCents;

// PROPOSTO — filtra a correção (age durante a nota inteira)
movFiltrado = α*movFiltrado + (1−α)*(forca * mov);
corrMidi    = midi + movFiltrado / 100.0;
```

No segundo caso a saída é sempre `altura real + correção lenta`: o vibrato do cantor atravessa
intacto (é rápido, o filtro não o alcança) e a desafinação média é corrigida (é lenta, o filtro
a segue). É a curva `G(f_v) = f_v/√(f_v² + f_c²)` documentada em
[documentacao-tecnica.md §9.2](documentacao-tecnica.md#92-cor--naturalidade) e refinada na
pesquisa bibliográfica.

---

## 5. O que falta, em três níveis

### Nível 1 — necessário para ser um *plugin*, não só um autotune

| Item | Por quê | Status |
|---|---|---|
| `KEY` × `SCALE` separados (24 tonalidades) | hoje só 6 das 24 são alcançáveis (§3). **Bloqueou o teste de usuário** — ver [Achado 3](teste-de-usuario.md) | ✅ **decidido** |
| **Mix seco/molhado** | `Forca` mistura *afinação*, não *sinal*. Não é a mesma coisa | ✅ **decidido** — substitui a `Forca` |
| **Detune** (afinação de referência) | A4 = 440 Hz está cravado; inviabiliza tocar com material fora de 440 | ⏳ não decidido |

### Nível 2 — esperado de um autotune, e já mapeado no backlog

| Item | Referência | Nota |
|---|---|---|
| **Retune Speed** | C1 | ✅ decidido. É a fundação da camada de expressão inteira |
| **Natural Vibrato** | K1 | 🟢 **sai de graça com C1** — uma multiplicação |
| **Humanize** | K2 | 🟢 **sai quase de graça com C1** — τ variável no tempo |
| **Create Vibrato** | K3 | parâmetros já especificados pelo manual |
| **Flex-Tune** (de verdade) | K5 | mecanismo novo, não renomeação |
| Tracking exposto | — | hoje fixo no HMM |
| Transpose | — | barato: o PSOLA já desloca altura |

### Nível 3 — fora do escopo, e a fronteira é defensável

Throat Length, Formant Correction, Hold, teclado MIDI e edição de nota por teclado.

A [documentacao-tecnica.md §8.2](documentacao-tecnica.md#82-a-cor-por-que-soa-duro-e-estático)
já argumenta que o protótipo é um **corretor**, não um **colorizador**. A pesquisa
bibliográfica confirmou que a própria Antares trata Throat e Formant como processos
**adicionais**, documentados separadamente da correção de altura.

> ✅ **Reforçado em 2026-08-26 por um resultado negativo com citação.**
> Santacruz et al. (2016, *Applied Sciences* 6(11):368) mostram que a transformação de
> envelope espectral só é perceptível em deslocamentos **da ordem de uma quinta** (~700 cents).
> Correções de afinação movem dezenas de cents. **Processamento de formante não pode resolver
> o problema de "cor" de um corretor** — a escala está errada por uma ordem de grandeza.
>
> Isso deixa de ser "escolha de escopo" e passa a ser **conclusão fundamentada**, o que é
> muito mais forte na banca. Ver
> [pesquisa-retune-speed-e-cor.md §3.1](pesquisa-retune-speed-e-cor.md).
>
> **Correção de rumo:** o *Humanize* sai do Nível 3. Ele não é timbre — é constante de tempo
> variável, e vem quase de graça com o Retune Speed (item K2).

---

## 6. Conclusão para o texto do TCC

A pergunta da banca não será *"por que não tem Throat Length?"*. Será:

> **"Isto é um corretor de afinação completo?"**

A resposta honesta hoje: **o motor sim, a interface ainda não.** Os três itens do Nível 1
somados fecham essa pergunta, e nenhum deles exige mudança no DSP — são montagem de parâmetro
e roteamento de sinal.

Vale registrar explicitamente no texto que o protótipo tem **dois controles que o Auto-Tune
Artist não expõe** (`Look-ahead` e `Forca`), sendo o `Look-ahead` diretamente ligado à
contribuição do trabalho (o eixo latência ↔ qualidade).

---

## 7. Referências internas

- [historico-e-decisoes.md](historico-e-decisoes.md) — as decisões tomadas a partir desta análise
- [modo-baixa-latencia.md](modo-baixa-latencia.md) — especificação do modo de baixa latência
- [pesquisa-bibliografica.md](pesquisa-bibliografica.md) — fontes primárias (patente, manuais, artigos)
- [documentacao-tecnica.md](documentacao-tecnica.md) — referência técnica completa
