@echo off
REM ============================================================================
REM  build.bat — configura e compila o plugin VST3 + Standalone (CAMINHO C2).
REM
REM  Pre-requisito: MSVC "Build Tools 2022" com a workload C++ instalado
REM  (cl.exe). CMake acha o toolset sozinho pelo gerador "Visual Studio 17 2022".
REM  A 1a execucao baixa o JUCE (FetchContent) -> precisa de internet e demora.
REM
REM  Uso: abra ESTA pasta (plugin\) num prompt e rode  build.bat
REM  Saida: build\TccAutotune_artefacts\Release\VST3\TCC Autotune.vst3
REM         (e Standalone\TCC Autotune.exe). O VST3 e copiado p/ a pasta de
REM         plugins do usuario (COPY_PLUGIN_AFTER_BUILD).
REM ============================================================================
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto erro
cmake --build build --config Release || goto erro
echo.
echo COMPILOU OK -^> procure o .vst3 em build\TccAutotune_artefacts\Release\VST3
goto fim
:erro
echo.
echo ERRO na compilacao (MSVC instalado? internet p/ baixar o JUCE?)
:fim
