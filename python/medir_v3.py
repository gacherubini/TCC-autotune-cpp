#!/usr/bin/env python3
# =============================================================================
#  medir_v3.py — compara o TD-PSOLA com o motor de ponteiro movel (v3).
#
#  Responde, com numero, as perguntas da spec (docs/especificacao-v3-ponteiro.md
#  §7): a nota de saida e' a mesma nos dois motores? Quanto e' o erro de ataque
#  do ponteiro (o beta chega 'defasagem' amostras atrasado)? Quanto e' a
#  latencia variavel? Algum motor introduz degraus (cliques)?
#
#  COMO MEDE O ERRO DE AFINACAO DA SAIDA
#  --------------------------------------
#  O alvo por amostra (fout) vem do proprio stream_test (dumpbeta=), decimado
#  por hop. O F0 da SAIDA e' medido com o autotune_rt (dumpf0=, look=8) sobre o
#  WAV gerado. As duas trilhas sao alinhadas pela latencia declarada de cada
#  motor. O erro e' |1200*log2(f0_saida / fout)| em cents, por quadro vozeado.
#    * "estavel": quadros a partir de 50 ms depois do inicio de cada regiao
#       vozeada -- mede se a nota certa saiu;
#    * "ataque": quadros nos primeiros 30 ms de cada regiao -- mede o preco da
#       defasagem da correcao no ponteiro.
#  O detector tem quadro de 1024 (23 ms), entao a janela de ataque e' GROSSA:
#  os numeros de ataque sao comparaveis ENTRE motores, nao absolutos.
#
#  USO:  .venv/bin/python python/medir_v3.py [--wav exemplo-antes.wav]
# =============================================================================
import argparse, os, re, subprocess, sys, tempfile
import numpy as np
import soundfile as sf

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESET = ["1.0", "crom", "tol=15", "retune=25"]
HOP = 256
CONFIGS = [  # (nome, flags extras do stream_test)
    ("PSOLA, look=4",       ["look=4"]),
    ("Ponteiro, look=4",    ["motor=ponteiro", "look=4"]),
    ("Low Latency (look=0)", ["lowlat=1"]),
]

def compilar(dir_bin):
    cxx = "clang++" if subprocess.run(["which", "clang++"], capture_output=True).returncode == 0 else "g++"
    exes = {}
    for fonte, exe in [("src/c1_streaming/stream_test.cpp", "stream_test"),
                       ("src/offline_causal/autotune_rt.cpp", "autotune_rt")]:
        dst = os.path.join(dir_bin, exe)
        r = subprocess.run([cxx, "-std=c++17", "-O2", "-I", os.path.join(RAIZ, "external"),
                            os.path.join(RAIZ, fonte), "-o", dst], capture_output=True, text=True)
        if r.returncode: sys.exit(r.stderr)
        exes[exe] = dst
    return exes

def rodar(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: sys.exit(r.stdout + r.stderr)
    return r.stdout

def cents(a, b): return 1200.0 * np.log2(a / b)

def regioes(vozeado):
    """Lista de (ini, fim) de quadros vozeados consecutivos."""
    out, ini = [], None
    for i, v in enumerate(vozeado):
        if v and ini is None: ini = i
        if not v and ini is not None: out.append((ini, i)); ini = None
    if ini is not None: out.append((ini, len(vozeado)))
    return out

def medir(nome, flags, exes, wav, tmp):
    saida = os.path.join(tmp, "saida.wav"); beta = os.path.join(tmp, "beta.txt"); f0s = os.path.join(tmp, "f0.txt")
    log = rodar([exes["stream_test"], wav, saida] + PRESET + flags + [f"dumpbeta={beta}"])
    lat = int(re.search(r"lat=(\d+)", log).group(1))
    fs = int(re.search(r"fs=(\d+)", log).group(1))
    dist = re.search(r"dist media=([\d.]+).*dist max=([\d.]+)", log)
    rodar([exes["autotune_rt"], saida, os.path.join(tmp, "lixo.wav"), "1.0", "crom", "tol=600",
           "look=8", f"hop={HOP}", f"dumpf0={f0s}"])
    alvo = np.loadtxt(beta)           # colunas: f0_entrada fout, uma linha por hop
    f0_out = np.loadtxt(f0s)          # um F0 por quadro da saida (hop=HOP)
    # alinhamento: quadro k da saida cobre a entrada deslocada de -lat amostras
    desl = int(round(lat / HOP))
    n = min(len(f0_out) - desl, len(alvo))
    fo = alvo[:n, 1]; fi = alvo[:n, 0]; fs_out = f0_out[desl:desl + n]
    voz = (fi > 0) & (fo > 0) & (fs_out > 0)
    err = np.full(n, np.nan); err[voz] = np.abs(cents(fs_out[voz], fo[voz]))
    est, atq = [], []
    q50, q30 = int(0.050 * fs / HOP), int(0.030 * fs / HOP)
    for ini, fim in regioes(fi > 0):
        est += [e for e in err[ini + q50:fim] if not np.isnan(e)]
        atq += [e for e in err[ini:min(fim, ini + q30)] if not np.isnan(e)]
    y, _ = sf.read(saida); y = y if y.ndim == 1 else y.mean(axis=1)
    degraus = int(np.sum(np.abs(np.diff(y)) > 0.25))
    return dict(nome=nome, lat_ms=1000 * lat / fs,
                dist_med=float(dist.group(1)) * 1000 / fs if dist else 0.0,
                dist_max=float(dist.group(2)) * 1000 / fs if dist else 0.0,
                est_med=np.median(est) if est else np.nan, est_p95=np.percentile(est, 95) if est else np.nan,
                atq_med=np.median(atq) if atq else np.nan, degraus=degraus)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--wav", default=os.path.join(RAIZ, "exemplo-antes.wav"))
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        exes = compilar(tmp)
        x, _ = sf.read(a.wav); x = x if x.ndim == 1 else x.mean(axis=1)
        print(f"degraus |d| > 0,25 na ENTRADA: {int(np.sum(np.abs(np.diff(x)) > 0.25))}\n")
        print("| Motor | Latência fixa | dist média | dist máx | erro estável (med / p95, ct) | erro de ataque (med, ct) | degraus |")
        print("|---|---:|---:|---:|---:|---:|---:|")
        for nome, flags in CONFIGS:
            r = medir(nome, flags, exes, a.wav, tmp)
            print(f"| {r['nome']} | {r['lat_ms']:.2f} ms | {r['dist_med']:.2f} ms | {r['dist_max']:.2f} ms | "
                  f"{r['est_med']:.1f} / {r['est_p95']:.1f} | {r['atq_med']:.1f} | {r['degraus']} |")

if __name__ == "__main__":
    main()
