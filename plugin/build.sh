#!/usr/bin/env bash
# ============================================================================
#  build.sh — configura, compila e VALIDA o plugin no macOS/Linux (CAMINHO C2).
#             Contraparte do build.bat (Windows/MSVC).
#
#  Pre-requisitos (macOS):
#    - Xcode Command Line Tools  ->  xcode-select --install
#    - CMake >= 3.22 e Ninja     ->  brew install cmake ninja
#    - (opcional, p/ o passo de teste) pluginval:
#                                    brew install --cask pluginval
#  A 1a execucao baixa o JUCE via FetchContent -> precisa de internet e demora
#  ~1 min. Depois fica em cache em build-mac/_deps.
#
#  Uso, a partir desta pasta (plugin/):
#      ./build.sh            # configura + compila + valida (se houver pluginval)
#      ./build.sh limpo      # apaga build-mac/ antes (rebuild do zero)
#
#  Saida: build-mac/TccAutotune_artefacts/Release/VST3/TCC Autotune.vst3
#         build-mac/TccAutotune_artefacts/Release/Standalone/TCC Autotune.app
#
#  NOTA: o diretorio de build e' "build-mac" (e nao "build") de proposito, para
#  que a arvore do Windows e a do macOS possam coexistir na mesma copia do
#  repositorio sem uma sobrescrever o cache da outra.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"

BUILD=build-mac
VST3="$BUILD/TccAutotune_artefacts/Release/VST3/TCC Autotune.vst3"
PLUGINVAL="/Applications/pluginval.app/Contents/MacOS/pluginval"

[ "${1:-}" = "limpo" ] && rm -rf "$BUILD"

command -v cmake >/dev/null || { echo "ERRO: cmake nao encontrado (brew install cmake)"; exit 1; }

# Ninja se existir (bem mais rapido); senao o gerador padrao.
if command -v ninja >/dev/null; then GEN=(-G Ninja); else GEN=(); fi

echo "== 1/3 configurando =="
cmake -S . -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release

echo "== 2/3 compilando =="
cmake --build "$BUILD"

echo "== 3/3 validando =="
if [ -x "$PLUGINVAL" ]; then
    # strictness 10 = nivel maximo: varre 44.1/48/96 kHz x blocos 64..1024,
    # testa automacao, abrir/fechar editor durante o processamento, salvar e
    # restaurar estado, thread-safety de parametros e fuzzing dos parametros.
    "$PLUGINVAL" --strictness-level 10 --validate "$VST3"
else
    echo "AVISO: pluginval nao instalado -> pulando a validacao."
    echo "       Instale com: brew install --cask pluginval"
fi

echo
echo "COMPILOU OK ->"
echo "  VST3:       $PWD/$VST3"
echo "  Standalone: $PWD/$BUILD/TccAutotune_artefacts/Release/Standalone/TCC Autotune.app"
