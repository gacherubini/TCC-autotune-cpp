# Documentação — TCC Autotune

Índice da documentação do projeto. Comece por aqui.

| Documento | O que contém | Quando ler |
|---|---|---|
| **[documentacao-tecnica.md](documentacao-tecnica.md)** | Referência técnica completa dos dois repositórios: pipeline pYIN + TD-PSOLA estágio por estágio, motor de streaming, plugin, todos os resultados medidos, **diagnóstico dos problemas**, **soluções propostas**, backlog priorizado e glossário. | Para entender o sistema inteiro, ou para decidir o que implementar no TCC 2. |
| **[teste-de-usuario.md](teste-de-usuario.md)** | Registro do teste de validação com usuário: o que foi testado, o que funcionou, os dois requisitos reprovados (latência e naturalidade), limitações do teste. Sem soluções. | Para saber *o que* está errado, sem opinião sobre como consertar. |
| **[arquitetura-streaming.md](arquitetura-streaming.md)** | Como o algoritmo de lote virou causal: as três fronteiras, ring buffer, Viterbi de lag fixo, PSOLA online, orçamento de latência. Inclui o roteiro de estudo da fundamentação. | Para mexer em `src/c1_streaming/` ou no plugin. |
| **[historico-e-decisoes.md](historico-e-decisoes.md)** | Registro cronológico: bugs caçados (o "pipoco"), decisões de arquitetura, varreduras experimentais (look-ahead, `N_FRAME`, tessitura), **as decisões do redesenho de interface** e a **errata da revisão bibliográfica**. | Para escrever o texto do TCC, ou antes de refazer algo que já foi tentado. |
| **[comparacao-antares.md](comparacao-antares.md)** | Comparação controle a controle com o Auto-Tune Artist: o que o protótipo tem, o que falta e em que nível. Contém a análise dos **três eixos** (profundidade / limiar / tempo) e por que `Forca` não é Retune Speed. | Antes de mexer em qualquer parâmetro do plugin. |
| **[execucao-do-plano.md](execucao-do-plano.md)** | 📓 **Diário das etapas** — o que já foi feito, a verificação de cada uma e os achados que apareceram no caminho. Comece por aqui para saber o estado real do código. | Antes de continuar a implementação. |
| **[diagnostico-block512.md](diagnostico-block512.md)** | 🔬 Diagnóstico completo da invariância ao tamanho de bloco: causa raiz, medições e as três correções avaliadas (uma aplicada, duas registradas). | Antes de mexer em `avancarPsola()` ou nas marcas do PSOLA. |
| **[plano-de-implementacao.md](plano-de-implementacao.md)** | 📐 **O plano de execução**: a cadeia de correção proposta, a prova de que a fusão é retrocompatível, as 6 etapas verificáveis, mudanças por arquivo e a estratégia de teste. | **Antes de escrever qualquer código.** |
| **[modo-baixa-latencia.md](modo-baixa-latencia.md)** | Especificação do modo de baixa latência por parâmetros (v1) e por troca do detector (v2): cada mudança, o ganho, o custo, os números por preset de voz e seis questões que na época bloqueavam a implementação. ✅ **O modo foi entregue** (botão Low Latency, Etapa 6), mas pelo mecanismo da [v3](especificacao-v3-ponteiro.md); o que ficou para trás foi o **v1 por parâmetros**. A **§7 daqui** é a interface que o botão seguiu (opção B). | Para entender por que a v3 troca o motor de síntese em vez de seguir por aqui. |
| **[pesquisa-bibliografica.md](pesquisa-bibliografica.md)** | Fundamentação das soluções: 5 artigos revisados por pares, a patente do Auto-Tune, manuais de fabricante. Cada afirmação com grau de evidência declarado. | Para citar qualquer coisa no texto do TCC. |
| **[pesquisa-latencia-antares.md](pesquisa-latencia-antares.md)** | 🔬 **Como o Auto-Tune declara 0,84 ms**: a dedução de que são **37 amostras fixas**, a arquitetura de ponteiro móvel da patente, a distinção entre *atraso de detecção* e *atraso do áudio*, e por que o plano v1/v2 tem um teto que só a troca do motor de síntese atravessa. | Antes de decidir qualquer coisa sobre latência. |
| **[pesquisa-retune-speed-e-cor.md](pesquisa-retune-speed-e-cor.md)** | O que o Retune Speed é (manual oficial, verbatim), por que `Tolerancia` **não** é Flex-Tune, e a conclusão de que **formante não resolve "cor"**. Define os itens K1–K6. | Antes de implementar o Retune Speed ou qualquer controle de expressão. |
| **[especificacao-v3-ponteiro.md](especificacao-v3-ponteiro.md)** | Especificação do motor v3 — ponteiro móvel sobre um anel, no lugar do TD-PSOLA: interface, fiação no streaming, controles (`motor=`/`lowlat=`/botão **Low Latency**), verificação e medição. | Mexer no motor v3 / Low Latency, ou entender a Etapa 6. |
| **[plano-v3-ponteiro.md](plano-v3-ponteiro.md)** | 📐 Plano de execução da Etapa 6, por tarefa — o que a [execucao-do-plano.md](execucao-do-plano.md) registra como feito. | Só para reconstruir a ordem em que a Etapa 6 foi implementada. |
| **[analise-v1-v2-v3.md](analise-v1-v2-v3.md)** | ⚠️ Compara os três caminhos de baixa latência **por estágio do pipeline**, mostra onde cada um corta e qual é o piso de cada um. **Corrige dois números de latência errados** em [modo-baixa-latencia.md](modo-baixa-latencia.md) e [pesquisa-latencia-antares.md](pesquisa-latencia-antares.md), e marca a §8 de [modo-baixa-latencia.md](modo-baixa-latencia.md) (as "questões em aberto") como desatualizada. | **Antes de citar qualquer número de latência**, e antes de discutir v1/v2/v3. |
| **[spec-encaixe-e-estabilidade.md](spec-encaixe-e-estabilidade.md)** | 🐛 Spec das três causas medidas em 03/09/2026: a detecção reporta **uma oitava abaixo** acima do `fmax` (100 % dos quadros), a nota-alvo **pisca** (mediana de 41 ms), e o TD-PSOLA **estoura o orçamento de tempo real** em 19,6 % dos blocos. Com as decisões de implementação, a seam de teste e a ordem. | Antes de mexer na detecção de altura, na escolha de nota-alvo ou na janela do PSOLA. |

