# Caminho C2 — plugin VST3 (casca JUCE em volta do núcleo C1)

Transforma o autotune em **plugin VST3 + app Standalone** para rodar **ao vivo no
Ableton Live**. **Não** reescreve DSP: o miolo é o `AutotuneStream`
(`../src/c1_streaming/autotune_stream.h`), já pronto e verificado headless no C1. O JUCE entra
só como adaptador do host (callback de áudio, parâmetros, GUI, latência).

## Arquivos

| arquivo | papel |
|---|---|
| `CMakeLists.txt` | traz o JUCE via `FetchContent` e declara o plugin (`juce_add_plugin`, formatos VST3 + Standalone) |
| `PluginProcessor.h/.cpp` | o `AudioProcessor`: parâmetros (APVTS) ↔ `StreamParams`, `processBlock` → `core.process`, `setLatencySamples` |
| `PluginEditor.h/.cpp` | a tela: `PainelAfinador`, os 9 controles em três grupos e o `TccLookAndFeel` (ver *A tela*, abaixo) |
| `build.bat` | configura + compila no **Windows** (atalho dos 2 comandos CMake) |
| `build.sh` | configura + compila + **valida** no **macOS/Linux** (ver seção própria abaixo) |
| `instalar_vst3.bat` | copia o `.vst3` para a pasta do sistema no Windows — exige prompt de **administrador** |

## A tela (GUI custom, 31/08/2026)

Até 31/08/2026 a tela era a **genérica do JUCE**: um slider ou combo por parâmetro, na ordem em
que foram declarados. Ela deixou de servir quando a faixa chegou a **13 colunas** numa linha
só, no fim da Etapa 5.

O que existe hoje, em `PluginEditor.h/.cpp`, sem uma linha de DSP nova:

- **`PainelAfinador`, em cima.** Um cabeçalho com o nome da **nota-alvo** e a frequência, um
  **arco** que mostra a que distância em cents o cantor está dela com a zona morta da
  `Tolerancia` desenhada dentro, e uma faixa com o **histórico dos últimos 2,5 s** da correção
  aplicada. A quantidade de correção virou um número na tela, no lugar do vão entre duas
  agulhas.
