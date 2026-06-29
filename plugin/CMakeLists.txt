# ============================================================================
#  CMakeLists.txt — CAMINHO C2: casca de plugin (JUCE) em volta do núcleo C1
#
#  Constrói o autotune em tempo real como plugin VST3 + app Standalone, para
#  rodar no Ableton Live (ou no app avulso, p/ dev sem DAW). NÃO reescreve DSP:
#  o miolo é o `AutotuneStream` (../src/c1_streaming/autotune_stream.h), já pronto e testado
#  headless no C1. O JUCE entra só como "host adapter" (callback de áudio,
#  parâmetros, GUI, relatório de latência).
#
#  Pré-requisitos (Windows): CMake ≥ 3.22, git, e o COMPILADOR MSVC (cl.exe) do
#  "Build Tools 2022" com a workload C++ — o caminho oficial do JUCE p/ VST3.
#  O JUCE é baixado automaticamente via FetchContent (precisa de internet na
#  1ª configuração; depois fica em cache no diretório de build).
#
#  Como compilar (num "x64 Native Tools Command Prompt" OU com o MSVC no PATH):
#     cmake -S . -B build -G "Visual Studio 17 2022" -A x64
#     cmake --build build --config Release
#  Saída: build/TccAutotune_artefacts/Release/VST3/TCC Autotune.vst3
#         (COPY_PLUGIN_AFTER_BUILD copia p/ a pasta de VST3 do usuário)
# ============================================================================
cmake_minimum_required(VERSION 3.22)
project(TccAutotune VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# 1. Traz o JUCE (framework de áudio/plugin) via FetchContent. GIT_SHALLOW
#    pega só o tag pedido (download menor). Pode-se trocar o GIT_TAG por uma
#    versão mais nova; 8.0.4 é estável e tem suporte a VST3 no Windows/MSVC.
# ----------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
# 2. Declara o plugin. FORMATS VST3 (p/ o Ableton) + Standalone (p/ testar sem
#    DAW). IS_SYNTH FALSE = é um EFEITO (recebe áudio, não gera). Os *_CODE são
#    identificadores de 4 chars exigidos pelo formato (manufacturer/plugin).
# ----------------------------------------------------------------------------
juce_add_plugin(TccAutotune
    PRODUCT_NAME              "TCC Autotune"
    COMPANY_NAME              "PUCRS"
    PLUGIN_MANUFACTURER_CODE  Pucr
    PLUGIN_CODE               Tcat
    FORMATS                   VST3 Standalone
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    # NÃO auto-instalar: o destino padrão (C:\Program Files\Common Files\VST3) é
    # protegido e exige ADMIN -> o passo de cópia falhava (MSB3073). Deixamos o
    # .vst3 na pasta de build e apontamos o Ableton para ela (Preferences ->
    # Plug-Ins -> "VST3 Custom Folder"), sem precisar de admin. (Para instalar no
    # sistema, rode `instalar_vst3.bat` num prompt de ADMIN.)
    COPY_PLUGIN_AFTER_BUILD   FALSE)

# ----------------------------------------------------------------------------
# 3. Fontes do plugin (a casca). O núcleo é header-only (autotune_stream.h +
#    dsp.h), então entra pelos include dirs abaixo — sem .cpp extra.
# ----------------------------------------------------------------------------
target_sources(TccAutotune PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

# Acha ../src (c1_streaming/autotune_stream.h, core/dsp.h) e ../external
# (dr_wav.h, só header — NÃO definimos DR_WAV_IMPLEMENTATION, então nenhum
# I/O de WAV é compilado).
target_include_directories(TccAutotune PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../external)

# Flags de compilação que o JUCE recomenda + desliga features que não usamos.
target_compile_definitions(TccAutotune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    DONT_SET_USING_JUCE_NAMESPACE=1)

# Módulos do JUCE necessários: audio_utils traz AudioProcessor, APVTS, a GUI
# genérica e o wrapper de plugin. Os "recommended_*" são flags de build.
target_link_libraries(TccAutotune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
