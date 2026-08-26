#!/usr/bin/env bash
# =============================================================================
#  sync-overleaf.sh — sincroniza o texto do TCC entre o Overleaf e este repo.
#
#  Conta gratuita do Overleaf não tem integração com Git nem com o GitHub, então
#  o caminho Overleaf -> repo é via ZIP:
#
#      No Overleaf:  Menu -> Download -> Source  (baixa o projeto .zip)
#      Aqui:         ./tcc-texto/sync-overleaf.sh
#
#  O script acha o ZIP mais recente em ~/Downloads, mostra EXATAMENTE o que vai
#  mudar, pede confirmação, e só então aplica sobre tcc-texto/. Nada é commitado
#  automaticamente — você revisa o diff e commita.
#
#  Sentido inverso (repo -> Overleaf): não dá para automatizar sem a integração
#  paga (o "upload de ZIP" do Overleaf cria um projeto NOVO, não mescla no
#  existente). Use:
#
#      ./tcc-texto/sync-overleaf.sh --alterados
#
#  para listar os arquivos que mudaram aqui desde o último sync; suba esses
#  arquivos um a um no Overleaf (Upload -> arquivo de mesmo nome substitui).
#
#  Uso:
#      ./tcc-texto/sync-overleaf.sh [caminho/do/projeto.zip]
#      ./tcc-texto/sync-overleaf.sh --alterados
#      ./tcc-texto/sync-overleaf.sh --help
# =============================================================================
set -euo pipefail

DESTINO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$DESTINO/.." && pwd)"
DOWNLOADS="${HOME}/Downloads"

# -----------------------------------------------------------------------------
#  Arquivos que existem SÓ neste repositório e que o ZIP do Overleaf não contém.
#  Nunca são sobrescritos nem apagados pelo sync.
# -----------------------------------------------------------------------------
PROTEGIDOS=(
    "README.md"                 # o README deste diretório, escrito para o repo
    "sync-overleaf.sh"          # este script
)

# -----------------------------------------------------------------------------
#  O ZIP do Overleaf traz o README da classe EP-TCC como "README.md", que
#  colidiria com o nosso. Renomeamos na entrada.
# -----------------------------------------------------------------------------
renomear_entrada() {
    local dir="$1"
    if [[ -f "$dir/README.md" ]]; then
        mv "$dir/README.md" "$dir/README-classe-ep-tcc.md"
    fi
}

uso() { sed -n '2,32p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

# -----------------------------------------------------------------------------
#  --alterados: o que mudou aqui e precisa subir para o Overleaf na mão.
# -----------------------------------------------------------------------------
listar_alterados() {
    echo "Arquivos de tcc-texto/ alterados em relação ao último commit:"
    echo

    # Os arquivos protegidos só existem neste repo — não devem subir pro Overleaf.
    local filtro=""
    for p in "${PROTEGIDOS[@]}"; do
        filtro+="${filtro:+|}tcc-texto/${p//./\\.}$"
    done

    local mudancas
    mudancas=$(git -C "$REPO" status --short -- tcc-texto | grep -Ev " (${filtro})" || true)

    if [[ -z "$mudancas" ]]; then
        echo "  (nenhum — o repo está igual ao último sync)"
    else
        echo "$mudancas" | sed 's/^/  /'
        echo
        echo "Suba esses arquivos no Overleaf: Upload -> arquivo de mesmo nome substitui o antigo."
    fi
    echo
    echo "Commits que tocaram tcc-texto/ (mais recentes primeiro):"
    git -C "$REPO" log --oneline -5 -- tcc-texto | sed 's/^/  /'
}

# -----------------------------------------------------------------------------
#  Localiza o ZIP: argumento explícito, ou o mais recente em ~/Downloads.
# -----------------------------------------------------------------------------
achar_zip() {
    if [[ $# -ge 1 && -n "${1:-}" ]]; then
        [[ -f "$1" ]] || { echo "ERRO: '$1' não existe." >&2; exit 1; }
        printf '%s' "$1"; return
    fi
    local recente
    recente=$(find "$DOWNLOADS" -maxdepth 1 -name '*.zip' -type f -print0 2>/dev/null \
              | xargs -0 ls -t 2>/dev/null | head -1 || true)
    [[ -n "$recente" ]] || {
        echo "ERRO: nenhum .zip encontrado em $DOWNLOADS." >&2
        echo "      No Overleaf: Menu -> Download -> Source." >&2
        exit 1
    }
    printf '%s' "$recente"
}

# ------------------------------- execução ------------------------------------
case "${1:-}" in
    --help|-h) uso; exit 0 ;;
    --alterados|-a) listar_alterados; exit 0 ;;
esac

ZIP="$(achar_zip "${1:-}")"
echo "ZIP:     $ZIP"
echo "         ($(date -r "$ZIP" '+%d/%m/%Y %H:%M'), $(du -h "$ZIP" | cut -f1))"
echo "Destino: $DESTINO"
echo

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
unzip -q -o "$ZIP" -d "$TMP"

# O ZIP do Overleaf não tem pasta-raiz; se tiver (outra origem), desce um nível.
if [[ ! -f "$TMP/tcc1.tex" ]]; then
    unico=$(find "$TMP" -maxdepth 1 -mindepth 1 -type d | head -1)
    if [[ -n "$unico" && -f "$unico/tcc1.tex" ]]; then TMP_SRC="$unico"; else TMP_SRC="$TMP"; fi
else
    TMP_SRC="$TMP"
fi

if [[ ! -f "$TMP_SRC/tcc1.tex" ]]; then
    echo "ERRO: este ZIP não parece ser o projeto do TCC (não achei tcc1.tex)." >&2
    echo "      Conteúdo encontrado:" >&2
    ls -1 "$TMP_SRC" | head -10 | sed 's/^/        /' >&2
    exit 1
fi

renomear_entrada "$TMP_SRC"

EXCLUDES=()
for p in "${PROTEGIDOS[@]}"; do EXCLUDES+=( --exclude "$p" ); done

echo "=== O que vai mudar ========================================================"
SAIDA=$(rsync -rin --delete "${EXCLUDES[@]}" "$TMP_SRC/" "$DESTINO/" | grep -v '^\.d\.\.t' || true)
if [[ -z "$SAIDA" ]]; then
    echo "  Nada. O repo já está igual ao ZIP."
    exit 0
fi
echo "$SAIDA" | while read -r linha; do
    case "$linha" in
        \*deleting*) echo "  APAGA      ${linha##* }" ;;
        \>f+++++++*) echo "  NOVO       ${linha##* }" ;;
        \>f*)        echo "  ALTERA     ${linha##* }" ;;
        cd*)         echo "  NOVA PASTA ${linha##* }" ;;
        *)           echo "  $linha" ;;
    esac
done
echo "==========================================================================="
echo
echo "Protegidos (nunca tocados): ${PROTEGIDOS[*]}"
echo
read -r -p "Aplicar? [s/N] " resposta
[[ "$resposta" =~ ^[sSyY]$ ]] || { echo "Cancelado. Nada mudou."; exit 0; }

rsync -ri --delete "${EXCLUDES[@]}" "$TMP_SRC/" "$DESTINO/" >/dev/null
echo
echo "Aplicado."
echo
echo "=== git status ============================================================"
git -C "$REPO" status --short -- tcc-texto | sed 's/^/  /'
echo "==========================================================================="
echo
echo "Revise com:  git -C \"$REPO\" diff -- tcc-texto"
echo "Commite com: git -C \"$REPO\" add tcc-texto && git -C \"$REPO\" commit -m \"docs(tcc): sync from Overleaf\""
