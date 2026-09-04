import subprocess, soundfile as sf, numpy as np
ENT="audioteste.wav"; ARGS=["1.0","crom","tol=15","glide=40","look=4","voz=contralto"]
# A saida do nucleo de STREAMING (Tarefa 5) eh o sinal corrigido ATRASADO da
# latencia algoritmica 'lat'. O gold (autotune_rt) NAO tem esse atraso. Para
# comparar amostra-a-amostra, alinhamos: deslocamos a saida do streaming para
# a esquerda de 'lat' antes de correlacionar (compara g[:m] vs s[lat:lat+m]).
# lat = frame + look*hop + GUARDA_PSOLA, onde a guarda do PSOLA online é de
# 2 períodos do tom mais grave (2*round(fs/fmin)) — necessária para que o grão
# da última marca da janela (largura estimada pela marca anterior) nunca
# alcance a região já finalizada. fmin=175 (contralto):
#       1024 + 4*256 + 2*round(44100/175) = 1024 + 1024 + 504 = 2552
LAT = 2552
def run(exe,outp,extra): subprocess.run([exe,ENT,outp]+ARGS+extra, capture_output=True)
def load(p): a,fs=sf.read(p); a=a.mean(axis=1) if a.ndim>1 else a; return a.astype(float),fs
run("./autotune_rt.exe","_gold.wav",[]); g,fs=load("_gold.wav")
print(f"lag usado p/ alinhar streaming->gold = {LAT} amostras")
ok=True
for B in (64,128,256,512):
    run("./stream_test.exe",f"_st_{B}.wav",[f"block={B}"]); s,_=load(f"_st_{B}.wav")
    s=s[LAT:]                                   # remove o atraso de latencia
    m=min(len(g),len(s)); cc=np.corrcoef(g[:m],s[:m])[0,1]
    print(f"block={B:4d}: corr c/ gold = {cc:.4f}")
    # Limiar 0.995: o drift de fase do PSOLA online foi ELIMINADO (espaçamento
    # de grãos invariante a truncamento). O resíduo (~0.997) é um "jitter" de
    # fase de pouquíssimas amostras (<3 ~ 0.07 ms) que varia de nota a nota,
    # inerente à re-síntese em janela ancorada por região vozeada — por isso a
    # correlação global fica ~0.997 enquanto a correlação POR REGIÃO é >0.999.
    # (Com tol=600 o alvo coincide com o F0 -> beta=1 e a saída é IDÊNTICA à
    #  entrada; era esse o papel do antigo forca=0. Ver teste_fase.py e
    #  docs/execucao-do-plano.md, Etapa 2.)
    ok &= (cc>0.995)
print("RESULTADO:", "PASS" if ok else "DIVERGIU")
