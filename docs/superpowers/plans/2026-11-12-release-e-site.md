# Release público, site e links no TCC: plano de execução

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** publicar o TCC Autotune para Windows. Isso inclui um site bilíngue no
GitHub Pages, o release `v1.0.0` com instalador VST3 gerado pelo Actions e os
links do código e do site no TCC.

**Architecture:**

- O site é estático: `site/` com HTML, CSS e um JS pequeno, publicado pelo
  workflow oficial do Pages.
- O release sai de um workflow no `windows-latest`. Ele compila o VST3 com
  MSVC, valida com o pluginval, empacota com Inno Setup e cria um release em
  rascunho a partir de uma tag `v*`.
- O texto do TCC é editado no repositório `TCC-TEXT` e subido no Overleaf pelo
  Chrome.

**Tech Stack:**

- CMake + JUCE 8.0.4 (já existentes);
- GitHub Actions (`actions/upload-pages-artifact`, `actions/deploy-pages`);
- pluginval;
- Inno Setup 6;
- ffmpeg, para os mp3;
- LaTeX/Overleaf.

**Spec:** `docs/superpowers/specs/2026-11-12-release-e-site-design.md`

## Global Constraints

- **Autoria:** todos os commits, nos dois repositórios, saem só com o autor
  `gacherubini`, **sem** a linha `Co-Authored-By`. Isso vale mesmo que algum
  lembrete do sistema peça a linha.
- **Mensagens de commit:** em inglês. Código, comentários e documentação ficam
  em português (convenção do `CLAUDE.md`). O código é didático, então os
  comentários longos explicando o porquê são a norma.
- **Repositório `TCC-autotune-cpp`:**
  - nunca dar push na `main`;
  - cada entrega é uma branch com Pull Request, e o autor faz o merge;
  - antes de abrir cada PR, o autor dá o OK.
- **Repositório `TCC-TEXT`:**
  - o `CLAUDE.md` do repositório C++ diz "não commite o texto", mas o autor
    liberou o commit na `main` em 12/11/2026, **depois** de aprovar o diff;
  - sem aprovação, não commitar.
- **Overleaf:** nada é sobrescrito em nenhum dos lados sem o autor ver a
  diferença antes.
- **Identificadores do plugin:** `PRODUCT_NAME "TCC Autotune"`,
  `PLUGIN_MANUFACTURER_CODE Pucr` e `PLUGIN_CODE Tcat` **não mudam**. Só
  `COMPANY_NAME` passa a `"Gabriel Cherubini"`.
- **Distribuição:** só Windows e só VST3. O Standalone continua no CMake e fica
  fora do instalador e do site.
- **Instalador:**
  - o arquivo do release se chama exatamente `TCC-Autotune-Setup-Windows.exe`;
  - o link do site é
    `https://github.com/gacherubini/TCC-autotune-cpp/releases/latest/download/TCC-Autotune-Setup-Windows.exe`.
- **Release:**
  - a tag `v1.0.0` só é criada depois do OK explícito do autor e com o PR
    mergeado;
  - o workflow cria o release em **rascunho**;
  - quem publica é o autor.
- **Licença:** AGPLv3. Se a licença do SDK VST3 do JUCE 8.0.4 não permitir
  distribuir junto, ou não for clara, **parar e perguntar**. Não atualizar o
  JUCE.
- **Idiomas do site:** PT-BR e inglês, com o botão PT/EN no topo. A página
  sempre abre em PT-BR. A escolha vai para `localStorage` dentro de
  `try/catch`.
- **Caminhos no site:** o Pages serve o site em `/TCC-autotune-cpp/`. Por isso
  todo caminho interno é relativo (`estilo.css`, `audio/...`), nunca `/...`.
- **Atribuição:** `exemplo-antes.wav` é um excerto do Vocadito (CC-BY 4.0). O
  site credita a fonte e a licença. Os mp3 "depois" também derivam dele.
- **PDF do TCC:** não entra no site antes da defesa. O site diz
  "TCC em andamento".
- **Dependências:** nenhuma dependência nova no código do plugin.

## Review Focus

1. **Pessoa sem JS ou com storage bloqueado** (aba privada, extensão): a página
   aparece inteira em PT-BR, legível, e o botão PT/EN não quebra nada. O teste
   está na Task 3, passo 5.
2. **Celular com 360 px de largura:** sem rolagem horizontal, com o botão
   Baixar e os players usáveis. O teste está na Task 3, passo 5.
