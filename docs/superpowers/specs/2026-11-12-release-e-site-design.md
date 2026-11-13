# Release público, site e links no TCC: design

Data: 12/11/2026. Decidido com o autor (Gabriel Abreu Cherubini) numa sessão
de perguntas. Complementa `docs/plano-distribuicao.md` (27/09/2026). Onde os
dois discordam, vale este documento.

Os itens marcados **(escolha do Claude)** não foram decididos pelo autor. Um
builder pode revê-los sem consultar ninguém.

## Por quê

O plugin é o produto do TCC ("Desenvolvimento de um Protótipo de Software Livre
para Correção Automática de Afinação Vocal", Ciência da Computação, PUCRS,
orientador Dr. Marco Mangan). O texto do TCC já promete três coisas que ainda
não existem:

- uma página no GitHub Pages;
- binários gerados automaticamente a cada versão, só VST3 para Windows;
- a licença AGPLv3.

O autor quer que qualquer pessoa entre num site, entenda que é um TCC gratuito
e baixe o plugin sem compilar nada. Também quer que o TCC aponte para o código
e para o site.

## Metas

1. Site de uma página, bilíngue, publicado no GitHub Pages.
2. Release `v1.0.0` com um instalador Windows do VST3, gerado pelo GitHub
   Actions a partir de uma tag.
3. TCC com links para os repositórios e para o site, e o Overleaf em sincronia
   com o repositório `TCC-TEXT`.

## Fora do escopo

- macOS, Linux, AU e AAX (Pro Tools).
- Distribuir o Standalone. O `FORMATS VST3 Standalone` continua no CMake para
  testes locais, mas não vai no instalador nem no site.
- Domínio próprio. Fica em `gacherubini.github.io/TCC-autotune-cpp`.
- Certificado de assinatura de código.
- Trocar o nome do plugin.
- A pasta local `python-tcc`, que é um protótipo antigo. Não é repositório git
  e foi substituída por `TCC-autotune-python`.

## O que seria um fracasso, mesmo funcionando

- O PDF do TCC ficar público antes da defesa.
- Os projetos do Ableton salvos durante a avaliação com usuário pararem de
  achar o plugin. Ou seja, `PLUGIN_MANUFACTURER_CODE Pucr` e `PLUGIN_CODE Tcat`
  **não mudam**.
- Algum arquivo do Overleaf ser sobrescrito, em qualquer direção, sem o autor
  ver a diferença antes.
- Commit com a linha `Co-Authored-By: Claude`. Todos os commits, em todos os
  repositórios, saem só com o autor `gacherubini`.
- Push direto na `main` do `TCC-autotune-cpp`. Tudo entra por branch e Pull
  Request, e o autor faz o merge.

## Ordem de execução (decidida pelo autor)

1. Conferir o Overleaf (só leitura).
2. Mockups do site com 2 ou 3 direções visuais. O autor escolhe uma, e aí o
   site é construído e publicado com o botão "em breve".
3. Release `v1.0.0`.
4. Notas de rodapé e link do site no TCC, depois upload no Overleaf.

## 1. Conferência do Overleaf

- Não existe ligação git com o Overleaf (conta gratuita). Abrir o projeto no
  Chrome do autor, onde ele já está logado, pelas ferramentas
  `mcp__claude-in-chrome__*`, **só lendo**.
- Baixar o ZIP pelo menu Download → Source e comparar com o repositório
  `C:\Users\guilh\Documents\codigo\TCC` no commit `7dda747`. O
  `sync-overleaf.sh` pega o ZIP mais novo de `~/Downloads` e mostra a prévia do
  rsync. Usar **só a prévia**, sem confirmar a aplicação.
- Mostrar ao autor as diferenças arquivo por arquivo. Ele decide qual versão
  vale. Até a decisão, nada é aplicado em nenhum dos lados.

## 2. Site

### Onde e como

- Pasta `site/` na raiz do `TCC-autotune-cpp`. **Não** em `docs/`, que guarda
  a documentação técnica.
- HTML, CSS e JS estáticos, sem etapa de build **(escolha do Claude)**.
- Publicação pelo workflow oficial do Pages (`actions/upload-pages-artifact` +
  `actions/deploy-pages`), disparado por mudanças em `site/**` na `main`. O
  Pages precisa estar configurado no repositório para usar "GitHub Actions"
  como fonte. Quem faz isso é o autor (ou o `gh api` com a permissão dele).
