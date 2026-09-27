#!/usr/bin/env python3
# =============================================================================
#  medir_qualidade.py — linha de base de QUALIDADE dos tres caminhos de audio.
#
#  POR QUE ESTE SCRIPT EXISTE
#  --------------------------
#  O plano de implementacao (docs/plano-de-implementacao.md, §9.2) exige gravar
#  a qualidade ANTES da Etapa 3, porque a Etapa 3 muda o proprio gold (o motor
#  offline): depois dela, qualquer numero de "similaridade com o offline" medido
#  antes deixa de ser comparavel. Sem essa fotografia a tabela de resultados do
#  TCC fica sem referencia.
#
#  Os scripts antigos (bench_stream.py, bench_pitch.py, bench_frames.py,
#  bench_latencia.py) fazem medicoes parecidas, mas foram escritos para Windows:
#  procuram './autotune_rt.exe' no diretorio corrente, usam um 'audioteste.wav'
#  que nao esta versionado, e dependem do venv de um repositorio irmao. Nenhum
#  deles roda numa maquina limpa. Este aqui roda: compila o que precisa, usa o
#  'exemplo-antes.wav' que ESTA no repo, e nao depende de '.exe' nem de Windows.
#
#  O QUE ELE MEDE
#  --------------
#    1. Latencia algoritmica reportada e fator de tempo real (xRT).
#    2. Correlacao amostra-a-amostra entre os caminhos (global e so nas regioes
#       vozeadas), com o alinhamento da latencia feito explicitamente.
#    3. Concordancia da trilha de F0 entre o streaming e o causal.
#    4. Contagem de "pipoco" (descontinuidades grandes) em cada saida.
#    5. Invariancia ao tamanho de bloco do host (64/128/256/512).
#
#  UMA ADVERTENCIA SOBRE A PALAVRA "GOLD"
#  --------------------------------------
#  Ela e' usada com DOIS sentidos no projeto, e a diferenca importa muito para
#  os numeros:
#    * CLAUDE.md e a arquitetura chamam de gold o motor OFFLINE ('autotune'),
#      com Viterbi global — a referencia de qualidade.
#    * bench_stream.py chama de gold o motor CAUSAL ('autotune_rt') — e' contra
#      ELE que o invariante "correlacao >= 0,995" sempre foi medido.
#  Os dois numeros sao muito diferentes (ver o relatorio). Este script mede e
#  rotula os DOIS, de proposito, para que o texto do TCC nao confunda um com o
#  outro.
#
#  USO
#  ---
#    python3 python/medir_qualidade.py                    # compila e mede tudo
#    python3 python/medir_qualidade.py --bin ./bin        # usa binarios prontos
#    python3 python/medir_qualidade.py --wav outro.wav
#    python3 python/medir_qualidade.py --trabalho /tmp/x  # guarda os WAVs
#
#  Dependencias: numpy e soundfile (num venv qualquer; nao precisa do venv do
#  repositorio irmao).
# =============================================================================

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

import numpy as np
import soundfile as sf

# --- Preset "natural" do projeto ---------------------------------------------
# O 3o argumento posicional e' o MIX (0..1), nao mais a 'forca' (mudou na
# Etapa 2). 1.0 = efeito cheio. tol=15 cents de zona morta, glide=40 ms de
# portamento, look=4 quadros de look-ahead do Viterbi.
PRESET = ["1.0", "crom", "tol=15", "glide=40", "look=4"]

# Parametros de analise. Precisam ser os MESMOS nos tres caminhos, senao as
# trilhas de F0 nao sao comparaveis quadro a quadro.
FRAME = 1024
HOP = 256

BLOCOS = [64, 128, 256, 512]

# Limiar do "pipoco": uma descontinuidade amostra-a-amostra maior que 30x a
# mediana das descontinuidades. A mediana e' usada como escala porque e' imune
# aos proprios outliers que estamos contando (a media nao seria).
FATOR_PIPOCO = 30.0