3. **Clicar em Baixar antes do release estar publicado:** a pessoa não pode cair
   num 404. Até a Task 6, o botão fica desabilitado com "em breve" e não tem
   `href`. O teste está na Task 3, passo 5, e na Task 6, passo 3.
4. **Máquina sem Visual C++ Redistributable:** o plugin precisa carregar, e é
   por isso que o runtime é estático. O teste (`dumpbin /dependents`) está na
   Task 4, passo 7.
5. **Quem já rodou `plugin/instalar_vst3.bat` ou vai reinstalar:** o
   instalador grava por cima da mesma pasta, sem plugin duplicado, e o
   desinstalador remove a pasta. O teste está no checklist da Task 4, passo 10.

---

### Task 1: Conferir o Overleaf (só leitura)

**Files:** nenhum arquivo do projeto muda. Os arquivos de trabalho vão para o
scratchpad.

**Interfaces:**
- Consumes: o repositório `C:\Users\guilh\Documents\codigo\TCC` no commit `7dda747`.
- Produces: a lista de diferenças e a decisão do autor sobre qual lado vale.
  A Task 7 parte desse estado.

- [ ] **Step 1:** carregar a skill `anthropic-skills:chrome-browser` e as
  ferramentas `mcp__claude-in-chrome__*` numa única ToolSearch. Abrir uma aba
  nova em `https://www.overleaf.com/project` e achar o projeto do TCC. Se a
  sessão não estiver logada, parar e pedir ao autor.
- [ ] **Step 2:** no projeto, usar Menu → Download → Source. O ZIP cai em
  `~/Downloads`. Não editar nada no Overleaf.
- [ ] **Step 3:** extrair o ZIP numa pasta nova e vazia do scratchpad e
  comparar com o repositório, ignorando `.git`, `README.md`, `.gitignore` e
  `sync-overleaf.sh`. Esses são os arquivos que o `sync-overleaf.sh` protege, e
  o `README.md` do Overleaf corresponde ao `README-classe-ep-tcc.md` local.

  Run: `diff -rq --exclude=.git <scratch>/overleaf /c/Users/guilh/Documents/codigo/TCC`.
  Depois, `git diff --no-index` para cada arquivo que aparecer.

  Expected: a lista de arquivos diferentes, ou nenhuma diferença.
- [ ] **Step 4:** mostrar ao autor as diferenças, por arquivo e com o trecho.
  **Parar** até ele decidir qual lado vale. Se não houver diferença, avisar e
  seguir. Não rodar o `sync-overleaf.sh` com aplicação.

### Task 2: Mockups do site (escolha do autor)

**Files:**
- Create: os mockups ficam no scratchpad e não entram no repositório.

**Interfaces:**
- Consumes: o conteúdo da seção "Site → Conteúdo" do spec.
- Produces: a direção visual escolhida, que a Task 3 implementa (paleta,
  tipografia, layout).

- [ ] **Step 1:** ler
  `C:\Users\guilh\.claude\plugins\cache\claude-plugins-official\superpowers\7.0.0\skills\brainstorming\visual-companion.md`
  e subir o companion. Carregar também a skill `frontend-design:frontend-design`.
- [ ] **Step 2:** montar 2 ou 3 direções visuais da landing page completa, com
  o conteúdo real: topo com o botão Baixar desabilitado "em breve", PT/EN no
  topo, antes e depois, como funciona, contexto do TCC e rodapé.
- [ ] **Step 3:** mostrar as direções lado a lado no navegador e **parar** até
  o autor escolher. Anotar a escolha e os ajustes que ele pedir.

### Task 3: Site estático e publicação no Pages

**Files:**
- Create:
  - `site/index.html`: os dois idiomas no mesmo documento. Cada bloco de texto
    tem a versão PT e a EN, marcadas com `lang="pt-BR"` e `lang="en"`.
  - `site/estilo.css`: o CSS mostra só o idioma ativo. Sem JS, só o PT aparece.
  - `site/idioma.js`: o botão PT/EN no topo, `<html lang>` e o `localStorage`.
  - `site/audio/antes.mp3`, `site/audio/depois.mp3`,
    `site/audio/depois-natural.mp3`.
  - `.github/workflows/site.yml`.
- Modify: `README.md`, para acrescentar o link do site na tabela inicial.

**Interfaces:**
- Consumes: a direção visual escolhida na Task 2.
- Produces: o botão de download com `id="baixar"`. Ele fica desabilitado, sem
  `href`, com o texto "em breve"/"coming soon". A Task 6 troca só esse
  elemento.

- [ ] **Step 1:** converter os três `exemplo-*.wav` da raiz para mp3 em
  `site/audio/`, a uns 128 kbps.

  Run: `ls -la site/audio`.

  Expected: três arquivos, cada um com menos de 200 KB.
