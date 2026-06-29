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
| `build.bat` | configura + compila (atalho dos 2 comandos CMake) |

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

## Parâmetros expostos

| parâmetro | faixa | tipo | efeito |
|---|---|---|---|
| **Forca** | 0–1 | ao vivo | quanto puxa para a nota da escala (1 = "duro") |
| **Tolerancia** | 0–50 cents | ao vivo | zona morta (preserva vibrato) |
| **Glide** | 0–200 ms | ao vivo | portamento até a nota-alvo |
| **Look-ahead** | 0–16 quadros | estrutural | latência × qualidade do Viterbi (re-prepara) |
| **Voz** | preset | estrutural | tessitura → `fmin/fmax` (re-prepara, muda a latência) |
| **Escala** | cromática / tônica | estrutural | grade de notas permitidas |

"Ao vivo" = aplicado sem realocar, a cada bloco. "Estrutural" = muda dimensões/
latência → o núcleo é re-preparado (e o host relê `setLatencySamples`).

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

## Testar rápido SEM DAW (app Standalone)

Rode `build\TccAutotune_artefacts\Release\Standalone\TCC Autotune.exe`. Em
*Options/Audio Settings* escolha o driver (ASIO de preferência), entrada = microfone,
saída = fones. Ajuste os sliders e cante. É o jeito mais rápido de validar antes do Ableton.

> **Nota (resultado do TCC):** a latência que se *ouve* ao vivo = driver_in +
> bloco_host + latência do núcleo + driver_out. O PDC (`setLatencySamples`) alinha
> faixas gravadas, mas não remove o atraso de monitoração. Nosso número (~40–60 ms)
> ser maior que o do Auto-Tune Pro (~1 ms) é **discussão de método**, não defeito.
