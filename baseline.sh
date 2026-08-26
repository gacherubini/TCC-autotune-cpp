#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  baseline.sh — captura ou confere a "impressao digital" da saida do projeto.
#
#  Existe por causa do plano de implementacao (docs/plano-de-implementacao.md):
#  cada etapa precisa provar que nao mudou o que nao devia. O script roda os
#  tres executaveis sobre o WAV de exemplo com varias combinacoes de parametros
#  e resume tudo em checksums.
#
#    ./baseline.sh gravar          -> grava a referencia em baseline/
#    ./baseline.sh conferir        -> compara o estado atual contra a referencia
#
#  Saida esperada de 'conferir' quando nada mudou: "IDENTICO".
# ---------------------------------------------------------------------------
set -u
RAIZ="$(cd "$(dirname "$0")" && pwd)"
ACAO="${1:-conferir}"
REF="$RAIZ/baseline"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

WAV="$RAIZ/exemplo-antes.wav"
[[ -f "$WAV" ]] || { echo "ERRO: falta $WAV"; exit 1; }

# Compilador: g++ no Windows/Linux, clang++ no macOS. Ambos aceitam as flags.
CXX="${CXX:-}"
if [[ -z "$CXX" ]]; then command -v g++ >/dev/null && CXX=g++ || CXX=clang++; fi

echo "== compilando ($CXX) =="
BIN="$TMP/bin"; mkdir -p "$BIN"
for par in "src/offline_causal/main.cpp:autotune" \
           "src/offline_causal/autotune_rt.cpp:autotune_rt" \
           "src/c1_streaming/stream_test.cpp:stream_test"; do
    src="${par%%:*}"; exe="${par##*:}"
    "$CXX" -std=c++17 -O2 -I "$RAIZ/external" "$RAIZ/$src" -o "$BIN/$exe" || {
        echo "ERRO ao compilar $src"; exit 1; }
done

# ---------------------------------------------------------------------------
#  Testes de unidade que se auto-verificam (nao entram no checksum: ou passam
#  ou o script para aqui).
# ---------------------------------------------------------------------------
echo "== testes de unidade =="
for t in src/tests/*.cpp; do
    [[ -e "$t" ]] || break
    nome=$(basename "$t" .cpp)
    "$CXX" -std=c++17 -O2 -I "$RAIZ/external" "$RAIZ/$t" -o "$BIN/$nome" || {
        echo "ERRO ao compilar $t"; exit 1; }
    if "$BIN/$nome" > "$TMP/$nome.out" 2>&1; then
        echo "  ok    $nome"
    else
        echo "  FALHA $nome:"; cat "$TMP/$nome.out"; exit 1
    fi
done

# ---------------------------------------------------------------------------
#  Casos. Cobrem: bypass, correcao cheia, zona morta, glide, look-ahead,
#  invariancia ao tamanho de bloco e escalas diferentes.
# ---------------------------------------------------------------------------
rodar() {  # $1 = nome do caso, $@ = comando
    local nome="$1"; shift
    ( cd "$TMP" && "$@" ) > "$TMP/$nome.log" 2>&1
    local h_wav="ausente" h_log
    [[ -f "$TMP/$nome.wav" ]] && h_wav=$(shasum -a 256 "$TMP/$nome.wav" | cut -d' ' -f1)
    # o log traz tempos de execucao; remove numeros de ms/xRT para nao virar ruido
    h_log=$(sed -E 's/[0-9]+[.,][0-9]+ *(ms|s|xRT)?//g; s/[0-9]+ *ms//g' "$TMP/$nome.log" \
            | shasum -a 256 | cut -d' ' -f1)
    printf '%-28s wav=%s log=%s\n' "$nome" "${h_wav:0:16}" "${h_log:0:16}"
}

echo "== rodando casos =="
{
  rodar gold_forca1      "$BIN/autotune"    "$WAV" gold_forca1.wav      1.0
  rodar gold_forca0      "$BIN/autotune"    "$WAV" gold_forca0.wav      0.0
  rodar gold_tol30       "$BIN/autotune"    "$WAV" gold_tol30.wav       1.0 crom tol=30
  rodar gold_glide120    "$BIN/autotune"    "$WAV" gold_glide120.wav    1.0 crom glide=120
  rodar gold_cmaior      "$BIN/autotune"    "$WAV" gold_cmaior.wav      1.0 C
  rodar gold_aminor      "$BIN/autotune"    "$WAV" gold_aminor.wav      1.0 Am

  rodar rt_look4         "$BIN/autotune_rt" "$WAV" rt_look4.wav         1.0 crom look=4
  rodar rt_look0         "$BIN/autotune_rt" "$WAV" rt_look0.wav         1.0 crom look=0
  rodar rt_glide0        "$BIN/autotune_rt" "$WAV" rt_glide0.wav        1.0 crom glide=0
  rodar rt_tol0          "$BIN/autotune_rt" "$WAV" rt_tol0.wav          1.0 crom tol=0

  rodar st_forca1        "$BIN/stream_test" "$WAV" st_forca1.wav        1.0
  rodar st_forca0        "$BIN/stream_test" "$WAV" st_forca0.wav        0.0
  rodar st_glide40       "$BIN/stream_test" "$WAV" st_glide40.wav       1.0 crom glide=40
  rodar st_tol15         "$BIN/stream_test" "$WAV" st_tol15.wav         1.0 crom tol=15
  rodar st_block64       "$BIN/stream_test" "$WAV" st_block64.wav       1.0 crom block=64
  rodar st_block512      "$BIN/stream_test" "$WAV" st_block512.wav      1.0 crom block=512
  rodar st_cmaior        "$BIN/stream_test" "$WAV" st_cmaior.wav        1.0 C
} | tee "$TMP/resumo.txt"

if [[ "$ACAO" == "gravar" ]]; then
    mkdir -p "$REF"
    cp "$TMP/resumo.txt" "$REF/resumo.txt"
    cp "$TMP"/*.wav "$REF/" 2>/dev/null
    echo; echo "REFERENCIA GRAVADA em baseline/ ($(ls "$REF"/*.wav 2>/dev/null | wc -l | tr -d ' ') wavs)"
elif [[ "$ACAO" == "conferir" ]]; then
    [[ -f "$REF/resumo.txt" ]] || { echo; echo "ERRO: sem referencia. Rode './baseline.sh gravar' primeiro."; exit 1; }
    echo
    if diff -u "$REF/resumo.txt" "$TMP/resumo.txt" > "$TMP/diff.txt"; then
        echo "IDENTICO — nada mudou."
    else
        echo "DIFERENTE:"; cat "$TMP/diff.txt"
        echo; echo "Para ver onde a onda diverge, compare os .wav em baseline/ com os gerados."
        exit 1
    fi
else
    echo "Uso: $0 [gravar|conferir]"; exit 1
fi