- URL final: `https://gacherubini.github.io/TCC-autotune-cpp/`.

### Visual

Antes de construir, mostrar ao autor 2 ou 3 direções visuais da landing page,
como mockups no navegador. Ele escolhe uma, e a construção segue a escolhida.
Nenhuma identidade visual foi decidida ainda.

### Idiomas

- PT-BR e inglês, com um botão PT/EN no topo da página.
- A página **sempre abre em PT-BR** (decisão do autor).
- A escolha feita no botão fica salva em `localStorage`, com `try/catch`, e
  vale na próxima visita **(escolha do Claude)**. Sem storage, a página abre
  em PT-BR e funciona normalmente.
- Os dois textos ficam na mesma página, e o JS só troca qual aparece.
  Atualizar também `<html lang>`.

### Conteúdo (escolhido pelo autor)

1. **Topo:** o que o plugin faz, em duas linhas. Deixar claro que é gratuito e
   de código aberto, e mostrar o botão Baixar.
2. **Antes e depois:** players com `exemplo-antes.wav`, `exemplo-depois.wav` e
   `exemplo-depois-natural.wav`, que estão na raiz do repositório e têm cerca de
   430 KB cada. Converter para mp3 em `site/audio/` **(escolha do Claude)**. As
   legendas dizem o que muda entre "depois" e "depois-natural". Para escrevê-las,
   ver o README do plugin e `docs/pesquisa-retune-speed-e-cor.md`.
3. **Como funciona:** resumo técnico curto. O pYIN detecta o tom, o TD-PSOLA
   corrige, e existe o motor de baixa latência por ponteiro. Mencionar também o
   estudo comparativo em Python (autocorrelação, YIN, pYIN, SWIPE) no
   repositório `TCC-autotune-python`.
4. **Contexto do TCC:** trabalho de conclusão de Ciência da Computação da PUCRS,
   com autor, orientador e o status "TCC em andamento". **Sem PDF** até a defesa.
   Depois, o PDF entra num release futuro do site.
5. **Download:**
   - O botão aponta para
     `https://github.com/gacherubini/TCC-autotune-cpp/releases/latest/download/TCC-Autotune-Setup-Windows.exe`.
     O nome do arquivo é fixo **(escolha do Claude)** e o workflow do release
     tem que gerar exatamente esse nome.
   - Até existir o release, o botão aparece desabilitado com "em breve".
   - Embaixo do botão, uma linha "Windows · VST3 · testado no Ableton Live"
     **(escolha do Claude)**.
   - Mais duas linhas sobre o SmartScreen: o aviso "O Windows protegeu o
     computador" é esperado porque o instalador não tem assinatura. O caminho é
     "Mais informações" → "Executar assim mesmo". Depois, é só reescanear os
     plugins na DAW.
   - O autor **não** quis uma seção de instalação passo a passo.
6. **Rodapé:** links para `github.com/gacherubini/TCC-autotune-cpp` e
   `github.com/gacherubini/TCC-autotune-python`, e a licença AGPLv3.

## 3. Release `v1.0.0`

Tudo isto num Pull Request para a `main` do `TCC-autotune-cpp`.

- **Licença:** `LICENSE` com o texto da AGPLv3 na raiz. Atualizar o README com
  a licença e o link de download. Conferir se a licença do SDK VST3 que vem no
  JUCE 8.0.4 permite distribuir junto (ver `plano-distribuicao.md`, etapa 1).
- **Fabricante:** em `plugin/CMakeLists.txt`, `COMPANY_NAME "PUCRS"` passa a
  `"Gabriel Cherubini"`. `PRODUCT_NAME "TCC Autotune"`,
  `PLUGIN_MANUFACTURER_CODE Pucr` e `PLUGIN_CODE Tcat` não mudam.
- **Nome:** continua "TCC Autotune". O autor aceitou o risco de citar
  "Autotune", que é marca da Antares, e o repositório não é renomeado.
- **Versão:** `project(TccAutotune VERSION 1.0.0 ...)` já está em 1.0.0, e a
  tag é `v1.0.0`.
