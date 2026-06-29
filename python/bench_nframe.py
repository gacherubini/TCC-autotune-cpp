import subprocess, re, sys, numpy as np, soundfile as sf

# Experimento: reduzir N_FRAME baixa o PISO de latencia, mas sobe a frequencia
# minima detectavel (janela do YIN = frame/2) e degrada a qualidade nos graves.
# Varre frame= com look=0 (latencia minima) e tabula piso x menor-freq x qualidade.
#
# Uso: python bench_nframe.py [entrada.wav] [forca] [escala] [tol] [glide]

ENTRADA = sys.argv[1] if len(sys.argv) > 1 else "audioteste.wav"
FORCA   = sys.argv[2] if len(sys.argv) > 2 else "1.0"
ESCALA  = sys.argv[3] if len(sys.argv) > 3 else "crom"
TOL     = sys.argv[4] if len(sys.argv) > 4 else "15"
GLIDE   = sys.argv[5] if len(sys.argv) > 5 else "40"
FRAMES  = [1024, 768, 512, 384, 256, 128]

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

# referencia OFFLINE (N_FRAME=1024 fixo no main.cpp)
subprocess.run(["./autotune.exe", ENTRADA, "_off_ref.wav", FORCA, ESCALA,
                f"tol={TOL}", f"glide={GLIDE}"], capture_output=True)
ref, fs = load("_off_ref.wav")

print(f"\nEntrada: {ENTRADA} | forca={FORCA} | look=0 (piso de latencia)")
print(f"{'frame':>5} | {'latencia':>9} | {'fmin det.':>9} | {'xRT':>6} | {'sim.offline':>11} | {'pipoco':>6}")
print("-" * 64)
for fr in FRAMES:
    out = subprocess.run(["./autotune_rt.exe", ENTRADA, f"_nf_{fr}.wav", FORCA, ESCALA,
                          f"tol={TOL}", f"glide={GLIDE}", "look=0", f"frame={fr}"],
                         capture_output=True, text=True).stdout
    lat  = re.search(r"Latencia algoritmica:\s*([\d.]+)\s*ms", out)
    fmin = re.search(r"a partir de ~([\d.]+)\s*Hz", out)
    xrt  = re.search(r"xRT\s*=\s*([\d.]+)", out)
    b, _ = load(f"_nf_{fr}.wav")
    lat  = lat.group(1)  if lat  else "?"
    fmin = fmin.group(1) if fmin else "?"
    xrt  = xrt.group(1)  if xrt  else "?"
    print(f"{fr:>5} | {lat:>6} ms | {fmin:>6} Hz | {xrt:>6} | {corr_com(ref, b, fs):>11.3f} | {spikes(b):>6}")

print("\nfmin det. = menor frequencia detectavel (= fs/(frame/2)).")
print("Piso de latencia cai com frame menor, mas a qualidade despenca quando fmin")
print("sobe acima das notas cantadas (graves viram 'sem nota' -> sem correcao).")