# Os tres caminhos: (nome curto, fonte, executavel).
CAMINHOS = [
    ("autotune", "src/offline_causal/main.cpp", "autotune"),
    ("autotune_rt", "src/offline_causal/autotune_rt.cpp", "autotune_rt"),
    ("stream_test", "src/c1_streaming/stream_test.cpp", "stream_test"),
]


# -----------------------------------------------------------------------------
#  Infraestrutura: compilar e rodar
# -----------------------------------------------------------------------------
def compilar(raiz_fonte, dir_bin, cxx):
    """Compila os tres CLIs em 'dir_bin'. Mesmas flags do baseline.sh, para que
    os binarios daqui sejam os mesmos que o baseline confere."""
    os.makedirs(dir_bin, exist_ok=True)
    for nome, fonte, exe in CAMINHOS:
        destino = os.path.join(dir_bin, exe)
        cmd = [cxx, "-std=c++17", "-O2", "-I", os.path.join(raiz_fonte, "external"),
               os.path.join(raiz_fonte, fonte), "-o", destino]
        print(f"  compilando {nome} ...", flush=True)
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)
            sys.exit(f"ERRO: falhou ao compilar {fonte}")
    return dir_bin


def rodar(exe, wav_ent, wav_sai, extra):
    """Roda um dos CLIs e devolve (stdout, segundos de relogio de parede).

    O tempo de parede inclui leitura e gravacao do WAV, entao NAO e' o xRT
    do motor — o proprio autotune_rt imprime o xRT dele, cronometrado so em
    volta do DSP. Aqui ele serve para o stream_test, que nao imprime xRT
    nenhum; e' um limite SUPERIOR do custo, e esta rotulado como tal."""
    t0 = time.perf_counter()
    r = subprocess.run([exe, wav_ent, wav_sai] + PRESET + extra,
                       capture_output=True, text=True)
    dt = time.perf_counter() - t0
    if r.returncode != 0:
        print(r.stdout, r.stderr, file=sys.stderr)
        sys.exit(f"ERRO: {os.path.basename(exe)} devolveu {r.returncode}")
    return r.stdout, dt


def carregar(caminho):
    """Le um WAV como mono float64. O projeto so gera mono, mas a media entre
    canais deixa o script tolerante a uma entrada estereo."""
    a, fs = sf.read(caminho)
    if a.ndim > 1:
        a = a.mean(axis=1)
    return a.astype(float), fs


# -----------------------------------------------------------------------------
#  Metricas
# -----------------------------------------------------------------------------
def correlacao(a, b):
    """Correlacao de Pearson sobre o trecho comum. Devolve nan se um dos lados
    for constante (acontece em trechos de silencio digital)."""
    n = min(len(a), len(b))
    if n < 2:
        return float("nan")
    a, b = a[:n], b[:n]
    if np.std(a) == 0 or np.std(b) == 0:
        return float("nan")
    return float(np.corrcoef(a, b)[0, 1])


def correlacao_por_janela(a, b, fs, seg=0.5, piso_rms=0.02):
    """Correlacao media em janelas de 'seg' segundos, ignorando janelas quase
    silenciosas (RMS < piso_rms).

    Por que existe alem da global: a correlacao global e' uma unica media sobre
    o sinal inteiro, entao UM trecho alto e dessincronizado derruba o numero
    todo, mesmo que 90% do audio seja identico. A versao por janela mostra a
    distribuicao — e e' o metodo que o bench_latencia.py ja usava."""
    n = min(len(a), len(b))
    w = int(seg * fs)
    cs = []
    for i in range(0, n - w, w):
        ja = a[i:i + w]
        if np.sqrt(np.mean(ja ** 2)) < piso_rms:
            continue
        c = correlacao(ja, b[i:i + w])
        if np.isfinite(c):
            cs.append(c)
    return (float(np.mean(cs)) if cs else float("nan")), len(cs)