- **Workflow:** `.github/workflows/release.yml`.
  - Disparo por tag `v*`. Runner `windows-latest` com MSVC e CMake em Release,
    target do VST3.
  - Roda o pluginval no `.vst3`. Se o pluginval falhar, o release não sai. O
    nível de rigor fica a critério do builder; o `plugin/build.sh` já usa o
    pluginval e serve de referência.
  - Gera o instalador com Inno Setup (`iscc`) e cria o GitHub Release da tag
    com o instalador anexado.
- **Instalador:**
  - Inno Setup **(escolha do Claude)**. Copia a pasta
    `TCC Autotune.vst3` para `{commoncf64}\VST3`, o que pede admin.
  - Tem desinstalador e oferece PT-BR e inglês **(escolha do Claude)**.
  - Não instala o Standalone.
  - Nome do arquivo: `TCC-Autotune-Setup-Windows.exe`.
- **SDK VST3:** conferir a licença do SDK que vem no JUCE 8.0.4. Se ela não
  permitir distribuir junto com AGPLv3, ou se a resposta não for clara, **parar
  e perguntar ao autor** (decisão do autor). Não atualizar o JUCE por conta
  própria.
- **Runtime do MSVC:** linkar estaticamente, para quem instala não precisar do
  Visual C++ Redistributable **(escolha do Claude)**.
- **Teste antes do merge:** o `workflow_dispatch` só funciona depois que o
  workflow está na `main`. Por isso o workflow também roda em `pull_request` e
  sobe o instalador como artifact do run. O autor baixa esse artifact e testa
  no Ableton da máquina dele **(escolha do Claude)**. O checklist do teste tem
  dois itens:
  - instalar, reescanear os plugins e abrir o plugin num projeto novo;
  - reabrir um projeto do Ableton salvo durante a avaliação com usuário e
    confirmar que o plugin continua lá.
- **Caminho de instalação:** é o mesmo `%CommonProgramFiles%\VST3` que o
  `plugin/instalar_vst3.bat` já usa. Quem rodou o `.bat` antes não fica com o
  plugin duplicado.
- **Tag e publicação (decisão do autor):**
  - O Claude cria e envia a tag `v1.0.0` só depois do OK explícito do autor,
    com o PR já mergeado.
  - O workflow cria o release **em rascunho**, com o instalador anexado.
  - O autor testa o instalador do rascunho e clica em Publicar no GitHub. Só aí
    o `releases/latest/download/...` do site passa a funcionar.
- **Depois do release:** trocar o botão "em breve" do site pelo link real.

## 4. Links no TCC

Repositório `C:\Users\guilh\Documents\codigo\TCC` (remoto `TCC-TEXT`,
privado), arquivo `tcc1-sections.tex`.

- A seção `sec:repositorio` ("Disponibilização do código, exemplos e
  materiais", linha ~1485) já cita os dois repositórios (linhas ~1495 e ~1501).
  Isso fica como está.
- **Notas de rodapé:** colocar `\footnote{\url{...}}` na **primeira menção** de
  cada projeto no texto, que pode estar antes do capítulo de implementação:
  - o estudo comparativo em Python → `https://github.com/gacherubini/TCC-autotune-python`;
  - o protótipo em C++ → `https://github.com/gacherubini/TCC-autotune-cpp`.
- **Site:** depois que ele estiver no ar, acrescentar o link em
  `sec:repositorio`.
- **Cronograma:** o item "Licença e nome do plugin" (linha ~2857) e as Sprints
  14 e 15 podem passar a "realizado", mas só se o autor aprovar ao ver o diff
  **(escolha do Claude: propor no mesmo diff, não aplicar sozinho)**.
- **Fluxo:**
  1. Mostrar o diff ao autor.
  2. Com a aprovação, commitar na `main` do `TCC-TEXT`, sem `Co-Authored-By`.
  3. Fazer o upload do `tcc1-sections.tex` no Overleaf pelo Chrome.
  4. Confirmar que o Overleaf compila sem erro novo.

## Deixado para o builder

- Estrutura de arquivos do `site/` e o markup.
- Script do Inno Setup, rigor do pluginval e cache do JUCE no Actions.
- Bitrate do mp3 e ferramenta de conversão.
- Texto em inglês, traduzido do PT-BR com o mesmo conteúdo.