- **Nove controles embaixo, em três grupos:** **Escala** | **Correção** | **Motor**. Os quatro
  widgets do Create Vibrato saíram na [Decisão 8](../docs/historico-e-decisoes.md#decisão-8--create-vibrato-sai-da-interface-fica-no-dsp-2026-08-31);
  é o que fez 13 caberem em 9.
- **Tema verde escuro** (`TccLookAndFeel`). Duas cores carregam significado e não são gosto:
  `destaque` é **sempre** o sinal corrigido e `cantado` é **sempre** a altura crua do cantor.
  Elas são de famílias de matiz diferentes de propósito, senão a leitura do arco se perde.

Três detalhes de implementação que valem para o texto do TCC:

1. **A nota-alvo vem do `fout`, não do `f0`.** Atacar um F# em dó maior faz o motor mirar em F
   ou G; ler a nota mais próxima do `f0` mostrava F#, que a escala nem permite.
2. **O anel do histórico é acumulado no timer da UI, a 60 Hz, não no processor.** O que a faixa
   plota é uma envoltória lenta (o vibrato vive em ~5,5 Hz), então não é preciso um ring buffer
   lock-free dentro do callback de áudio só para desenhar um gráfico.
3. **A formatação do texto dos sliders tem de vir *depois* dos attachments.** O
   `SliderAttachment` instala o próprio `textFromValueFunction` a partir de `param.getText()`,
   que ignora `setNumDecimalPlacesToDisplay` e imprimia `15.0000…` em toda caixa. Foi aí que o
   **Mix passou a ser mostrado em %** (o parâmetro continua 0–1), fechando um item cosmético do
   backlog da Etapa 2.

Verificação: `baseline.sh` responde `IDENTICO` antes e depois (nenhum áudio mudou), e o
`pluginval` no nível 10 passa, incluindo *Editor Automation* e *Fuzz parameters*.

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
| **Humanize** | 0–1 | ao vivo | afrouxa o Retune Speed na sustentação da nota (padrão 0) |
| **Create Vibrato** 🚫 | off/sen/tri/qua | ao vivo | **gera** vibrato (≠ Natural Vibrato, que preserva) |
| **Vibrato Rate** 🚫 | 0,1–10 Hz | ao vivo | taxa do vibrato gerado (padrão 5,5) |
| **Vibrato Depth** 🚫 | 0–100 ct | ao vivo | profundidade do vibrato gerado (padrão 0 = desligado) |
| **Amplitude Amount** 🚫 | 0–1 | ao vivo | modulação de amplitude em sincronia (±3 dB em 1) |
| **Look-ahead** | 0–16 quadros | estrutural | latência × qualidade do Viterbi (re-prepara) |
| **Voz** | preset | estrutural | tessitura → `fmin/fmax` (re-prepara, muda a latência) |
| **Tonica** | 12 opções | estrutural | tônica da escala (Etapa 1) |
| **Escala** | cromática / maior / menor | estrutural | grade de notas permitidas |
| **Low Latency** (`lowlat`) | on/off | estrutural | troca o motor de síntese, Etapa 6 (ver abaixo). Padrão **desligado** |

"Ao vivo" = aplicado sem realocar, a cada bloco. "Estrutural" = muda dimensões/
latência → o núcleo é re-preparado (e o host relê `setLatencySamples`).

🚫 = **existe no parâmetro, não existe na tela.** Os quatro controles do Create Vibrato saíram
do editor em **31/08/2026** ([Decisão 8](../docs/historico-e-decisoes.md#decisão-8--create-vibrato-sai-da-interface-fica-no-dsp-2026-08-31)):
é um **gerador** num protótipo que se declara **corretor**. Eles continuam automatizáveis pelo
host, salvos no estado e alcançáveis pelos CLIs (`vibforma=`, `vibtaxa=`, `vibprof=`,
`vibamp=`). Não é widget esquecido — não "conserte".

**O Mix aparece na tela em %**, e não em 0–1 (desde 31/08/2026). O parâmetro em si continua
0–1: quem muda é só a formatação do texto do slider.

> **`Mix = 0` não é um bypass instantâneo.** A saída passa a ser a entrada **atrasada da
> latência do motor**, não a entrada de agora. É o comportamento correto para um plugin que
> reporta latência: se o bypass não atrasasse, mexer no Mix deslocaria o áudio no tempo. O
> host compensa um atraso fixo; não compensa um atraso que aparece e some.

## O botão Low Latency (motor v3, Etapa 6)

Um `ToggleButton` **"Low Latency"** no grupo *Motor*, id de parâmetro estável `"lowlat"`
(`AudioParameterBool`, padrão **desligado**). É **estrutural**: liga/desliga dispara re-prepare,
porque muda a latência declarada.

Ligado, ele força duas coisas de uma vez — **não** são controles independentes enquanto o botão
estiver ativo:

- **O motor de síntese vira o ponteiro móvel**, no lugar do TD-PSOLA.
- **`look` é forçado a 0.** O slider de Look-ahead continua **visível**, mas fica **desabilitado
  e mostra 0** — o valor salvo no parâmetro não é sobrescrito, só ignorado enquanto o botão está
  ligado; desligar devolve exatamente o `look` de antes.

O que muda na saída: `setLatencySamples()` passa a reportar **8 amostras (0,18 ms)**, contra os
milhares de amostras (`fs/FMIN`) do TD-PSOLA — o Ableton mostra 8 em vez de, por exemplo, 2552 no
preset contralto. A **nota** de saída não muda (é o mesmo `β`); o que muda é como o esticamento é
feito, o tipo de artefato e uma parte de latência **variável** (0 a T, o período da nota cantada)
que **não** entra nesse número declarado. Mecânica completa, verificação e a medição comparando
os dois motores: [`../docs/especificacao-v3-ponteiro.md`](../docs/especificacao-v3-ponteiro.md) e
a [Etapa 6 do diário](../docs/execucao-do-plano.md#etapa-6--motor-v3-de-ponteiro-móvel-low-latency).

Padrão **desligado** por dois motivos: uma instalação nova tem de soar como antes, e a linha de
base (`baseline.sh`) mede o motor padrão (TD-PSOLA).

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
> faixas gravadas, mas **não remove o atraso de monitoração**.
>
> ⚠️ **Cite a latência sempre junto com o FMIN.** A guarda do PSOLA é `2·fs/FMIN`, então o
> número muda com o preset de voz: **71,4 ms** com o FMIN padrão de 80 Hz e **57,9 ms** com
> `voz=contralto` (FMIN 175 Hz).
>
> A diferença para o Auto-Tune Pro (37 amostras, 0,84 ms) **não é de ajuste, é de
> arquitetura**: a latência dele é fixa em amostras, a nossa é proporcional a `fs/FMIN`.
> Ele não faz análise-e-ressíntese — corrige com um ponteiro de leitura móvel sobre um buffer
> circular, e o áudio nunca espera pela detecção. A dedução completa, com a patente e o que
> seria preciso para chegar perto, está em
> [`../docs/pesquisa-latencia-antares.md`](../docs/pesquisa-latencia-antares.md).
