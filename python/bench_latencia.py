import subprocess, re, sys, numpy as np, soundfile as sf

# Benchmark do motor CAUSAL (autotune_rt.exe): varre o look-ahead e tabula
# LATENCIA x QUALIDADE x xRT. Qualidade = (a) semelhanca com a saida OFFLINE
# (tida como "ouro"), (b) pipoco (saltos grandes). O proprio C++ imprime
# latencia e xRT; este script so chama o exe e organiza a tabela.
#
# Uso: python bench_latencia.py [entrada.wav] [forca] [escala] [tol] [glide]

ENTRADA = sys.argv[1] if len(sys.argv) > 1 else "audioteste.wav"
FORCA   = sys.argv[2] if len(sys.argv) > 2 else "1.0"
ESCALA  = sys.argv[3] if len(sys.argv) > 3 else "crom"
TOL     = sys.argv[4] if len(sys.argv) > 4 else "15"
GLIDE   = sys.argv[5] if len(sys.argv) > 5 else "40"
LOOKS   = [0, 1, 2, 4, 8, 16, 32]

def load(p):
    x, fs = sf.read(p)
    if x.ndim > 1: x = x.mean(axis=1)
    return x.astype(float), fs

def corr_com(ref, b, fs):
    n = min(len(ref), len(b)); a, c = ref[:n], b[:n]
    W = int(0.5 * fs); cs = []
    for i in range(0, n - W, W):
        sa = a[i:i+W]
        if np.sqrt(np.mean(sa**2)) < 0.02: continue
        cc = np.corrcoef(sa, c[i:i+W])[0, 1]
        if np.isfinite(cc): cs.append(cc)
    return float(np.mean(cs)) if cs else float("nan")

def spikes(b):
    b = b / (np.max(np.abs(b)) + 1e-12)
    d = np.abs(np.diff(b)); med = np.median(d) + 1e-12
    return int(np.sum(d > 30 * med))

# referencia OFFLINE
subprocess.run(["./autotune.exe", ENTRADA, "_off_ref.wav", FORCA, ESCALA,
                f"tol={TOL}", f"glide={GLIDE}"], capture_output=True)
ref, fs = load("_off_ref.wav")

print(f"\nEntrada: {ENTRADA} | forca={FORCA} escala={ESCALA} tol={TOL} glide={GLIDE}")
print(f"{'look':>4} | {'latencia':>9} | {'xRT':>6} | {'sim.offline':>11} | {'pipoco':>6}")
print("-" * 52)
for look in LOOKS:
    out = subprocess.run(["./autotune_rt.exe", ENTRADA, f"_rt_l{look}.wav", FORCA, ESCALA,
                          f"tol={TOL}", f"glide={GLIDE}", f"look={look}"],
                         capture_output=True, text=True).stdout
    lat = re.search(r"Latencia algoritmica:\s*([\d.]+)\s*ms", out)
    xrt = re.search(r"xRT\s*=\s*([\d.]+)", out)
    b, _ = load(f"_rt_l{look}.wav")
    lat = lat.group(1) if lat else "?"
    xrt = xrt.group(1) if xrt else "?"
    print(f"{look:>4} | {lat:>6} ms | {xrt:>6} | {corr_com(ref, b, fs):>11.3f} | {spikes(b):>6}")

print("\nNota: 'sim.offline' = correlacao media com a saida offline (1.0 = identica ao 'ouro').")
print("look=0 e guloso (menor latencia, menor qualidade); look maior aproxima do offline.")
