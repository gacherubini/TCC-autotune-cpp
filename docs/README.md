# Documentação — TCC Autotune

Índice da documentação do projeto. Comece por aqui.

| Documento | O que contém | Quando ler |
|---|---|---|
| **[documentacao-tecnica.md](documentacao-tecnica.md)** | Referência técnica completa dos dois repositórios: pipeline pYIN + TD-PSOLA estágio por estágio, motor de streaming, plugin, todos os resultados medidos, **diagnóstico dos problemas**, **soluções propostas**, backlog priorizado e glossário. | Para entender o sistema inteiro, ou para decidir o que implementar no TCC 2. |
| **[teste-de-usuario.md](teste-de-usuario.md)** | Registro do teste de validação com usuário: o que foi testado, o que funcionou, os dois requisitos reprovados (latência e naturalidade), limitações do teste. Sem soluções. | Para saber *o que* está errado, sem opinião sobre como consertar. |
| **[arquitetura-streaming.md](arquitetura-streaming.md)** | Como o algoritmo de lote virou causal: as três fronteiras, ring buffer, Viterbi de lag fixo, PSOLA online, orçamento de latência. Inclui o roteiro de estudo da fundamentação. | Para mexer em `src/c1_streaming/` ou no plugin. |
| **[historico-e-decisoes.md](historico-e-decisoes.md)** | Registro cronológico: bugs caçados (o "pipoco"), decisões de arquitetura e as varreduras experimentais (look-ahead, `N_FRAME`, tessitura). | Para escrever o texto do TCC, ou antes de refazer algo que já foi tentado. |

## Mapa rápido

```
                  ┌─ documentacao-tecnica.md ──── o sistema inteiro + o que fazer agora
   README.md ─────┼─ teste-de-usuario.md ──────── o que o usuário reprovou
   (raiz)         ├─ arquitetura-streaming.md ─── como o tempo real funciona
                  └─ historico-e-decisoes.md ──── o que já foi tentado e por quê
```

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
