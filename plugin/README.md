# Caminho C2 — plugin VST3 (casca JUCE em volta do núcleo C1)

Transforma o autotune em **plugin VST3 + app Standalone** para rodar **ao vivo no
Ableton Live** (Windows). **Não** reescreve DSP: o miolo é o `AutotuneStream`
(`../src/autotune_stream.h`), já pronto e verificado headless no C1. O JUCE entra
só como adaptador do host (callback de áudio, parâmetros, GUI, latência).

## Arquivos

| arquivo | papel |
|---|---|
| `CMakeLists.txt` | traz o JUCE via `FetchContent` e declara o plugin (`juce_add_plugin`, formatos VST3 + Standalone) |
| `PluginProcessor.h/.cpp` | o `AudioProcessor`: parâmetros (APVTS) ↔ `StreamParams`, `processBlock` → `core.process`, `setLatencySamples` |
| `build.bat` | configura + compila no **Windows** (atalho dos 2 comandos CMake) |
| `build.sh` | configura + compila + **valida** no **macOS/Linux** (ver seção própria abaixo) |

GUI: por enquanto a **genérica do JUCE** (sliders/combos automáticos a partir dos
parâmetros). Uma GUI custom é refinamento posterior.

## Pré-requisitos

- **CMake ≥ 3.22** e **git** (já instalados).
- **MSVC "Build Tools 2022"** com a workload **"Desktop development with C++"**
  (`cl.exe`) — o caminho oficial do JUCE para VST3 no Windows. Instalação:
  ```
  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
  ```
- Internet na **1ª** configuração (baixa o JUCE; depois fica em cache no `build/`).

## Compilar

Nesta pasta (`plugin/`):

```
build.bat
```

ou manualmente:

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Saída: `build/TccAutotune_artefacts/Release/VST3/TCC Autotune.vst3` e o
`build/TccAutotune_artefacts/Release/Standalone/TCC Autotune.exe`.

> A auto-instalação na pasta VST3 do **sistema** (`C:\Program Files\Common Files\VST3`)
> está **desligada** no CMake porque exige **admin** (causava o erro `MSB3073` no build).
> Para instalar no sistema, rode `instalar_vst3.bat` num prompt **como administrador**.
> Sem admin: aponte o Ableton para a pasta de build (ver abaixo).

## Compilar no macOS

Acrescentado em **26/08/2026**. O `CMakeLists.txt` não tem nada específico de plataforma —
serve aos dois sistemas sem alteração.

Pré-requisitos:

```
xcode-select --install               # Command Line Tools (NÃO precisa do Xcode completo)
brew install cmake ninja
brew install --cask pluginval        # opcional, para o passo de validação
```

Depois, nesta pasta:

```
./build.sh          # configura + compila + valida
./build.sh limpo    # apaga build-mac/ antes, para recompilar do zero
```

Saída: `build-mac/TccAutotune_artefacts/Release/VST3/TCC Autotune.vst3` e o
`.../Standalone/TCC Autotune.app`.

> O diretório é `build-mac`, e não `build`, **de propósito**: assim a árvore do Windows e a do
> macOS coexistem na mesma cópia do repositório sem uma sobrescrever o cache da outra.

### Validação automatizada

O `build.sh` termina rodando o [pluginval](https://github.com/Tracktion/pluginval) no nível de
rigor **10 (máximo)**, que carrega o VST3 num host de verdade e testa: processamento a 44,1 /
48 / 96 kHz × blocos de 64 a 1024, automação em sub-blocos, abrir e fechar a GUI **durante** o
callback de áudio, salvar e restaurar estado, thread-safety dos parâmetros e fuzzing. Saída
esperada: `SUCCESS`.

Ele é o complemento do `test_escalas` (rodado pelo `baseline.sh`): o teste unitário prova que a
lógica está certa, o `pluginval` prova que o plugin não quebra o host. Detalhes e resultado em
[`../docs/execucao-do-plano.md`](../docs/execucao-do-plano.md), seção *Etapa 1-bis*.

### Testar rápido sem DAW (macOS)

```
open "build-mac/TccAutotune_artefacts/Release/Standalone/TCC Autotune.app"
```

Em *Options → Audio/MIDI Settings* escolha entrada (microfone) e saída (fones — **use fones**,
senão realimenta). É o jeito mais rápido de ver a GUI e ouvir o efeito.

## Parâmetros expostos

| parâmetro | faixa | tipo | efeito |
|---|---|---|---|
| **Mix** | 0–1 | ao vivo | seco/molhado: 0 = só a entrada, 1 = só o corrigido (padrão 1) |
| **Tolerancia** | 0–50 cents | ao vivo | zona morta (preserva vibrato) |
| **Retune Speed** | 0–200 ms | ao vivo | tempo até a nota. Padrão 25 ms (Antares: 10–50 é típico); 0 = efeito "duro" |
| **Natural Vibrato** | 0–2 | ao vivo | 0 = remove o vibrato, 1 = preserva (padrão), 2 = dobra |
| **Look-ahead** | 0–16 quadros | estrutural | latência × qualidade do Viterbi (re-prepara) |
| **Voz** | preset | estrutural | tessitura → `fmin/fmax` (re-prepara, muda a latência) |
| **Tonica** | 12 opções | estrutural | tônica da escala (Etapa 1) |
| **Escala** | cromática / maior / menor | estrutural | grade de notas permitidas |

"Ao vivo" = aplicado sem realocar, a cada bloco. "Estrutural" = muda dimensões/
latência → o núcleo é re-preparado (e o host relê `setLatencySamples`).

> **`Mix = 0` não é um bypass instantâneo.** A saída passa a ser a entrada **atrasada da
> latência do motor**, não a entrada de agora. É o comportamento correto para um plugin que
> reporta latência: se o bypass não atrasasse, mexer no Mix deslocaria o áudio no tempo. O
> host compensa um atraso fixo; não compensa um atraso que aparece e some.

## Testar no Ableton

1. Compile (acima). O `.vst3` fica em
   `build\TccAutotune_artefacts\Release\VST3\TCC Autotune.vst3` (a auto-instalação
   no sistema está desligada — ver nota acima).
2. No Ableton: *Options → Preferences → Plug-Ins* → ative **"Use VST3 Plug-In
   Custom Folder"** e aponte para a pasta **`...\plugin\build\TccAutotune_artefacts\Release\VST3`**;
   clique **Rescan**. (Ou rode `instalar_vst3.bat` como **admin** para jogar na
   pasta do sistema.)
3. Insira **TCC Autotune** numa faixa de áudio com a voz; cante monitorando.
4. A latência reportada aparece na barra inferior (ex.: *"Latency: NNNN samples"*).
   Use **ASIO** e `look` pequeno (0–4) para cantar confortável.

## Testar rápido SEM DAW (app Standalone — Windows)

Rode `build\TccAutotune_artefacts\Release\Standalone\TCC Autotune.exe`. Em
*Options/Audio Settings* escolha o driver (ASIO de preferência), entrada = microfone,
saída = fones. Ajuste os sliders e cante. É o jeito mais rápido de validar antes do Ableton.

> **Nota (resultado do TCC):** a latência que se *ouve* ao vivo = driver_in +
> bloco_host + latência do núcleo + driver_out. O PDC (`setLatencySamples`) alinha
> faixas gravadas, mas não remove o atraso de monitoração. Nosso número (~40–60 ms)
> ser maior que o do Auto-Tune Pro (~1 ms) é **discussão de método**, não defeito.