def mascara_vozeada(f0_por_quadro, n_amostras, frame=FRAME, hop=HOP):
    """Converte a trilha de F0 (um valor por quadro; 0 = nao-vozeado) numa
    mascara booleana por AMOSTRA.

    O quadro k cobre as amostras [k*hop, k*hop+frame). Quadros vozeados
    sobrepostos se unem naturalmente pelo OR."""
    m = np.zeros(n_amostras, dtype=bool)
    for k, f in enumerate(f0_por_quadro):
        if f > 0:
            ini = k * hop
            fim = min(ini + frame, n_amostras)
            if ini < n_amostras:
                m[ini:fim] = True
    return m


def contar_pipoco(x, fator=FATOR_PIPOCO):
    """Conta descontinuidades amostra-a-amostra maiores que 'fator' vezes a
    mediana. E' o detector de clique do projeto: o PSOLA, quando emenda dois
    graos com fase errada, produz um degrau que aparece como um |x[n]-x[n-1]|
    ordens de grandeza acima do tipico."""
    p = np.max(np.abs(x))
    if p <= 0:
        return 0
    x = x / p
    d = np.abs(np.diff(x))
    med = np.median(d) + 1e-12
    return int(np.sum(d > fator * med))


def ler_f0(caminho):
    with open(caminho) as f:
        return np.array([float(l) for l in f if l.strip()])


def extrair(regex, texto, padrao="?"):
    m = re.search(regex, texto)
    return m.group(1) if m else padrao


