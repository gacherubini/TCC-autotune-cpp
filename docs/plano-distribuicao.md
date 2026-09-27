# Plano de distribuição pública

Status: **planejado, nada implementado** (anotado em 27/09/2026).
No cronograma do TCC são as Sprints 14 (05/10 a 18/10) e 15 (19/10 a 01/11).

Objetivo: qualquer pessoa entra num site, baixa o plugin para **Windows** e
instala sem compilar nada.

**Escopo: só Windows, só VST3.** Decisão do autor em 27/09/2026. O macOS
(VST3 + AU) e o Pro Tools (AAX) ficam fora da distribuição. O texto do TCC
registra isso na seção `sec:impl_formato` (tabela `tab:compat_daws`) e no item
"Distribuição pública" da Conclusão.

## Compatibilidade

| DAW (Windows) | Carrega? | Por quê |
|---|---|---|
| Ableton Live | Sim | VST3. **Única testada.** |
| FL Studio, Reaper, Cubase, Studio One, Bitwig | Sim, previsto | VST3. Não testadas (Sprint 16). |
| Pro Tools | Não | Só aceita AAX: conta de desenvolvedor Avid, SDK e assinatura PACE. |
| Logic Pro, GarageBand | Não | Só existem no macOS e só aceitam AU. |

## Como vai funcionar

```
git tag v1.0.0 && git push --tags
        │
        ▼
GitHub Actions ── runner windows-latest ── VST3 + pluginval ──┐
                                                              ▼
                                     GitHub Release v1.0.0 (zip anexado)
                                                              ▲
Site (GitHub Pages) ── botão "Baixar" ── /releases/latest/download/<arquivo>
```

- **Site:** GitHub Pages, site de projeto, em
  `gacherubini.github.io/<nome-do-repo>`. Estático (HTML/CSS/JS), grátis porque o
  repositório é público.
- **Binário:** nas GitHub Releases, nunca dentro do Git (limite de 100 MB por
  arquivo no Git; nas Releases o limite é 2 GB). O link
  `github.com/gacherubini/<repo>/releases/latest/download/<arquivo>` aponta
  sempre para a versão mais nova, então o site não muda a cada release. O GitHub
  conta os downloads de cada arquivo.
- **Assinatura:** sem certificado de assinatura de código, o Windows mostra o
  aviso do SmartScreen na primeira execução do instalador ("Mais informações" →
  "Executar assim mesmo"). Um `.vst3` copiado à mão para a pasta de plugins não
  passa por esse aviso. Certificado é caro; não vale para este projeto. O site
  explica o aviso.

## Decisão pendente (bloqueia o início)

**Nome do plugin.** Hoje é `PRODUCT_NAME "TCC Autotune"` e
`COMPANY_NAME "PUCRS"` em `plugin/CMakeLists.txt`. Trocar os dois:
- "Auto-Tune" é marca registrada da Antares;
- o nome da universidade como fabricante exige autorização;
- o `PLUGIN_MANUFACTURER_CODE Pucr` também muda. **Atenção:** trocar o
  `PLUGIN_CODE` ou o manufacturer code faz as DAWs verem outro plugin, e
  projetos salvos com o nome antigo não o reconhecem. Decidir uma vez só.
- Renomear o repositório **antes** de divulgar o link: o GitHub redireciona o
  repositório antigo, mas **não** redireciona a URL do Pages.

## Etapas

### 1. Licença
- O JUCE 8 é **AGPLv3 ou licença comercial**. Para distribuir de graça com o
  código aberto: adicionar `LICENSE` com a AGPLv3 na raiz do repositório.
- Hoje o repositório **não tem licença**, e sem ela ninguém tem direito legal de
  usar o código, mesmo sendo público.
- Conferir a licença do SDK VST3 da Steinberg na versão que o JUCE 8.0.4 traz.

### 2. Nome e identidade
- Aplicar a decisão pendente no `CMakeLists.txt`, na interface e no README.
- Renomear o repositório.

### 3. Build automatizado
- `.github/workflows/release.yml`, disparado por tag `v*`, runner `windows-latest`.
- MSVC, `FORMATS VST3 Standalone` (como hoje), zip do `.vst3`.
- Rodar o pluginval antes de anexar.
- Anexar o zip à Release com nome fixo (ex.: `<Nome>-Windows-VST3.zip`) para o
  link `latest/download` não quebrar.
- Opcional: instalador com Inno Setup, que copia o `.vst3` para
  `C:\Program Files\Common Files\VST3` (pede admin).

### 4. Site
- Pasta `site/` na raiz. **Não usar `docs/`**, que já guarda a documentação técnica.
- Publicar com o workflow oficial do Pages (`actions/deploy-pages`), disparado
  quando `site/` muda na `main`.
- Uma página só:
  - o que o plugin faz, em duas linhas;
  - exemplos antes e depois (`exemplo-antes.wav`, `exemplo-depois*.wav`,
    convertidos para mp3);
  - os controles explicados (tabela `tab:rev_parametros` do TCC);
  - download (Windows, VST3) e a tabela de compatibilidade acima, deixando claro
    que não há versão para macOS nem para Pro Tools;
  - instalação passo a passo, com o caminho da pasta VST3 em cada DAW;
  - limitações conhecidas: custo do TD-PSOLA em notas longas (usar Low Latency);
  - links para o código e para o TCC.
- Domínio próprio é opcional (ex.: `.com.br` no registro.br, uns R$ 40/ano); o
  Pages faz o HTTPS.
- Dá para publicar o site antes do build, com o botão "em breve".

### 5. Ligar ao TCC
- Seção `sec:repositorio` do `tcc1-sections.tex`: acrescentar o link do site.
- Quando a primeira versão sair, a Sprint 15 do cronograma passa a "Realizado".
- Quando as DAWs da Sprint 16 forem testadas, atualizar a `tab:compat_daws`.

## Fora do escopo (registrado para o futuro)

- **macOS:** o `plugin/build.sh` já tem o toolchain de macOS e o pluginval
  (commit de 26/08/2026). Seria `FORMATS VST3 AU`, binário universal
  (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`), compilado no runner
  `macos-latest`. Sem a conta Apple Developer (US$ 99/ano) o Gatekeeper bloqueia
  e o usuário precisa rodar `xattr -dr com.apple.quarantine <plugin>`.
- **Pro Tools (AAX):** conta de desenvolvedor Avid, SDK AAX e assinatura PACE.

## Checklist

- [ ] Nome do plugin decidido
- [ ] `LICENSE` (AGPLv3)
- [ ] `CMakeLists.txt` com nome e fabricante novos
- [ ] Repositório renomeado
- [ ] Workflow de release (Windows + pluginval)
- [ ] Primeira Release `v1.0.0` com o zip
- [ ] Site em `site/` publicado no Pages
- [ ] Link do site no TCC