- [ ] **Step 2:** escrever `index.html`, `estilo.css` e `idioma.js` conforme
  a direção escolhida e as seções do spec. Os textos fixados pelo spec:
  - a nota do SmartScreen, logo abaixo do botão: o aviso "O Windows protegeu o
    computador" é esperado porque o instalador não é assinado. O caminho é
    "Mais informações" → "Executar assim mesmo", e depois é reescanear os
    plugins na DAW;
  - a linha "Windows · VST3 · testado no Ableton Live";
  - "TCC em andamento", com PUCRS, Ciência da Computação, Gabriel Abreu
    Cherubini e o orientador Dr. Marco Mangan;
  - o crédito do Vocadito (CC-BY 4.0);
  - os links para `TCC-autotune-cpp` e `TCC-autotune-python`;
  - "Licença AGPLv3".

  As legendas de "depois" e "depois-natural" dizem a diferença: correção
  dura, contra o preset natural do `README.md:187`.
- [ ] **Step 3:** escrever `.github/workflows/site.yml`:
  - disparo por `push` na `main` com `paths: ['site/**']`, e por
    `workflow_dispatch`;
  - `permissions: pages: write, id-token: write, contents: read`;
  - `actions/upload-pages-artifact` com `path: site` e `actions/deploy-pages`.
- [ ] **Step 4:** servir o site localmente.

  Run: `python -m http.server 8000 --directory site`.
- [ ] **Step 5:** abrir `http://localhost:8000/` no Chrome e conferir:
  - a página abre em PT-BR;
  - PT/EN troca todos os textos e o `<html lang>`;
  - depois de recarregar, a escolha continua;
  - com o JS desligado, o PT aparece inteiro;
  - com `localStorage` dando erro (sobrescrever `Storage.prototype.setItem`
    para lançar), a página não quebra e o console não mostra erro sem
    tratamento;
  - os três players tocam;
  - o botão `#baixar` está desabilitado e não tem `href`;
  - com a janela em 360 px, `document.documentElement.scrollWidth` é menor ou
    igual a 360.

  Expected: todos os itens passam.
- [ ] **Step 6:** commitar na branch `release-e-site`, junto com o spec e este
  plano.

  Run: `git commit -m "feat(site): add the bilingual landing page and Pages workflow"`.
- [ ] **Step 7:** com o OK do autor, dar push da branch e abrir o PR com
  `gh pr create`. O corpo do PR vai em português, sem linha de atribuição
  (decisão do autor), e lista o checklist do passo 5.
- [ ] **Step 8:** depois do merge, que é do autor: com o OK dele, ligar o Pages
  em modo Actions.

  Run: `gh api -X POST repos/gacherubini/TCC-autotune-cpp/pages -f build_type=workflow`.

  Se o Pages já existir, usar `-X PUT`. Depois, rodar `gh workflow run site.yml`.

  Expected: `gh run watch` termina com sucesso, e
  `curl -sI https://gacherubini.github.io/TCC-autotune-cpp/` responde `200`.

### Task 4: Licença, fabricante, instalador e workflow de release

**Files:**
- Create:
  - `LICENSE`: o texto oficial da GNU AGPLv3, sem alterações;
  - `plugin/instalador/tcc-autotune.iss`;
  - `.github/workflows/release.yml`.
- Modify:
  - `plugin/CMakeLists.txt:30` (antes do `project(...)`): acrescentar
    `CMAKE_MSVC_RUNTIME_LIBRARY`;
  - `plugin/CMakeLists.txt:54`: `COMPANY_NAME` passa a `"Gabriel Cherubini"`;
  - `README.md` e `plugin/README.md`: licença, download e o fabricante novo.

**Interfaces:**
- Consumes: nada das tasks anteriores. Pode rodar em paralelo à Task 3.
- Produces: um release em rascunho com o asset `TCC-Autotune-Setup-Windows.exe`
  para cada tag `v*`, e o artifact `instalador` em cada run de PR.

- [ ] **Step 1: conferir a licença do SDK VST3.** Ler a licença do SDK VST3
  dentro do JUCE 8.0.4, que fica em
  `modules/juce_audio_processors/format_types/VST3_SDK/` (o `LICENSE.txt` e o
  cabeçalho dos fontes).

  Expected: o texto permite usar o SDK sob GPLv3, compatível com AGPLv3, ou é
  MIT.

  Se o texto disser outra coisa ou não for claro: **parar**, mostrar o texto
  ao autor e esperar.