# -----------------------------------------------------------------------------
#  Programa principal
# -----------------------------------------------------------------------------
def main():
    aqui = os.path.dirname(os.path.abspath(__file__))
    raiz = os.path.dirname(aqui)

    ap = argparse.ArgumentParser(description="Linha de base de qualidade dos tres caminhos.")
    ap.add_argument("--wav", default=os.path.join(raiz, "exemplo-antes.wav"),
                    help="WAV de entrada (padrao: exemplo-antes.wav do repo)")
    ap.add_argument("--fonte", default=raiz,
                    help="raiz do codigo-fonte a compilar (padrao: a raiz do repo)")
    ap.add_argument("--bin", default=None,
                    help="diretorio com os binarios ja compilados; pula a compilacao")
    ap.add_argument("--trabalho", default=None,
                    help="onde gravar os WAVs intermediarios (padrao: diretorio temporario)")
    ap.add_argument("--cxx", default=os.environ.get("CXX") or ("g++" if shutil.which("g++") else "clang++"),
                    help="compilador C++ (padrao: g++ se existir, senao clang++)")
    args = ap.parse_args()

    if not os.path.isfile(args.wav):
        sys.exit(f"ERRO: nao achei o WAV de entrada '{args.wav}'")

    temporario = args.trabalho is None
    trab = tempfile.mkdtemp(prefix="medir_qualidade_") if temporario else args.trabalho
    os.makedirs(trab, exist_ok=True)

    print("=" * 78)
    print("  LINHA DE BASE DE QUALIDADE")
    print("=" * 78)
    print(f"entrada  : {args.wav}")
    print(f"preset   : {' '.join(PRESET)}   (3o posicional = MIX)")
    print(f"trabalho : {trab}")

    if args.bin:
        dir_bin = os.path.abspath(args.bin)
        print(f"binarios : {dir_bin} (pre-compilados)")
    else:
        dir_bin = os.path.join(trab, "bin")
        print(f"binarios : {dir_bin} (compilando com {args.cxx})")
        compilar(args.fonte, dir_bin, args.cxx)

    exe = {nome: os.path.join(dir_bin, ex) for nome, _, ex in CAMINHOS}
    for nome, caminho in exe.items():
        if not os.path.isfile(caminho):
            sys.exit(f"ERRO: nao achei o binario '{caminho}'")

    def s(nome):
        return os.path.join(trab, nome)

    x, fs = carregar(args.wav)
    dur = len(x) / fs

    # ---------------------------------------------------------------------
    #  1. Rodadas base
    #
    #  O 'autotune' (offline) nao aceita look= nem dumpf0= — ele ignora flags
    #  que nao conhece, entao passar o preset inteiro e' inofensivo, mas NAO
    #  da' para extrair a trilha de F0 dele. Por isso a comparacao de F0 e'
    #  streaming x autotune_rt (que aceita dumpf0=).
    # ---------------------------------------------------------------------
    print("\n-- rodando os tres caminhos --")
    out_off, t_off = rodar(exe["autotune"], args.wav, s("offline.wav"), [])
    out_rt, t_rt = rodar(exe["autotune_rt"], args.wav, s("rt.wav"),
                         [f"frame={FRAME}", f"hop={HOP}", "dumpf0=" + s("rt_f0.txt")])
    out_st, t_st = rodar(exe["stream_test"], args.wav, s("st_128.wav"),
                         [f"frame={FRAME}", f"hop={HOP}", "block=128",
                          "dumpf0=" + s("st_f0.txt")])

    off, _ = carregar(s("offline.wav"))
    rt, _ = carregar(s("rt.wav"))
    st, _ = carregar(s("st_128.wav"))

    # ---------------------------------------------------------------------
    #  2. Latencia e xRT
    #
    #  ATENCAO: as duas formulas de latencia do projeto DIVERGEM (esta na lista
    #  de armadilhas do CLAUDE.md). O autotune_rt orca 1 periodo de FMIN para o
    #  PSOLA e SOMA o bloco do callback; o autotune_stream.h orca 2 periodos e
    #  NAO soma o bloco. Os dois numeros aparecem abaixo, sem conciliacao — e'
    #  medicao, nao opiniao.
    # ---------------------------------------------------------------------
    lat_rt_ms = extrair(r"Latencia algoritmica:\s*([\d.]+)\s*ms", out_rt)
    xrt_rt = extrair(r"xRT\s*=\s*([\d.]+)", out_rt)
    lat_st_amostras = int(extrair(r"lat=(\d+) amostras", out_st, "0"))
    lat_st_ms = 1000.0 * lat_st_amostras / fs

    print("\n" + "=" * 78)
    print("  1. LATENCIA ALGORITMICA E CUSTO")
    print("=" * 78)
    print(f"  sinal de teste                   : {dur:.2f} s @ {fs} Hz")
    print(f"  autotune_rt  latencia reportada  : {lat_rt_ms} ms")
    print(f"  autotune_rt  xRT (o proprio C++) : {xrt_rt}")
    print(f"  stream_test  latencia reportada  : {lat_st_amostras} amostras = {lat_st_ms:.1f} ms")
    print(f"  stream_test  xRT (relogio de parede, INCLUI I/O de WAV -- limite superior)")
    print(f"                                   : {t_st / dur:.3f}")
    print(f"  autotune (offline) tempo total   : {t_off:.3f} s ({t_off / dur:.3f} x a duracao)")
    print("  nota: as duas formulas de latencia divergem por construcao;")
    print("        autotune_rt = frame + look*hop + 1*fs/FMIN + bloco;")
    print("        autotune_stream.h = frame + look*hop + 2*fs/FMIN (sem bloco).")

    # ---------------------------------------------------------------------
    #  3. Correlacao entre os caminhos
    #
    #  A saida do streaming e' o sinal corrigido ATRASADO da latencia
    #  algoritmica; as outras duas nao tem esse atraso. Para comparar
    #  amostra-a-amostra e' obrigatorio remover o atraso antes.
    # ---------------------------------------------------------------------
    st_al = st[lat_st_amostras:]

    f0_rt = ler_f0(s("rt_f0.txt"))
    f0_st = ler_f0(s("st_f0.txt"))
    # Mascara de vozeamento tirada da trilha do autotune_rt: e' a unica trilha
    # de F0 que da' para extrair com os mesmos parametros de analise dos tres
    # caminhos (o offline nao tem dumpf0=).
    voz = mascara_vozeada(f0_rt, min(len(off), len(rt), len(st_al)))

    def par(nome, a, b):
        g = correlacao(a, b)
        # A correlacao "vozeada" so pode ir ate onde a mascara existe: ela foi
        # construida sobre o trecho comum aos TRES caminhos, que e' mais curto
        # que o comum a um par (o streaming perde o comprimento da latencia).
        n = min(len(a), len(b), len(voz))
        mv = voz[:n]
        v = correlacao(a[:n][mv], b[:n][mv]) if mv.any() else float("nan")
        jm, jn = correlacao_por_janela(a, b, fs)
        print(f"  {nome:<44} global={g:7.4f}  vozeada={v:7.4f}  janelas(0,5s)={jm:7.4f} (n={jn})")
        return g, v, jm

    print("\n" + "=" * 78)
    print("  2. CORRELACAO ENTRE OS CAMINHOS")
    print("=" * 78)
    print(f"  (streaming alinhado removendo {lat_st_amostras} amostras de latencia)")
    par("stream_test  x  autotune_rt   (causal)", rt, st_al)
    par("stream_test  x  autotune      (offline/GOLD)", off, st_al)
    par("autotune_rt  x  autotune      (offline/GOLD)", off, rt)

    # ---------------------------------------------------------------------
    #  2b. ONDE o causal diverge do offline
    #
    #  A correlacao global do causal com o offline e' baixa, e sozinha ela
    #  engana: sugere que os dois motores discordam o tempo todo. Nao e' o
    #  caso. Esta varredura mostra janela a janela quanto eles batem, e quanto
    #  passariam a bater se fosse permitido deslizar uma janela no tempo. Se um
    #  deslize pequeno recupera a correlacao, a divergencia e' de FASE (os graos
    #  do PSOLA foram colados em posicoes diferentes); se nem deslizando
    #  recupera, os dois motores decidiram alvos DIFERENTES — que e' o que
    #  acontece nos ataques, onde o Viterbi global ja "sabe" para onde a nota
    #  vai e o causal ainda nao.
    # ---------------------------------------------------------------------
    print("\n  detalhe janela a janela (autotune_rt x autotune, janelas de 0,25 s):")
    print(f"  {'t (s)':>7} | {'RMS':>6} | {'corr':>7} | {'melhor corr':>11} | {'desliz.':>8}")
    w = int(0.25 * fs)
    margem = 700  # ate onde a busca de deslize pode ir, em amostras
    n = min(len(off), len(rt))
    for i in range(margem, n - w - margem, w):
        ja = off[i:i + w]
        rms = float(np.sqrt(np.mean(ja ** 2)))
        if rms < 0.02:
            continue
        c0 = correlacao(ja, rt[i:i + w])
        melhor = max(((L, correlacao(ja, rt[i + L:i + L + w]))
                      for L in range(-margem, margem + 1, 1)), key=lambda t: t[1])
        marca = "" if c0 > 0.99 else "   <-- diverge"
        print(f"  {i / fs:7.2f} | {rms:6.3f} | {c0:+7.3f} | {melhor[1]:+11.3f} | "
              f"{melhor[0]:+8d}{marca}")

    # ---------------------------------------------------------------------
    #  4. Trilha de F0
    #
    #  Os primeiros quadros sao descartados: o Viterbi de lag fixo so tem
    #  decisao estavel depois de encher a janela de look-ahead, e comparar a
    #  borda so mede o transitorio de partida.
    # ---------------------------------------------------------------------
    look = int(extrair(r"look=(\d+)", " ".join(PRESET), "4"))
    ini = look + 2
    m = min(len(f0_rt), len(f0_st))
    a, b = f0_rt[:m], f0_st[:m]
    dif = np.abs(a[ini:] - b[ini:])
    ambos_voz = (a[ini:] > 0) & (b[ini:] > 0)
    conc_1hz = float(np.mean(dif[ambos_voz] < 1.0)) if ambos_voz.any() else 0.0
    conc_voz = float(np.mean((a[ini:] > 0) == (b[ini:] > 0)))

    print("\n" + "=" * 78)
    print("  3. TRILHA DE F0 (streaming x autotune_rt)")
    print("=" * 78)
    print(f"  quadros: autotune_rt={len(f0_rt)}  stream_test={len(f0_st)}")
    print(f"  concordancia da DECISAO de vozeamento : {100 * conc_voz:.2f}%")
    print(f"  concordancia de F0 (< 1 Hz, ambos vozeados) : {100 * conc_1hz:.2f}%  "
          f"(sobre {int(ambos_voz.sum())} quadros)")
    if ambos_voz.any():
        print(f"  erro mediano de F0 nos vozeados : {np.median(dif[ambos_voz]):.4f} Hz")
        print(f"  erro maximo  de F0 nos vozeados : {np.max(dif[ambos_voz]):.4f} Hz")
    print("  nota: o 'autotune' offline NAO aceita dumpf0=, entao nao ha' trilha")
    print("        de F0 do gold para comparar. A referencia aqui e' o causal.")

    # ---------------------------------------------------------------------
    #  5. Pipoco
    # ---------------------------------------------------------------------
    print("\n" + "=" * 78)
    print(f"  4. PIPOCO (saltos > {FATOR_PIPOCO:.0f}x a mediana)")
    print("=" * 78)
    p_ent = contar_pipoco(x)
    for rotulo, sinal in [("entrada (nao processada)", x),
                          ("autotune    (offline/GOLD)", off),
                          ("autotune_rt (causal)", rt),
                          ("stream_test (block=128)", st)]:
        c = contar_pipoco(sinal)
        delta = "" if sinal is x else f"   ({c - p_ent:+d} vs. a entrada)"
        print(f"  {rotulo:<28}: {c:5d}{delta}")
    print("  LEIA ISTO antes de citar o numero: a ENTRADA nao processada tambem")
    print(f"  pontua ({p_ent}). Numa voz real a 44,1 kHz a mediana de |x[n]-x[n-1]| e'")
    print("  da ordem de 2e-3 e o pico legitimo passa de 0,27 — ou seja, '30x a")
    print("  mediana' cai por volta do percentil 97 do sinal NORMAL, e o detector")
    print("  conta conteudo de alta frequencia, nao clique. O numero absoluto so")
    print("  faz sentido COMPARADO ao da entrada: se o processamento nao acrescenta")
    print("  eventos, nao introduziu descontinuidade. Um alvo de 'zero absoluto' nao")
    print("  e' atingivel com este detector neste material.")

    # ---------------------------------------------------------------------
    #  6. Invariancia ao tamanho de bloco
    #
    #  O host de audio escolhe o tamanho do bloco; o plugin nao. Se a saida
    #  mudar com o bloco, o resultado depende da configuracao do usuario — o
    #  que inviabiliza qualquer teste de regressao. Por isso o invariante e'
    #  IGUALDADE EXATA, nao "parecido".
    # ---------------------------------------------------------------------
    print("\n" + "=" * 78)
    print("  5. INVARIANCIA AO TAMANHO DE BLOCO")
    print("=" * 78)
    ref = None
    for B in BLOCOS:
        saida = s(f"st_b{B}.wav")
        rodar(exe["stream_test"], args.wav, saida,
              [f"frame={FRAME}", f"hop={HOP}", f"block={B}"])
        y, _ = carregar(saida)
        if ref is None:
            ref = y
            print(f"  block={B:4d}: referencia ({len(y)} amostras)")
            continue
        n = min(len(ref), len(y))
        difere = int(np.sum(ref[:n] != y[:n])) + abs(len(ref) - len(y))
        maxd = float(np.max(np.abs(ref[:n] - y[:n]))) if n else 0.0
        veredicto = "IDENTICO" if difere == 0 else "DIVERGE"
        print(f"  block={B:4d}: {difere} amostras diferentes do block=64 "
              f"| maior diferenca = {maxd:.3e}   {veredicto}")
    print("  o invariante do projeto pede IGUALDADE EXATA aqui; qualquer linha")
    print("  marcada DIVERGE e' uma quebra dele, nao um arredondamento.")

    print("\n" + "=" * 78)
    if temporario:
        print(f"  (os WAVs intermediarios ficaram em {trab} — use --trabalho para fixar)")
    print("  FIM")
    print("=" * 78)


if __name__ == "__main__":
    main()
