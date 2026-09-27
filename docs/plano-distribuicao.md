# Plano de distribuição pública

Status: **planejado, nada implementado** (anotado em 27/09/2026).
No cronograma do TCC são as Sprints 14 (05/10 a 18/10) e 15 (19/10 a 01/11).

Objetivo: qualquer pessoa entra num site, baixa o plugin para Windows ou macOS e
instala sem compilar nada.

## Como vai funcionar

```
git tag v1.0.0 && git push --tags
        │
        ▼
GitHub Actions ──┬── runner windows-latest ── VST3 ─────────┐
                 └── runner macos-latest ─── VST3 + AU ─────┤
                                                            ▼
                                     GitHub Release v1.0.0 (zips anexados)
                                                            ▲
Site (GitHub Pages) ── botão "Baixar" ── /releases/latest/download/<arquivo>
```

- **Site:** GitHub Pages, site de projeto, em
  `gacherubini.github.io/<nome-do-repo>`. Estático (HTML/CSS/JS), grátis porque o
  repositório é público.
- **Binários:** nas GitHub Releases, nunca dentro do Git (limite de 100 MB por
  arquivo no Git; nas Releases o limite é 2 GB). O link
  `github.com/gacherubini/<repo>/releases/latest/download/<arquivo>` aponta
  sempre para a versão mais nova, então o site não muda a cada release. O GitHub
  conta os downloads de cada arquivo.
- **Sem Mac próprio:** o runner `macos-latest` do Actions compila o binário
  macOS. O `plugin/build.sh` já tem o toolchain de macOS e a validação com o
  pluginval (commit de 26/08/2026), então dá para partir dele.

## Decisões pendentes (bloqueiam o início)

1. **Nome do plugin.** Hoje é `PRODUCT_NAME "TCC Autotune"` e
   `COMPANY_NAME "PUCRS"` em `plugin/CMakeLists.txt`. Trocar os dois:
   - "Auto-Tune" é marca registrada da Antares;
   - o nome da universidade como fabricante exige autorização;
   - o `PLUGIN_MANUFACTURER_CODE Pucr` também muda. **Atenção:** trocar o
     `PLUGIN_CODE` ou o manufacturer code faz as DAWs verem outro plugin, e
     projetos salvos com o nome antigo não o reconhecem. Decidir uma vez só.
   - Renomear o repositório **antes** de divulgar o link: o GitHub redireciona o
     repositório antigo, mas **não** redireciona a URL do Pages.
2. **Conta Apple Developer (US$ 99/ano)?**
   - Com ela: assinar e notarizar o binário macOS; instala sem aviso.
   - Sem ela: o Gatekeeper bloqueia. O site precisa ensinar
     `xattr -dr com.apple.quarantine <caminho do plugin>` no Terminal.
   - No Windows, sem certificado aparece o aviso do SmartScreen, mas dá para
     prosseguir ("Mais informações" → "Executar assim mesmo"). Certificado de
     assinatura para Windows é caro; não vale para este projeto.

## Etapas

### 1. Licença
- O JUCE 8 é **AGPLv3 ou licença comercial**. Para distribuir de graça com o
  código aberto: adicionar `LICENSE` com a AGPLv3 na raiz do repositório.
- Hoje o repositório **não tem licença**, e sem ela ninguém tem direito legal de
  usar o código, mesmo sendo público.
- Conferir a licença do SDK VST3 da Steinberg na versão que o JUCE 8.0.4 traz.

### 2. Nome e identidade
- Aplicar a decisão 1 no `CMakeLists.txt`, na interface e no README.
- Renomear o repositório.

### 3. Build automatizado (a parte mais trabalhosa)
- `.github/workflows/release.yml`, disparado por tag `v*`.
- Windows: MSVC, `FORMATS VST3`, zip do `.vst3`.
- macOS: `FORMATS VST3 AU`, binário universal
  (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`) para Apple Silicon e Intel. O AU é
  o formato do Logic e do GarageBand.
- Rodar o pluginval nos dois antes de anexar.
- Anexar os zips à Release com nomes fixos (ex.: `<Nome>-Windows-VST3.zip`,
  `<Nome>-macOS.zip`) para o link `latest/download` não quebrar.
- Opcional: instalador (Inno Setup no Windows, `.pkg` no macOS) no lugar do zip.

### 4. Site
- Pasta `site/` na raiz. **Não usar `docs/`**, que já guarda a documentação técnica.
- Publicar com o workflow oficial do Pages (`actions/deploy-pages`), disparado
  quando `site/` muda na `main`.
- Uma página só:
  - o que o plugin faz, em duas linhas;
  - exemplos antes e depois (`exemplo-antes.wav`, `exemplo-depois*.wav`,
    convertidos para mp3);
  - os controles explicados (tabela `tab:rev_parametros` do TCC);
  - download por sistema;
  - instalação passo a passo por sistema e por DAW (Ableton, Logic, Reaper);
  - limitações conhecidas: custo do TD-PSOLA em notas longas (usar Low Latency);
  - links para o código e para o TCC.
- Domínio próprio é opcional (ex.: `.com.br` no registro.br, uns R$ 40/ano); o
  Pages faz o HTTPS.
- Dá para publicar o site antes do build, com o botão "em breve".

### 5. Ligar ao TCC
- Seção `sec:repositorio` do `tcc1-sections.tex`: acrescentar o link do site.
- Quando a primeira versão sair, a Sprint 15 do cronograma passa a "Realizado".

## Checklist

- [ ] Nome do plugin decidido
- [ ] Decisão sobre a conta Apple Developer
- [ ] `LICENSE` (AGPLv3)
- [ ] `CMakeLists.txt` com nome, fabricante e `FORMATS VST3 AU`
- [ ] Repositório renomeado
- [ ] Workflow de release (Windows + macOS + pluginval)
- [ ] Primeira Release `v1.0.0` com os dois zips
- [ ] Site em `site/` publicado no Pages
- [ ] Link do site no TCC
