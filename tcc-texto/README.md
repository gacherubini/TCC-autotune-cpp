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

## Sincronizar com o Overleaf

A conta gratuita do Overleaf não tem integração com Git nem com o GitHub, então o sync é via
ZIP, com o apoio de [`sync-overleaf.sh`](sync-overleaf.sh).

### Overleaf → repo (automatizado)

1. No Overleaf: **Menu → Download → Source** (baixa o `.zip` do projeto).
2. Aqui:

```bash
./tcc-texto/sync-overleaf.sh
```

O script acha o ZIP mais recente em `~/Downloads`, lista exatamente o que vai mudar
(`NOVO` / `ALTERA` / `APAGA`), pede confirmação e só então aplica. **Nada é commitado
automaticamente** — você revisa o diff e commita. Passe um caminho explícito se o ZIP estiver
em outro lugar:

```bash
./tcc-texto/sync-overleaf.sh ~/Desktop/projeto.zip
```

### Repo → Overleaf (manual)

Não dá para automatizar sem a integração paga: o "upload de ZIP" do Overleaf cria um projeto
**novo**, não mescla no existente. Para saber o que precisa subir:

```bash
./tcc-texto/sync-overleaf.sh --alterados
```

Ele lista os arquivos alterados desde o último sync. Suba cada um no Overleaf por
**Upload** — um arquivo de mesmo nome substitui o antigo.

### Arquivos protegidos

Estes existem só neste repositório e o sync nunca os toca nem os apaga:

| Arquivo | Por quê |
|---|---|
| `README.md` | este arquivo, escrito para o repo — o ZIP traz um `README.md` diferente (o da classe EP-TCC), que é renomeado para `README-classe-ep-tcc.md` na entrada |
| `sync-overleaf.sh` | o próprio script |

> **Fonte da verdade.** Com esse fluxo, o Overleaf continua sendo onde você escreve e o repo
> guarda o histórico. Sempre que editar dos dois lados entre um sync e outro, o
> `sync-overleaf.sh` vai mostrar o conflito como `ALTERA` antes de aplicar — **leia a lista
> antes de confirmar**, porque aplicar sobrescreve o lado local.
