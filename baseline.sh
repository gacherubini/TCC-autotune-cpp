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
#    ./baseline.sh gravar-legado   -> regrava a tabela da Etapa 2 (raro, ver abaixo)
#
#  Saida esperada de 'conferir' quando nada mudou: "IDENTICO".
#
#  TRES CLASSES DE FALHA, e elas nao querem dizer a mesma coisa. Ate 03/09/2026 o
#  script tratava as duas ultimas como uma so, e isso escondia a diferenca que
#  mais importa na hora de decidir se um resultado e' defeito ou resultado:
#
#    1. INVARIANTE quebrada  -> DEFEITO, sempre. Um motor deixou de ser
#       identidade em beta = 1, ou a saida passou a depender do tamanho de bloco.
#       Nao se resolve com 'gravar'. O script para aqui.
#    2. TABELA DA ETAPA 2 desatualizada -> pode ser defeito OU consequencia
#       documentada. Ela congela a saida da malha da Etapa 2 DE PONTA A PONTA, e
#       de ponta a ponta inclui o DETECTOR de altura. Uma mudanca deliberada na
#       deteccao (a guarda contra a subharmonica, 03/09/2026) muda a tabela sem
#       que a alegacao dela tenha deixado de valer. Regravavel de proposito, por
#       'gravar-legado', que e' um comando separado justamente para nao acontecer
#       por distracao dentro de um 'gravar'.
#    3. RESUMO diferente -> o esperado a cada etapa que muda audio. Regravavel
#       por 'gravar', com a lista dos casos e a razao de cada um no diario.
#
#  O que a tabela da Etapa 2 continua provando, e o que ela nunca provou sozinha:
#  a alegacao "a malha da Etapa 3+ com legado=1 vibrato=0 reproduz a Etapa 2" e'
#  verificada AMOSTRA A AMOSTRA por src/tests/test_retune.cpp secao 1, contra uma
#  copia congelada do codigo da Etapa 2 e SEM detector no meio. Essa prova e' mais
#  forte que a tabela e nao depende dela.
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
# O glob e a execucao sao ancorados em $RAIZ, e nao no diretorio de onde o script
# foi chamado. Os dois importam, por razoes diferentes:
#
#   - o GLOB, porque 'src/tests/*.cpp' relativo nao casa nada quando o script e'
#     chamado de outro lugar, e o '[[ -e ]] || break' entao pula TODOS os testes
#     em silencio. Uma suite que se auto-desliga conforme o diretorio de chamada
#     e' pior que uma suite que falha;
#   - a EXECUCAO (o subshell com cd), porque test_deteccao.cpp le
#     exemplo-antes.wav por caminho relativo. Ele e' o unico teste que precisa de
#     um arquivo, e sem o cd ele derruba a linha de base inteira com "nao achei
#     exemplo-antes.wav" quando o script roda de fora da raiz.
for t in "$RAIZ"/src/tests/*.cpp; do
    [[ -e "$t" ]] || break
    nome=$(basename "$t" .cpp)
    "$CXX" -std=c++17 -O2 -I "$RAIZ/external" "$t" -o "$BIN/$nome" || {
        echo "ERRO ao compilar $t"; exit 1; }
    if ( cd "$RAIZ" && "$BIN/$nome" ) > "$TMP/$nome.out" 2>&1; then
        echo "  ok    $nome"
    else
        echo "  FALHA $nome:"; cat "$TMP/$nome.out"; exit 1
    fi
done

# ---------------------------------------------------------------------------
#  Casos (37). Cobrem: bypass, correcao cheia, zona morta, glide, look-ahead,
#  invariancia ao tamanho de bloco, escalas diferentes e, desde a Etapa 6, o
#  motor de ponteiro movel (v3) e o modo de baixa latencia (lowlat=1).
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
    # O log traz tempos de execucao; remove numeros de ms/xRT para nao virar ruido.
    # Reduz todo caminho de .wav ao nome do arquivo: o autotune offline imprime o
    # caminho do WAV de ENTRADA ("Compare ouvindo: entrada (...)"), o que tornava
    # os 9 hashes 'log=' dos casos gold_* dependentes da MAQUINA -- eles nunca
    # batiam fora do computador onde a referencia foi gravada, e um 'DIFERENTE'
    # permanente treina quem le a ignorar o script inteiro. Nao da para casar
    # contra $RAIZ: o shell do MSYS converte /c/Users/... para C:/Users/... antes
    # de entregar o argumento ao .exe, entao as duas formas aparecem no log.
    h_log=$(sed -E 's#[^ ()]*[\\/]([^ ()\\/]*\.wav)#\1#g;
                    s/[0-9]+[.,][0-9]+ *(ms|s|xRT)?//g; s/[0-9]+ *ms//g' \
            "$TMP/$nome.log" | shasum -a 256 | cut -d' ' -f1)
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
  rodar st_block1024     "$BIN/stream_test" "$WAV" st_block1024.wav     1.0 crom block=1024

  # Etapas 4 e 5: Humanize e Create Vibrato. Os casos "off" existem para provar
  # que o caminho neutro nao mexe em nada; os "on" fixam o comportamento novo.
  rodar st_humanize0     "$BIN/stream_test" "$WAV" st_humanize0.wav     1.0 crom retune=25 humanize=0
  rodar st_humanize1     "$BIN/stream_test" "$WAV" st_humanize1.wav     1.0 crom retune=25 humanize=1
  rodar st_createvib_off "$BIN/stream_test" "$WAV" st_createvib_off.wav 1.0 crom retune=25 vibforma=0 vibprof=30
  rodar st_createvib_sen "$BIN/stream_test" "$WAV" st_createvib_sen.wav 1.0 crom retune=25 vibforma=1 vibtaxa=5.5 vibprof=30
  rodar st_createvib_amp "$BIN/stream_test" "$WAV" st_createvib_amp.wav 1.0 crom retune=25 vibforma=1 vibtaxa=5.5 vibprof=30 vibamp=1
  rodar gold_humanize1   "$BIN/autotune"    "$WAV" gold_humanize1.wav   1.0 crom retune=25 humanize=1

  # Etapa 6: motor v3 (ponteiro movel). lowlat=1 == motor=ponteiro look=0, que
  # e' exatamente o botao do plugin. Os pares mix0/tol600 e block64/block512 sao
  # invariantes (abaixo); mix1, natural e ponteiro_look4 fixam o comportamento.
  rodar st_lowlat_mix1     "$BIN/stream_test" "$WAV" st_lowlat_mix1.wav     1.0 crom lowlat=1
  rodar st_lowlat_mix0     "$BIN/stream_test" "$WAV" st_lowlat_mix0.wav     0.0 crom lowlat=1
  rodar st_lowlat_tol600   "$BIN/stream_test" "$WAV" st_lowlat_tol600.wav   1.0 crom tol=600 lowlat=1
  rodar st_lowlat_natural  "$BIN/stream_test" "$WAV" st_lowlat_natural.wav  1.0 crom tol=15 retune=25 lowlat=1
  rodar st_lowlat_block64  "$BIN/stream_test" "$WAV" st_lowlat_block64.wav  1.0 crom lowlat=1 block=64
  rodar st_lowlat_block512 "$BIN/stream_test" "$WAV" st_lowlat_block512.wav 1.0 crom lowlat=1 block=512
  rodar st_ponteiro_look4  "$BIN/stream_test" "$WAV" st_ponteiro_look4.wav  1.0 crom motor=ponteiro look=4
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
# Invariancia ao tamanho de bloco. O script sempre rodou os dois casos, mas
# nunca comparou um com o outro -- o invariante estava documentado e nunca
# verificado, e de fato estava QUEBRADO (corrigido em 26/08/2026).
par "invariancia ao tamanho de bloco: 64 == 512"        st_block64.wav  st_block512.wav
par "invariancia ao tamanho de bloco: 64 == 1024"       st_block64.wav  st_block1024.wav
# Etapa 5: "Create Vibrato desligado" nao pode ser "quase desligado". Com
# vibforma=0 a profundidade pedida tem de ser simplesmente ignorada.
par "Create Vibrato off == retune25 puro"               st_createvib_off.wav st_retune25.wav
# Etapa 4: humanize=0 tem de ser exatamente a Etapa 3.
par "humanize=0 == retune25 puro"                       st_humanize0.wav     st_retune25.wav
# Etapa 6: o motor de ponteiro tem os MESMOS dois caminhos de identidade do
# PSOLA, e eles tem de concordar: se divergirem, a interpolacao deixou de ser
# exata em fracao zero ou um salto disparou com beta = 1.
par "ponteiro em identidade (beta=1) == bypass  [lowlat]" st_lowlat_tol600.wav st_lowlat_mix0.wav
par "invariancia ao bloco no ponteiro: 64 == 512"        st_lowlat_block64.wav st_lowlat_block512.wav

# ---------------------------------------------------------------------------
#  Nao-regressao da ETAPA 3 contra a ETAPA 2.
#
#  A Etapa 3 trocou a malha de correcao por outra, mais geral:
#      outCents = LP(alvo) + k*(real - LP(real))
#  A afirmacao que sustenta a mudanca e' que ela NAO perde o comportamento
#  antigo -- ele vira o caso particular k=0 (mais a flag de ataque). Aqui essa
#  afirmacao deixa de ser afirmacao e vira teste: cada caso roda de novo com
#  'legado=1 vibrato=0' e tem de bater com o hash da Etapa 2, gravado em
#  baseline/etapa2-legado.sha256.
#
#  Isso e' diferente do resumo.txt: aquele e' uma fotografia do estado atual e
#  muda a cada etapa. Este e' um marco no passado. Se as etapas 4 e 5 quebrarem
#  a generalizacao, e' aqui que aparece.
#
#  ATE 03/09/2026 esta tabela era descrita como "NUNCA regravada", e o script
#  tratava uma divergencia dela como invariante quebrada. Isso confundia duas
#  coisas. A tabela compara a saida DE PONTA A PONTA, e de ponta a ponta inclui
#  o DETECTOR de altura -- entao ela congelava, sem dizer, muito mais do que a
#  malha de correcao que ela existe para proteger. Quando a guarda contra a
#  subharmonica (D1) corrigiu a deteccao de proposito, 14 dos 19 casos mudaram
#  sem que a alegacao da tabela tivesse deixado de valer um dia.
#
#  A alegacao continua provada, e melhor, por src/tests/test_retune.cpp secao 1:
#  ele compara CorretorAltura contra uma copia CONGELADA do codigo da Etapa 2,
#  amostra a amostra, sem detector no meio. E' essa prova que sustenta a Etapa 3.
#  A tabela aqui vale como rede de seguranca de ponta a ponta, e e' regravavel de
#  proposito por './baseline.sh gravar-legado' -- comando separado justamente
#  para que a regravacao nunca aconteca por distracao dentro de um 'gravar'.
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
    nLeg=0; nBad=0; LEGADO_FALHOU=0
    while read -r h nome; do
        [[ "$h" == \#* || -z "$h" ]] && continue
        got=$(shasum -a 256 "$TMP/g_$nome.wav" 2>/dev/null | cut -d\  -f1)
        nLeg=$((nLeg+1))
        if [[ "$got" != "$h" ]]; then
            echo "  FALHA $nome  (esperado ${h:0:16}, obtido ${got:0:16})"
            # Hash obtido VAZIO nao e' regressao, e' o arquivo nao ter sido lido.
            # Acontece quando este .sha256 vem com fim de linha CRLF (um clone ou
            # um worktree novo no Windows): o 'read' deixa o \r colado no nome e
            # o g_$nome.wav procurado nao existe. Ver .gitattributes.
            [[ -z "$got" ]] && echo "        (hash vazio: confira o fim de linha deste arquivo)"
            nBad=$((nBad+1)); LEGADO_FALHOU=1
        fi
    done < "$LEG"
    [[ $nBad -eq 0 ]] && echo "  ok    $nLeg casos reproduzem a Etapa 2 bit a bit"
    if [[ "$ACAO" == "gravar-legado" ]]; then
        # Regravacao DELIBERADA da tabela da Etapa 2. Comando proprio, e nao um
        # efeito colateral de 'gravar', porque esta tabela e' um marco no passado:
        # ela so deve mudar quando alguem decidiu mudar a deteccao ou a sintese e
        # sabe dizer por que. A razao vai no diario, caso a caso.
        #
        # A checagem de invariante mora LA EMBAIXO, depois deste bloco, e este
        # bloco sai com 'exit 0'. Sem a guarda abaixo, 'gravar-legado' congelaria
        # hashes de um motor que deixou de ser identidade em beta = 1 e ainda
        # devolveria sucesso -- o oposto exato do que o cabecalho deste arquivo
        # promete ("INVARIANTE quebrada -> DEFEITO, sempre"). Uma tabela de
        # referencia gravada por cima de um resultado errado e' pior que nenhuma.
        if [[ "$INVAR_FALHOU" == "1" ]]; then
            echo
            echo "ERRO: invariante quebrada — NAO vou regravar a tabela da Etapa 2."
            echo "      Um motor deixou de ser identidade em beta=1, ou a saida"
            echo "      passou a depender do tamanho de bloco. Conserte primeiro."
            exit 1
        fi
        # Os comentarios do arquivo antigo SOBREVIVEM. Eles carregam a
        # proveniencia da tabela — inclusive a nota de 26/08/2026 explicando que
        # um dos hashes originais estava errado e por que —, e uma regravacao que
        # os apagasse trocaria historia por numeros. Erro cometido na primeira
        # versao deste comando, notado quando a regravacao teve de ser desfeita.
        grep '^#' "$LEG" > "$TMP/leg_hdr.txt" 2>/dev/null || : > "$TMP/leg_hdr.txt"
        { cat "$TMP/leg_hdr.txt"
          printf '# REGRAVADA em %s. A razao, caso a caso, esta no diario.\n' "$(date +%d/%m/%Y)"
          for nome in gold_mix1 gold_mix0 gold_tol600 gold_tol30 gold_glide120 \
                      gold_cmaior gold_aminor rt_look4 rt_look0 rt_glide0 rt_tol0 \
                      st_mix1 st_mix0 st_tol600 st_glide40 st_tol15 st_block64 \
                      st_block512 st_cmaior; do
              printf '%s  %s\n' "$(shasum -a 256 "$TMP/g_$nome.wav" | cut -d' ' -f1)" "$nome"
          done
        } > "$LEG"
        echo; echo "TABELA DA ETAPA 2 REGRAVADA ($(wc -l < "$LEG" | tr -d ' ') casos)."
        echo "Registre no diario a razao de cada caso que mudou."
        exit 0
    fi
else
    echo; echo "  AVISO: sem $LEG — nao-regressao da Etapa 3 nao verificada."
fi

# Invariante quebrada e' defeito, nao mudanca de comportamento: para aqui, antes
# de gravar uma referencia nova por cima de um resultado errado.
if [[ "$INVAR_FALHOU" == "1" ]]; then
    echo; echo "ERRO: invariante quebrada. Isso NAO se resolve com 'gravar' —"
    echo "      significa que um motor deixou de ser identidade em beta=1,"
    echo "      ou que a saida passou a depender do tamanho de bloco do host."
    exit 1
fi

# A tabela da Etapa 2 e' de outra natureza (ver o cabecalho): ela pode divergir
# por defeito OU por mudanca deliberada de deteccao/sintese. O script nao tem
# como distinguir as duas, entao ele NAO decide -- reporta e devolve a decisao a
# quem fez a mudanca. Nao para o 'gravar': a fotografia dos 37 casos e a tabela
# da Etapa 2 sao independentes, e travar uma na outra so' produziria um impasse.
if [[ "${LEGADO_FALHOU:-0}" == "1" ]]; then
    echo
    echo "AVISO: a tabela da Etapa 2 divergiu."
    echo "  Se a deteccao ou a sintese mudou DE PROPOSITO nesta etapa, isto e'"
    echo "  consequencia esperada: confira caso a caso, registre a razao no"
    echo "  diario e regrave com './baseline.sh gravar-legado'."
    echo "  Se nada devia ter mudado, e' regressao — e a malha de correcao tem"
    echo "  prova propria em src/tests/test_retune.cpp secao 1, que roda sem"
    echo "  detector no meio e diz qual das duas coisas quebrou."
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
elif [[ "$ACAO" == "gravar-legado" ]]; then
    # So chega aqui se a tabela nao existir; com ela presente, o bloco da
    # nao-regressao regrava e sai antes.
    echo; echo "ERRO: nao achei $LEG para regravar."; exit 1
else
    echo "Uso: $0 [gravar|conferir|gravar-legado]"; exit 1
fi
