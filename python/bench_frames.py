import subprocess, sys, soundfile as sf, numpy as np
ENT = "audioteste.wav"; FRAME, HOP = 1024, 256
x, fs = sf.read(ENT);  x = x.mean(axis=1) if x.ndim>1 else x; N = len(x)
# índices teóricos esperados: 0, HOP, 2*HOP, ... enquanto frame couber
esp = list(range(0, N-FRAME+1, HOP))
ok_all = True
for B in (64, 128, 256, 512):
    subprocess.run(["./stream_test.exe", ENT, "_s.wav", "0", "crom",
                    f"frame={FRAME}", f"hop={HOP}", f"block={B}", "dumpframes=_fr.txt"],
                   capture_output=True)
    got = [int(l) for l in open("_fr.txt")]
    ok = (got == esp)
    print(f"block={B:4d}: {len(got)} quadros, bate com fatiamento direto? {ok}")
    ok_all &= ok
print("RESULTADO:", "PASS" if ok_all else "FALHOU")