## Mapa rápido

```
                  ┌─ documentacao-tecnica.md ──── o sistema inteiro + o que fazer agora
                  ├─ teste-de-usuario.md ──────── o que o usuário reprovou
   README.md ─────┼─ arquitetura-streaming.md ─── como o tempo real funciona
   (raiz)         ├─ historico-e-decisoes.md ──── o que já foi tentado, decidido e corrigido
                  ├─ comparacao-antares.md ────── o que falta para ser um plugin completo
                  ├─ plano-de-implementacao.md ── o que vai ser feito e em que ordem
                  ├─ execucao-do-plano.md ─────── o que JA foi feito, etapa por etapa
                  ├─ modo-baixa-latencia.md ───── v1/v2 por parâmetros (o modo saiu pela v3)
                  ├─ especificacao-v3-ponteiro.md ─ o motor v3, implementado na Etapa 6
                  ├─ plano-v3-ponteiro.md ──────── o plano da Etapa 6, por tarefa
                  ├─ analise-v1-v2-v3.md ──────── os numeros de latencia, por estagio
                  ├─ spec-encaixe-e-estabilidade.md ─ os tres defeitos medidos e como corrigir
                  ├─ pesquisa-bibliografica.md ── as fontes de tudo acima
                  ├─ pesquisa-retune-speed-e-cor.md ─ Retune Speed e a origem da "cor"
                  └─ pesquisa-latencia-antares.md ─ por que o Auto-Tune tem 37 amostras
```

## ⚠️ Leia antes de citar a documentação técnica

A [documentacao-tecnica.md](documentacao-tecnica.md) foi escrita **antes** da revisão
bibliográfica. Oito afirmações dela foram corrigidas — incluindo a origem do C1, o mecanismo
do L6 e a meta de latência do RNF01. **O texto original foi preservado de propósito**, como
registro do que se acreditava antes.

👉 **A lista completa das correções está na
[errata em historico-e-decisoes.md](historico-e-decisoes.md#errata--afirmações-corrigidas-pela-pesquisa-bibliográfica-2026-08-26).**

## Onde estão os problemas em aberto

Os dois requisitos reprovados no teste de usuário, e onde ler sobre cada um:

| Problema | Diagnóstico | Soluções |
|---|---|---|
| **Latência de 57,9 ms** (meta: ≤ 20 ms) | [documentacao-tecnica.md §8.1](documentacao-tecnica.md#81-o-delay-de-onde-vêm-os-579-ms) | [§9.1](documentacao-tecnica.md#91-latência) |
| **Som duro / "sem cor"** | [documentacao-tecnica.md §8.2](documentacao-tecnica.md#82-a-cor-por-que-soa-duro-e-estático) | [§9.2](documentacao-tecnica.md#92-cor--naturalidade) |
| **3 achados de código** (CPU quadrática, alocação no callback, normalização por janela) | [documentacao-tecnica.md §8.3](documentacao-tecnica.md#83-três-achados-de-código) | [§9.3](documentacao-tecnica.md#93-correções-de-engenharia) |

## Texto do TCC

O LaTeX do trabalho está em **repositório separado**:
[`gacherubini/TCC-TEXT`](https://github.com/gacherubini/TCC-TEXT). A §10 do documento
técnico lista **correções pendentes no texto do TCC 1** identificadas durante a revisão.

**Backlog priorizado e plano por sprint:** [documentacao-tecnica.md §10](documentacao-tecnica.md#10-backlog-priorizado-e-plano-por-sprint)
— com a repriorização registrada na [errata](historico-e-decisoes.md#errata--afirmações-corrigidas-pela-pesquisa-bibliográfica-2026-08-26) (item 4: L6 sobe para o topo).

## Estado atual do TCC 2

| Frente | Status |
|---|---|
| Diagnóstico dos dois sintomas | ✅ concluído |
| Revisão bibliográfica | ✅ concluída |
| Comparação com o estado da arte | ✅ concluída |
| Redesenho de interface | ✅ **implementado** (2026-08-31) — painel afinador + 9 controles em três grupos; ver [`../plugin/README.md`](../plugin/README.md#a-tela-gui-custom-31082026) |
| Pesquisa sobre Retune Speed e "cor" | ✅ concluída — 1 decisão cancelada, 6 itens novos (K1–K6) |
| Plano de implementação | ✅ escrito — 6 etapas |
| **Etapa 0** — malha de correção unificada | ✅ **concluída** (2026-08-26), 17/17 casos idênticos |
| Etapas 1–5 | ✅ **concluídas** (2026-08-26) |
| **Etapa 6** — motor v3 / Low Latency | ✅ **concluída** (2026-09-02) |
| Modo de baixa latência | ✅ **resolvido pela v3 (Etapa 6)** — o caminho v1/v2 por parâmetros fica superado |
| Correções de código (§8.3) | ⏳ não medidas |
| **Reavaliação de escuta com o usuário** | ⛔ **não feita** — é o que fecha as duas reprovações |
