# Texto do TCC (LaTeX)

Fonte do trabalho *"Desenvolvimento de um Protótipo Gratuito de Correção Automática de
Afinação Vocal"* — Gabriel Abreu Cherubini, orientação Dr. Marco Mangan, PUCRS 2026.

Usa a classe **EP-TCC** da Escola Politécnica da PUCRS (adaptada da PP-LaTeX de Ricardo
Piccoli). A documentação da classe está em [`README-classe-ep-tcc.md`](README-classe-ep-tcc.md).

## Estrutura

| Arquivo | Conteúdo |
|---|---|
| `tcc1.tex` | documento principal: metadados, resumo, inclusão dos capítulos |
| `tcc1-intro.tex` | Cap. 1 — Introdução |
| `tcc1-sections.tex` | Caps. 2–10 — contexto, problema, proposta, conceitos, algoritmos, estudo comparativo, resultados, implementação, conclusão, cronograma |
| `apendice-formulas.tex` | apêndice de fórmulas |
| `tcc.bib` | bibliografia |
| `fig/` | figuras |
| `tcc.cls`, `tcc-*.bst`, `tcc.layout` | classe e estilos bibliográficos (não editar) |
| `Makefile`, `sort.sh` | compilação |

## Compilar

```bash
make
```

Requer TeXLive (`texlive-full` no Ubuntu). Veja `README-classe-ep-tcc.md` para a lista de
pacotes mínima.

## Relação com o restante do repositório

O Capítulo *Implementação* descreve o código deste repositório. A documentação técnica em
[`../docs/`](../docs/) é o material de apoio para a redação — em particular:

- [`../docs/documentacao-tecnica.md`](../docs/documentacao-tecnica.md) §7 traz todos os
  números medidos, e §10 lista **correções pendentes no texto do TCC 1** identificadas na
  revisão técnica.
- [`../docs/teste-de-usuario.md`](../docs/teste-de-usuario.md) é a evidência da Sprint 10,
  a ser incorporada ao capítulo de resultados do TCC 2.

> **Fonte da verdade:** a partir deste commit, o texto é versionado aqui. Se você mantinha
> uma cópia no Overleaf, escolha um dos dois como fonte única para evitar divergência.