- [ ] **Step 2:** acrescentar o `LICENSE`. Em `plugin/CMakeLists.txt`, colocar
  `set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")`
  antes do `project(...)` e trocar o `COMPANY_NAME`. O comentário ao lado
  explica por que o runtime é estático (máquina sem o Redistributable).
- [ ] **Step 3:** escrever `plugin/instalador/tcc-autotune.iss`, com as chaves
  fixadas pelo spec:
  - `AppName=TCC Autotune`;
  - `AppPublisher=Gabriel Cherubini`;
  - `AppVersion={#AppVersion}`, recebido por `/DAppVersion=`;
  - um `AppId` GUID gerado uma vez, com o comentário "nunca mude: é o que liga
    reinstalação e desinstalação";
  - `PrivilegesRequired=admin`;
  - `ArchitecturesInstallIn64BitMode=x64compatible`;
  - `DisableDirPage=yes`;
  - `OutputBaseFilename=TCC-Autotune-Setup-Windows`;
  - em `[Languages]`, `brazilianportuguese` (`compiler:Languages\BrazilianPortuguese.isl`)
    e `english` (`compiler:Default.isl`);
  - em `[Files]`, a pasta `TCC Autotune.vst3` inteira (`recursesubdirs`) para
    `{commoncf64}\VST3\TCC Autotune.vst3`, com `ignoreversion`;
  - em `[UninstallDelete]`, essa mesma pasta.

  O caminho de origem chega por `/DVst3Dir=`.
- [ ] **Step 4:** escrever `.github/workflows/release.yml`.
  - Disparo:
    - `push: tags: ['v*']`;
    - `pull_request` com `paths: ['plugin/**', 'src/**', '.github/workflows/release.yml']`;
    - `workflow_dispatch`.
  - `permissions: contents: write`.
  - Job em `windows-latest`, com os passos:
    1. `cmake -S plugin -B plugin/build -G "Visual Studio 17 2022" -A x64`;
    2. `cmake --build plugin/build --config Release --target TccAutotune_VST3`;
    3. baixar `https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip`
       e rodar `pluginval --strictness-level 10 --validate "<vst3>"`, o mesmo
       nível do `plugin/build.sh:50`. Se falhar, o job falha;
    4. garantir o `iscc`: se não estiver no PATH, `choco install innosetup -y`;
    5. compilar o `.iss` com `/DAppVersion=` (a tag sem o `v`, ou `0.0.0-pr`
       fora de tag) e `/DVst3Dir=`;
    6. `actions/upload-artifact`, com nome `instalador`, sempre;
    7. só em tag: `gh release create "$tag" --draft --title "TCC Autotune $tag"`
       com o `.exe` anexado e notas curtas em português.
  - Cache do `plugin/build/_deps` a critério do builder.
- [ ] **Step 5:** compilar localmente com o MSVC.

  Run (em `plugin/`): `build.bat`.

  Expected: `COMPILOU OK`.
- [ ] **Step 6:** se o `iscc` estiver instalado localmente, compilar o `.iss`
  com os mesmos `/D` do workflow.

  Expected: aparece `TCC-Autotune-Setup-Windows.exe` na pasta `Output` do `.iss`.

  Sem `iscc` local, o passo 9 cobre isso.
- [ ] **Step 7: runtime estático.**

  Run: `dumpbin /dependents "plugin/build/TccAutotune_artefacts/Release/VST3/TCC Autotune.vst3/Contents/x86_64-win/TCC Autotune.vst3"`,
  num Developer Prompt do VS.

  Expected: nenhuma linha com `VCRUNTIME140.dll` ou `MSVCP140.dll`.
- [ ] **Step 8:** commitar numa branch nova `release-v1` a partir da `main`.

  Run: `git commit -m "build: add AGPLv3, Windows installer and release workflow"`.

  Com o OK do autor, dar push e abrir o PR.
- [ ] **Step 9:** acompanhar o run do PR.

  Run: `gh run watch`.

  Expected: o run passa o pluginval e o artifact `instalador` contém
  `TCC-Autotune-Setup-Windows.exe`.
- [ ] **Step 10:** pedir ao autor o teste na máquina dele, com o artifact do
  PR, e **parar** até o resultado:
  - instalar, passando pelo SmartScreen;
  - no Ableton, reescanear e abrir o plugin num projeto novo. O fabricante
    aparece como "Gabriel Cherubini";
  - reabrir um projeto salvo durante a avaliação com usuário. O plugin continua
    carregado;
  - instalar de novo por cima: um plugin só na lista;
  - desinstalar: a pasta `C:\Program Files\Common Files\VST3\TCC Autotune.vst3`
    some.

  Com tudo aprovado, o autor faz o merge.

