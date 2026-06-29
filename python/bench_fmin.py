import subprocess, re, sys, numpy as np, soundfile as sf

# Experimento: subir FMIN baixa o PISO de latencia (o termo PSOLA = fs/FMIN, periodo
# mais longo), mas para de corrigir notas abaixo de FMIN. Aqui medimos isso PARA A VOZ
# DA GRAVACAO: descobrimos a nota mais grave realmente cantada (via dumpf0, reaproveitando
# o detector real) e varremos FMIN nos presets de tessitura PADRAO (Fach/SATB, iguais ao
# "Vocal Range" do Auto-Tune), tabulando latencia x % de notas perdidas x qualidade.
#
# Uso: python bench_fmin.py [entrada.wav] [forca] [escala] [tol] [glide]

ENTRADA = sys.argv[1] if len(sys.argv) > 1 else "audioteste.wav"
FORCA   = sys.argv[2] if len(sys.argv) > 2 else "1.0"
ESCALA  = sys.argv[3] if len(sys.argv) > 3 else "crom"
TOL     = sys.argv[4] if len(sys.argv) > 4 else "15"
GLIDE   = sys.argv[5] if len(sys.argv) > 5 else "40"

# presets de tessitura padrao (nome, FMIN, FMAX) -- floors em notas reais
PRESETS = [
    ("baixo",     82,  330),   # E2-E4
    ("baritono",  98,  392),   # G2-G4
    ("tenor",    131,  523),   # C3-C5
    ("contralto",175,  698),   # F3-F5
    ("mezzo",    220,  880),   # A3-A5
    ("soprano",  262, 1047),   # C4-C6
]

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

def nota(f):
    if f <= 0: return "-"
    nomes = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
    m = int(round(69 + 12*np.log2(f/440.0)))
    return f"{nomes[m%12]}{m//12 - 1}"

# 1. F0 verdadeiro da voz (FMIN baixo p/ nao perder grave) -> conteudo de pitch real
subprocess.run(["./autotune_rt.exe", ENTRADA, "_null.wav", "0", ESCALA,
                "fmin=70", "look=0", "dumpf0=_f0.txt"], capture_output=True)
f0 = np.array([float(l) for l in open("_f0.txt")])
voz = f0[f0 > 0]
fmin_voz = np.percentile(voz, 2)     # nota mais grave robusta (p2)
print(f"\nVoz '{ENTRADA}': {len(voz)} quadros vozeados | "
      f"grave~{fmin_voz:.0f} Hz ({nota(fmin_voz)}) | "
      f"mediana {np.median(voz):.0f} Hz ({nota(np.median(voz))}) | "
      f"agudo~{np.percentile(voz,98):.0f} Hz ({nota(np.percentile(voz,98))})")

# 2. referencia OFFLINE (gold; FMIN=80 fixo no main.cpp)
subprocess.run(["./autotune.exe", ENTRADA, "_off_ref.wav", FORCA, ESCALA,
                f"tol={TOL}", f"glide={GLIDE}"], capture_output=True)
ref, fs = load("_off_ref.wav")

print(f"\nforca={FORCA} | look=0 (piso) | % perdidas = quadros vozeados abaixo de FMIN")
print(f"{'preset':>10} | {'FMIN':>5} | {'latencia':>9} | {'lat.PSOLA':>9} | "
      f"{'% perdidas':>10} | {'xRT':>6} | {'sim.offline':>11} | {'pipoco':>6}")
print("-" * 92)
for nome, fm, fx in PRESETS:
    out = subprocess.run(["./autotune_rt.exe", ENTRADA, f"_fm_{fm}.wav", FORCA, ESCALA,
                          f"tol={TOL}", f"glide={GLIDE}", "look=0", f"voz={nome}"],
                         capture_output=True, text=True).stdout
    lat   = re.search(r"Latencia algoritmica:\s*([\d.]+)\s*ms", out)
    psola = re.search(r"psola\s*([\d.]+)", out)
    xrt   = re.search(r"xRT\s*=\s*([\d.]+)", out)
    b, _  = load(f"_fm_{fm}.wav")
    perdidas = 100.0 * np.mean(voz < fm)
    lat   = lat.group(1)   if lat   else "?"
    psola = psola.group(1) if psola else "?"
    xrt   = xrt.group(1)   if xrt   else "?"
    print(f"{nome:>10} | {fm:>5} | {lat:>6} ms | {psola:>6} ms | "
          f"{perdidas:>9.1f}% | {xrt:>6} | {corr_com(ref, b, fs):>11.3f} | {spikes(b):>6}")

print("\nlat.PSOLA = fs/FMIN (termo que domina o piso). Sobe FMIN -> cai a latencia,")
print("mas '% perdidas' sobe quando FMIN passa da nota mais grave cantada (vira 'sem nota').")
print("Preset ideal = o de maior FMIN com % perdidas ~0 (cobre o grave da voz).")
