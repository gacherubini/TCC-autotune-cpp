# Documentação — TCC Autotune

Índice da documentação do projeto. Comece por aqui.

| Documento | O que contém | Quando ler |
|---|---|---|
| **[documentacao-tecnica.md](documentacao-tecnica.md)** | Referência técnica completa dos dois repositórios: pipeline pYIN + TD-PSOLA estágio por estágio, motor de streaming, plugin, todos os resultados medidos, **diagnóstico dos problemas**, **soluções propostas**, backlog priorizado e glossário. | Para entender o sistema inteiro, ou para decidir o que implementar no TCC 2. |
| **[teste-de-usuario.md](teste-de-usuario.md)** | Registro do teste de validação com usuário: o que foi testado, o que funcionou, os dois requisitos reprovados (latência e naturalidade), limitações do teste. Sem soluções. | Para saber *o que* está errado, sem opinião sobre como consertar. |
| **[arquitetura-streaming.md](arquitetura-streaming.md)** | Como o algoritmo de lote virou causal: as três fronteiras, ring buffer, Viterbi de lag fixo, PSOLA online, orçamento de latência. Inclui o roteiro de estudo da fundamentação. | Para mexer em `src/c1_streaming/` ou no plugin. |
| **[historico-e-decisoes.md](historico-e-decisoes.md)** | Registro cronológico: bugs caçados (o "pipoco"), decisões de arquitetura, varreduras experimentais (look-ahead, `N_FRAME`, tessitura), **as decisões do redesenho de interface** e a **errata da revisão bibliográfica**. | Para escrever o texto do TCC, ou antes de refazer algo que já foi tentado. |
| **[comparacao-antares.md](comparacao-antares.md)** | Comparação controle a controle com o Auto-Tune Artist: o que o protótipo tem, o que falta e em que nível. Contém a análise dos **três eixos** (profundidade / limiar / tempo) e por que `Forca` não é Retune Speed. | Antes de mexer em qualquer parâmetro do plugin. |
| **[modo-baixa-latencia.md](modo-baixa-latencia.md)** | Especificação do modo de baixa latência: cada mudança, o ganho, o custo, os números por preset de voz e **seis questões em aberto**. ⚠️ Nada implementado. | Antes de escrever a primeira linha do modo. |
| **[pesquisa-bibliografica.md](pesquisa-bibliografica.md)** | Fundamentação das soluções: 5 artigos revisados por pares, a patente do Auto-Tune, manuais de fabricante. Cada afirmação com grau de evidência declarado. | Para citar qualquer coisa no texto do TCC. |
| **[pesquisa-retune-speed-e-cor.md](pesquisa-retune-speed-e-cor.md)** | O que o Retune Speed é (manual oficial, verbatim), por que `Tolerancia` **não** é Flex-Tune, e a conclusão de que **formante não resolve "cor"**. Define os itens K1–K6. | Antes de implementar o Retune Speed ou qualquer controle de expressão. |

## Mapa rápido

```
                  ┌─ documentacao-tecnica.md ──── o sistema inteiro + o que fazer agora
                  ├─ teste-de-usuario.md ──────── o que o usuário reprovou
   README.md ─────┼─ arquitetura-streaming.md ─── como o tempo real funciona
   (raiz)         ├─ historico-e-decisoes.md ──── o que já foi tentado, decidido e corrigido
                  ├─ comparacao-antares.md ────── o que falta para ser um plugin completo
                  ├─ modo-baixa-latencia.md ───── o que o modo vai fazer (não implementado)
                  ├─ pesquisa-bibliografica.md ── as fontes de tudo acima
                  └─ pesquisa-retune-speed-e-cor.md ─ Retune Speed e a origem da "cor"
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

O LaTeX do trabalho está em [`../tcc-texto/`](../tcc-texto/). A §10 do documento técnico
lista **correções pendentes no texto do TCC 1** identificadas durante a revisão.

**Backlog priorizado e plano por sprint:** [documentacao-tecnica.md §10](documentacao-tecnica.md#10-backlog-priorizado-e-plano-por-sprint)
— com a repriorização registrada na [errata](historico-e-decisoes.md#errata--afirmações-corrigidas-pela-pesquisa-bibliográfica-2026-08-26) (item 4: L6 sobe para o topo).

## Estado atual do TCC 2

| Frente | Status |
|---|---|
| Diagnóstico dos dois sintomas | ✅ concluído |
| Revisão bibliográfica | ✅ concluída |
| Comparação com o estado da arte | ✅ concluída |
| Redesenho de interface | 📋 decidido, não implementado |
| Pesquisa sobre Retune Speed e "cor" | ✅ concluída — 1 decisão cancelada, 6 itens novos (K1–K6) |
| Modo de baixa latência | ⏸️ especificado, implementação suspensa |
| Correções de código (§8.3) | ⏳ não medidas |
