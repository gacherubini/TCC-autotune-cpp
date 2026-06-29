import sys, numpy as np, soundfile as sf

# Verifica se o TD-PSOLA PRESERVA OS FORMANTES (envelope espectral) ao corrigir o
# pitch. Compara, num mesmo trecho vozeado sustentado, o envelope cepstral de
# entrada vs saida. Espera-se: a estrutura fina (harmonicos) se desloca junto com
# o pitch, mas os PICOS DO ENVELOPE (formantes) ficam nas mesmas frequencias.
#
# Uso: python formantes.py entrada.wav saida.wav [tempo_seg]

def load(p):
    x, fs = sf.read(p)
    if x.ndim > 1: x = x.mean(axis=1)
    return x.astype(float), fs

def envelope(seg, fs, NFFT=8192, L=40):
    win = np.hanning(len(seg))
    X = np.fft.fft(seg * win, n=NFFT)
    logmag = np.log(np.abs(X) + 1e-9)
    c = np.fft.ifft(logmag).real            # cepstro real
    lift = np.zeros(NFFT); lift[:L] = 1; lift[-L + 1:] = 1
    env = np.exp(np.fft.fft(c * lift).real) # envelope suave (formantes)
    half = NFFT // 2
    freqs = np.arange(half) * fs / NFFT
    return freqs, env[:half], np.abs(X)[:half]

def picos(freqs, env, fmax=4000):
    out = []
    for i in range(2, len(env) - 2):
        if freqs[i] > fmax: break
        if env[i] > env[i-1] and env[i] >= env[i+1] and env[i] > env[i-2] and env[i] >= env[i+2]:
            out.append(freqs[i])
    return out

a, fa = load(sys.argv[1])
b, fb = load(sys.argv[2])

# escolhe o trecho mais forte da entrada (voz sustentada)
if len(sys.argv) > 3:
    c = int(float(sys.argv[3]) * fa)
else:
    ene = np.convolve(a**2, np.ones(2048)/2048, mode="same")
    c = int(np.argmax(ene))
W = int(0.06 * fa)                  # ~60 ms
i0 = max(0, c - W//2); i1 = i0 + W
sa = a[i0:i1]; sb = b[i0:i1]

fr, ea, ma = envelope(sa, fa)
_,  eb, mb = envelope(sb, fb)
pa = picos(fr, ea); pb = picos(fr, eb)

# pitch (1o pico do espectro fino) so pra mostrar que o pitch MUDOU
def f0(freqs, mag):
    lo = np.argmax(freqs > 70); hi = np.argmax(freqs > 500)
    k = lo + int(np.argmax(mag[lo:hi]))
    return freqs[k]

print(f"Trecho ~t={c/fa:.2f}s ({W/fa*1000:.0f} ms)")
print(f"  Pitch aprox: entrada {f0(fr,ma):.1f} Hz  ->  saida {f0(fr,mb):.1f} Hz   "
      f"({'mudou' if abs(f0(fr,ma)-f0(fr,mb))>3 else 'igual'})")
print(f"  Formantes entrada (Hz): {[f'{x:.0f}' for x in pa[:4]]}")
print(f"  Formantes saida   (Hz): {[f'{x:.0f}' for x in pb[:4]]}")
n = min(len(pa), len(pb))
if n:
    desv = np.abs(np.array(pa[:n]) - np.array(pb[:n]))
    print(f"  Desvio medio dos formantes: {desv.mean():.0f} Hz  (max {desv.max():.0f} Hz)")
    print("  -> formantes PRESERVADOS" if desv.mean() < 80 else "  -> formantes DESLOCADOS (atencao)")
