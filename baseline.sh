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
#
#  Etapa 2: os casos "forca0" viraram DOIS casos, porque a antiga forca=0 fazia
#  duas coisas ao mesmo tempo e so uma delas sobreviveu como parametro:
#    mix0   -> bypass de verdade; o PSOLA roda mas o resultado e' descartado;
#    tol600 -> tolerancia maior que meio semitom => alvo == f0 => beta = 1, e o
#              PSOLA roda EM IDENTIDADE, com o resultado indo para a saida.
#  Os dois tem de dar o MESMO checksum. E' isso que os torna um teste: se o
#  PSOLA ganhar drift de fase, tol600 muda e mix0 nao. Ver o par-a-par abaixo.
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
  rodar gold_mix1        "$BIN/autotune"    "$WAV" gold_mix1.wav        1.0
  rodar gold_mix0        "$BIN/autotune"    "$WAV" gold_mix0.wav        0.0
  rodar gold_tol600      "$BIN/autotune"    "$WAV" gold_tol600.wav      1.0 crom tol=600
  rodar gold_tol30       "$BIN/autotune"    "$WAV" gold_tol30.wav       1.0 crom tol=30
  rodar gold_glide120    "$BIN/autotune"    "$WAV" gold_glide120.wav    1.0 crom glide=120
  rodar gold_cmaior      "$BIN/autotune"    "$WAV" gold_cmaior.wav      1.0 C
  rodar gold_aminor      "$BIN/autotune"    "$WAV" gold_aminor.wav      1.0 Am

  rodar rt_look4         "$BIN/autotune_rt" "$WAV" rt_look4.wav         1.0 crom look=4
  rodar rt_look0         "$BIN/autotune_rt" "$WAV" rt_look0.wav         1.0 crom look=0
  rodar rt_glide0        "$BIN/autotune_rt" "$WAV" rt_glide0.wav        1.0 crom glide=0
  rodar rt_tol0          "$BIN/autotune_rt" "$WAV" rt_tol0.wav          1.0 crom tol=0

  rodar st_mix1          "$BIN/stream_test" "$WAV" st_mix1.wav          1.0
  rodar st_mix0          "$BIN/stream_test" "$WAV" st_mix0.wav          0.0
  rodar st_tol600        "$BIN/stream_test" "$WAV" st_tol600.wav        1.0 crom tol=600
  rodar st_glide40       "$BIN/stream_test" "$WAV" st_glide40.wav       1.0 crom glide=40
  rodar st_tol15         "$BIN/stream_test" "$WAV" st_tol15.wav         1.0 crom tol=15
  rodar st_block64       "$BIN/stream_test" "$WAV" st_block64.wav       1.0 crom block=64
  rodar st_block512      "$BIN/stream_test" "$WAV" st_block512.wav      1.0 crom block=512
  rodar st_cmaior        "$BIN/stream_test" "$WAV" st_cmaior.wav        1.0 C

  # Etapa 3: os controles novos. retune25 e' o padrao do plugin; vibrato0 e
  # vibrato2 sao os extremos do Natural Vibrato.
  rodar st_retune25      "$BIN/stream_test" "$WAV" st_retune25.wav      1.0 crom retune=25
  rodar st_vibrato0      "$BIN/stream_test" "$WAV" st_vibrato0.wav      1.0 crom retune=25 vibrato=0
  rodar st_vibrato2      "$BIN/stream_test" "$WAV" st_vibrato2.wav      1.0 crom retune=25 vibrato=2
  rodar gold_retune25    "$BIN/autotune"    "$WAV" gold_retune25.wav    1.0 crom retune=25
} | tee "$TMP/resumo.txt"

# ---------------------------------------------------------------------------
#  Pares que TEM de bater entre si, independentemente da referencia gravada.
#  Sao invariantes do algoritmo, nao fotografias do passado: valem mesmo depois
#  de um re-baseline legitimo. Se um destes quebrar, o problema e' real.
# ---------------------------------------------------------------------------
echo
echo "== invariantes (independem da referencia) =="
par() {  # $1 = descricao, $2/$3 = arquivos que devem ser identicos
    if cmp -s "$TMP/$2" "$TMP/$3"; then echo "  ok    $1"
    else echo "  FALHA $1  ($2 != $3)"; INVAR_FALHOU=1; fi
}
INVAR_FALHOU=0
par "PSOLA em identidade (beta=1) == bypass  [offline]"   gold_tol600.wav gold_mix0.wav
par "PSOLA em identidade (beta=1) == bypass  [streaming]" st_tol600.wav   st_mix0.wav

