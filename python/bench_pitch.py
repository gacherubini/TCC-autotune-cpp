# ============================================================================
#  bench_pitch.py — CAMINHO C1, TAREFA 3: valida a trilha de F0 do streaming
#
#  Roda autotune_rt (gold, Viterbi de lag fixo OFFLINE) e stream_test (Viterbi
#  de lag fixo em STREAMING, com janela deslizante de psi) sobre o mesmo
#  áudio, com os mesmos parâmetros (look/frame/hop), e compara as trilhas de
#  F0 quadro a quadro. Esperado: coincidência > 99% (alguns quadros de borda
#  podem diferir).
# ============================================================================
import subprocess
import numpy as np

ENT = "audioteste.wav"
LOOK = "4"; FR = "1024"; HOP = "256"

# --- gold: autotune_rt (offline, Viterbi de lag fixo) ---
subprocess.run(["./autotune_rt.exe", ENT, "_g.wav", "1.0", "crom",
                 f"look={LOOK}", f"frame={FR}", f"hop={HOP}", "dumpf0=_g_f0.txt"], capture_output=True)
g = np.array([float(l) for l in open("_g_f0.txt")])

# --- streaming: stream_test (causal, janela deslizante) ---
subprocess.run(["./stream_test.exe", ENT, "_s.wav", "1.0", "crom",
                 f"look={LOOK}", f"frame={FR}", f"hop={HOP}", "block=128", "dumpf0=_s_f0.txt"], capture_output=True)
s = np.array([float(l) for l in open("_s_f0.txt")])

m = min(len(g), len(s))
g2, s2 = g[:m], s[:m]

ig = max(int(LOOK) + 2, 0)
dif = np.abs(g2[ig:] - s2[ig:])
voiced = (g2[ig:] > 0) & (s2[ig:] > 0)
match = np.mean(dif[voiced] < 1.0) if voiced.any() else 0

print(f"quadros gold={len(g)} stream={len(s)} | coincidencia F0 (<1Hz): {100*match:.1f}%")
print("RESULTADO:", "PASS" if match > 0.99 else "VER DIFERENCAS")