### Task 5: Tag `v1.0.0` e release em rascunho

**Files:** nenhum.

**Interfaces:**
- Consumes: o workflow da Task 4, já mergeado na `main`.
- Produces: o release `v1.0.0`, que o autor publica.

- [ ] **Step 1:** pedir o OK explícito do autor para a tag. Sem OK, parar.
- [ ] **Step 2:** criar e enviar a tag.

  Run: `git switch main && git pull && git tag -a v1.0.0 -m "TCC Autotune 1.0.0" && git push origin v1.0.0`.

  Depois, `gh run watch`.

  Expected: o run da tag termina com sucesso.
- [ ] **Step 3:** conferir o rascunho.

  Run: `gh release view v1.0.0 --json isDraft,assets`.

  Expected: `isDraft: true` e um asset `TCC-Autotune-Setup-Windows.exe`.
- [ ] **Step 4:** pedir ao autor para testar o `.exe` do rascunho e clicar em
  Publicar. **Parar** até ele confirmar.
- [ ] **Step 5:** conferir o link público.

  Run: `curl -sIL https://github.com/gacherubini/TCC-autotune-cpp/releases/latest/download/TCC-Autotune-Setup-Windows.exe | grep -i "^HTTP"`.

  Expected: a última linha é `200`.

### Task 6: Ligar o botão Baixar

**Files:**
- Modify: `site/index.html`, só o elemento `#baixar` e os textos dele, nos dois
  idiomas.

**Interfaces:**
- Consumes: o `#baixar` da Task 3 e o link público da Task 5.

- [ ] **Step 1:** numa branch `site-download` a partir da `main`, trocar o
  `#baixar` por um link ativo com o `href` do Global Constraints e o texto
  "Baixar para Windows"/"Download for Windows".
- [ ] **Step 2:** commitar.

  Run: `git commit -m "feat(site): point the download button at the v1.0.0 release"`.

  Com o OK do autor, dar push e abrir o PR.
- [ ] **Step 3:** depois do merge e do deploy, abrir o site publicado e clicar
  em Baixar.

  Expected: começa o download do `TCC-Autotune-Setup-Windows.exe`.

### Task 7: Links no TCC e upload no Overleaf

**Files:**
- Modify: `C:\Users\guilh\Documents\codigo\TCC\tcc1-sections.tex`. Os pontos:
  - a primeira menção ao estudo em Python;
  - a primeira menção ao protótipo em C++;
  - a `sec:repositorio` (~1485-1501);
  - a proposta para o cronograma (~2857).

**Interfaces:**
- Consumes: a decisão da Task 1 sobre qual versão vale e a URL do site da
  Task 3.

- [ ] **Step 1:** achar a primeira menção de cada projeto.

  Run: `grep -n -i "python\|estudo comparativo\|protótipo\|c++\|juce" tcc1-sections.tex | head -40`.

  A primeira menção pode estar antes da `sec:repositorio`.
- [ ] **Step 2:** acrescentar as notas de rodapé e o link do site:
  - na primeira menção do estudo em Python,
    `\footnote{\url{https://github.com/gacherubini/TCC-autotune-python}}`;
  - na primeira menção do protótipo em C++,
    `\footnote{\url{https://github.com/gacherubini/TCC-autotune-cpp}}`;
  - em `sec:repositorio`, uma frase com
    `\url{https://gacherubini.github.io/TCC-autotune-cpp/}`.

  À parte, preparar a proposta de marcar como realizados "Licença e nome do
  plugin" e as Sprints 14 e 15. Essa proposta é um bloco separado do diff, para
  o autor aceitar ou recusar.
- [ ] **Step 3:** compilar.

  Run (no repositório `TCC`): `make`.

  Expected: o PDF é gerado, sem `Undefined control sequence` nem referência
  `??` nova. Se o `make` não existir localmente, a verificação fica para o
  passo 6.
- [ ] **Step 4:** mostrar o diff ao autor e **parar** até ele aprovar.
- [ ] **Step 5:** commitar na `main` do `TCC-TEXT` e dar push.

  Run: `git commit -m "Link the code repositories and the project site"`.
- [ ] **Step 6:** pelo Chrome, no projeto do Overleaf, subir o
  `tcc1-sections.tex` com Upload → substituir arquivo. Recompilar.

  Expected: o PDF do Overleaf compila, e as notas de rodapé e o link aparecem
  nas páginas certas.