# ---------------------------------------------------------------------------
#  Nao-regressao da ETAPA 3 contra a ETAPA 2.
#
#  A Etapa 3 trocou a malha de correcao por outra, mais geral:
#      outCents = LP(alvo) + k*(real - LP(real))
#  A afirmacao que sustenta a mudanca e' que ela NAO perde o comportamento
#  antigo -- ele vira o caso particular k=0 (mais a flag de ataque). Aqui essa
#  afirmacao deixa de ser afirmacao e vira teste: cada caso roda de novo com
#  'legado=1 vibrato=0' e tem de bater com o hash da Etapa 2, gravado em
#  baseline/etapa2-legado.sha256 e NUNCA regravado.
#
#  Isso e' diferente do resumo.txt: aquele e' uma fotografia do estado atual e
#  muda a cada etapa. Este e' um marco fixo no passado. Se as etapas 4 e 5
#  quebrarem a generalizacao, e' aqui que aparece.
# ---------------------------------------------------------------------------
LEG="$REF/etapa2-legado.sha256"
if [[ -f "$LEG" ]]; then
    echo
    echo "== nao-regressao: legado=1 vibrato=0 reproduz a Etapa 2 =="
    L=(legado=1 vibrato=0)
    ( cd "$TMP"
      "$BIN/autotune"    "$WAV" g_gold_mix1.wav      1.0 "${L[@]}"
      "$BIN/autotune"    "$WAV" g_gold_mix0.wav      0.0 "${L[@]}"
      "$BIN/autotune"    "$WAV" g_gold_tol600.wav    1.0 crom tol=600 "${L[@]}"
      "$BIN/autotune"    "$WAV" g_gold_tol30.wav     1.0 crom tol=30 "${L[@]}"
      "$BIN/autotune"    "$WAV" g_gold_glide120.wav  1.0 crom glide=120 "${L[@]}"
      "$BIN/autotune"    "$WAV" g_gold_cmaior.wav    1.0 C "${L[@]}"
      "$BIN/autotune"    "$WAV" g_gold_aminor.wav    1.0 Am "${L[@]}"
      "$BIN/autotune_rt" "$WAV" g_rt_look4.wav       1.0 crom look=4 "${L[@]}"
      "$BIN/autotune_rt" "$WAV" g_rt_look0.wav       1.0 crom look=0 "${L[@]}"
      "$BIN/autotune_rt" "$WAV" g_rt_glide0.wav      1.0 crom glide=0 "${L[@]}"
      "$BIN/autotune_rt" "$WAV" g_rt_tol0.wav        1.0 crom tol=0 "${L[@]}"
      "$BIN/stream_test" "$WAV" g_st_mix1.wav        1.0 "${L[@]}"
      "$BIN/stream_test" "$WAV" g_st_mix0.wav        0.0 "${L[@]}"
      "$BIN/stream_test" "$WAV" g_st_tol600.wav      1.0 crom tol=600 "${L[@]}"
      "$BIN/stream_test" "$WAV" g_st_glide40.wav     1.0 crom glide=40 "${L[@]}"
      "$BIN/stream_test" "$WAV" g_st_tol15.wav       1.0 crom tol=15 "${L[@]}"
      "$BIN/stream_test" "$WAV" g_st_block64.wav     1.0 crom block=64 "${L[@]}"
      "$BIN/stream_test" "$WAV" g_st_block512.wav    1.0 crom block=512 "${L[@]}"
      "$BIN/stream_test" "$WAV" g_st_cmaior.wav      1.0 C "${L[@]}"
    ) > /dev/null 2>&1
    nLeg=0; nBad=0
    while read -r h nome; do
        [[ "$h" == \#* || -z "$h" ]] && continue
        got=$(shasum -a 256 "$TMP/g_$nome.wav" 2>/dev/null | cut -d\  -f1)
        nLeg=$((nLeg+1))
        if [[ "$got" != "$h" ]]; then
            echo "  FALHA $nome  (esperado ${h:0:16}, obtido ${got:0:16})"
            nBad=$((nBad+1)); INVAR_FALHOU=1
        fi
    done < "$LEG"
    [[ $nBad -eq 0 ]] && echo "  ok    $nLeg casos reproduzem a Etapa 2 bit a bit"
else
    echo; echo "  AVISO: sem $LEG — nao-regressao da Etapa 3 nao verificada."
fi

# Invariante quebrada e' defeito, nao mudanca de comportamento: para aqui, antes
# de gravar uma referencia nova por cima de um resultado errado.
if [[ "$INVAR_FALHOU" == "1" ]]; then
    echo; echo "ERRO: invariante quebrada. Isso NAO se resolve com 'gravar' —"
    echo "      significa que o PSOLA deixou de ser identidade em beta=1."
    exit 1
fi

if [[ "$ACAO" == "gravar" ]]; then
    mkdir -p "$REF"
    cp "$TMP/resumo.txt" "$REF/resumo.txt"
    # Os g_*.wav sao do teste de nao-regressao (comparados contra a tabela
    # congelada, nao contra a referencia) — nao entram no baseline gravado.
    for w in "$TMP"/*.wav; do [[ "$(basename "$w")" == g_* ]] || cp "$w" "$REF/"; done
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
