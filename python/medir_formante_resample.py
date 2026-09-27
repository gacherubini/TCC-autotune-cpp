#!/usr/bin/env python3
# =============================================================================
#  medir_formante_resample.py — o v3 do modo de baixa latencia e' viavel?
#
#  POR QUE ESTE SCRIPT EXISTE
#  --------------------------
#  A pesquisa de 2026-08-27 (docs/pesquisa-latencia-antares.md) mostrou que o
#  Auto-Tune declara 37 amostras FIXAS de latencia porque nao faz analise-e-
#  ressintese: ele corrige altura com um ponteiro de leitura movel sobre um
#  buffer circular. O audio nunca espera pela deteccao.
#
#  Adotar essa arquitetura aqui (o "v3") derrubaria a latencia de ~71 ms para
#  ~1 ms. O preco e' bem definido: REAMOSTRAR DESLOCA O ESPECTRO INTEIRO, e
#  portanto move os formantes junto com a altura. O TD-PSOLA, que e' o motor
#  de hoje, os mantem parados — e essa preservacao e' a justificativa declarada
#  da escolha do PSOLA neste trabalho.
#
#  A pergunta, entao, nao e' "reamostrar move formante?" (move, por definicao),
#  e sim: MOVE O BASTANTE PARA IMPORTAR, NA FAIXA DE CORRECAO QUE ESTE MATERIAL
#  REALMENTE EXIGE? Se a correcao tipica for de 10 cents, o deslocamento e' de
#  0,6 % e ninguem ouve. Se for de 150 cents, e' de 9 % e muda a vogal.
#
#  Isto e' a questao 2 da §7 da pesquisa, e e' a medicao que decide se vale
#  implementar o v3 — antes de escrever qualquer linha do motor novo.
#
#  O QUE ELE MEDE
#  --------------
#    1. A distribuicao REAL de beta = fout/f0 no material, extraida do proprio
#       motor (stream_test dumpbeta=), sem reimplementar a malha de correcao.
#    2. O deslocamento de formante que essa distribuicao implica, em % (que e'
#       exatamente |beta - 1|), confrontado com os limiares de discriminacao
#       de formante da literatura.
#    3. Uma verificacao EMPIRICA, nao analitica: pega um trecho vozeado
#       sustentado, reamostra de fato pelos betas medidos, e mede o quanto os
#       picos do envelope cepstral andaram. Serve para confirmar que a conta
#       do item 2 descreve o sinal, e nao so' a teoria.
#    4. O contraste com o que o PSOLA de verdade produziu no mesmo trecho.
#
#  O CRITERIO
#  ----------
#  O limiar de discriminacao de frequencia de formante (razao de Weber dF/F) e'
#  reportado entre ~1 % e ~7 % conforme o estudo e o formante:
#
#    * Flanagan (1955): 1 % a 5 % para F2; 12 a 17 Hz para F1 em 300 Hz (~4 a 6 %)
#    * Mermelstein (1978): 6,8 %; 50 Hz para F1 em 350 Hz (~14 %)
#    * Estudos recentes de discriminacao de formante em vogais isoladas: 1 % a 2 %
#
#  Tomamos a faixa 1 %–5 % como zona de decisao, e o extremo conservador (1 %)
#  como limiar de "possivelmente audivel". Em cents:
#
#    dF/F = 2^(c/1200) - 1   =>   1 % ~ 17 cents ; 2 % ~ 34 cents ; 5 % ~ 85 cents
#
#  ATENCAO AO GRAU DE EVIDENCIA (padrao deste projeto): os limiares acima sao
#  de discriminacao de formante em VOGAIS SINTETICAS ISOLADAS, em escuta
#  atenta. Canto real, com vibrato e num contexto musical, quase certamente
#  tolera mais. Portanto este script produz um limite SUPERIOR do problema: se
#  ele disser "inaudivel", e' um resultado forte; se disser "audivel", e' um
#  alerta que so' a escuta confirma.
#
#  USO
#  ---
#    python3 python/medir_formante_resample.py              # compila e mede
#    python3 python/medir_formante_resample.py --bin DIR    # binarios prontos
#    python3 python/medir_formante_resample.py --wav A.wav  # outro material
#
#  Dependencias: numpy e soundfile (ha um .venv/ na raiz, fora do versionamento:
#  ./.venv/bin/python python/medir_formante_resample.py).
# =============================================================================

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

import numpy as np
import soundfile as sf

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Mesmo preset do medir_qualidade.py, para que os numeros conversem entre os
# dois relatorios. 'glide=' continua valendo como apelido de 'retune='.
PRESET = ["1.0", "crom", "tol=15", "glide=40", "look=4"]

HOP = 256

# Formantes de referencia de uma vogal aberta de voz feminina/contralto, usados
# so' para traduzir a porcentagem em Hz — a porcentagem em si NAO depende deles,
# porque a reamostragem escala todas as frequencias pelo mesmo fator.
FORMANTES_REF = [("F1", 700.0), ("F2", 1220.0), ("F3", 2600.0)]

# Zona de decisao, em fracao (ver o cabecalho).
JND_CONSERVADOR = 0.01   # 1 %  — abaixo disto, nenhum estudo reporta deteccao
JND_TIPICO = 0.05        # 5 %  — acima disto, todos reportam


# -----------------------------------------------------------------------------
#  Infraestrutura
# -----------------------------------------------------------------------------
def compilar(dir_bin, cxx):
    os.makedirs(dir_bin, exist_ok=True)
    destino = os.path.join(dir_bin, "stream_test")
    cmd = [cxx, "-std=c++17", "-O2", "-I", os.path.join(RAIZ, "external"),
           os.path.join(RAIZ, "src/c1_streaming/stream_test.cpp"), "-o", destino]
    print("  compilando stream_test ...", flush=True)
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr, file=sys.stderr)
        sys.exit("ERRO: falhou ao compilar stream_test")
    return destino


def carregar(caminho):
    x, fs = sf.read(caminho)
    if x.ndim > 1:
        x = x.mean(axis=1)
    return x.astype(float), fs


def ler_beta(caminho):
    """Le o dump (f0_hz, fout_hz) por quadro e devolve so' os quadros vozeados."""
    f0, fout = [], []
    with open(caminho) as fp:
        for linha in fp:
            if linha.startswith("#"):
                continue
            a, b = linha.split()
            a, b = float(a), float(b)
            if a > 0 and b > 0:
                f0.append(a)
                fout.append(b)
    return np.array(f0), np.array(fout)


# -----------------------------------------------------------------------------
#  Analise espectral — reaproveita a abordagem do formantes.py
# -----------------------------------------------------------------------------
def envelope(seg, fs, NFFT=8192, L=40):
    """Envelope cepstral (formantes) e espectro fino (harmonicos) de um trecho."""
    win = np.hanning(len(seg))
    X = np.fft.fft(seg * win, n=NFFT)
    c = np.fft.ifft(np.log(np.abs(X) + 1e-9)).real
    lift = np.zeros(NFFT)
    lift[:L] = 1
    lift[-L + 1:] = 1
    env = np.exp(np.fft.fft(c * lift).real)
    half = NFFT // 2
    return np.arange(half) * fs / NFFT, env[:half]


def picos(freqs, env, fmin=200.0, fmax=3500.0, n=3):
    """Os n maiores maximos locais do envelope, ordenados por frequencia."""
    cand = []
    for i in range(2, len(env) - 2):
        f = freqs[i]
        if f < fmin:
            continue
        if f > fmax:
            break
        if env[i] > env[i - 1] and env[i] >= env[i + 1] and env[i] > env[i - 2] and env[i] >= env[i + 2]:
            cand.append((env[i], f))
    cand.sort(reverse=True)
    return sorted(f for _, f in cand[:n])


def reamostrar(seg, beta):
    """O que o motor de ponteiro movel faz: le o buffer a uma taxa beta.

    y[n] = x[n*beta], com interpolacao linear entre amostras. Isso multiplica
    TODAS as frequencias por beta — harmonicos e formantes juntos. E' esta a
    diferenca de espécie para o PSOLA, que reposiciona periodos inteiros e
    deixa o envelope onde estava."""
    n = int(len(seg) / beta)
    idx = np.arange(n) * beta
    i0 = np.floor(idx).astype(int)
    frac = idx - i0
    i1 = np.minimum(i0 + 1, len(seg) - 1)
    return seg[i0] * (1 - frac) + seg[i1] * frac


def trecho_sustentado(x, fs, dur=0.06):
    """O trecho de maior energia — a vogal sustentada mais forte do material."""
    n = int(dur * fs)
    ene = np.convolve(x ** 2, np.ones(2048) / 2048, mode="same")
    c = int(np.argmax(ene))
    i0 = max(0, c - n // 2)
    return i0, x[i0:i0 + n]


# -----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--wav", default=os.path.join(RAIZ, "exemplo-antes.wav"))
    ap.add_argument("--bin", dest="dir_bin", default=None,
                    help="diretorio com stream_test ja compilado; pula a compilacao")
    ap.add_argument("--cxx", default=os.environ.get("CXX") or
                    ("g++" if shutil.which("g++") else "clang++"))
    args = ap.parse_args()

    tmp = tempfile.mkdtemp(prefix="formante_")
    exe = (os.path.join(args.dir_bin, "stream_test") if args.dir_bin
           else compilar(os.path.join(tmp, "bin"), args.cxx))

    print("=" * 78)
    print("  DESLOCAMENTO DE FORMANTE DA REAMOSTRAGEM — o v3 e' viavel?")
    print("=" * 78)
    print(f"material : {os.path.relpath(args.wav, RAIZ)}")
    print(f"preset   : {' '.join(PRESET)}")
    print()

    # -- 1. beta real, extraido do proprio motor -----------------------------
    saida = os.path.join(tmp, "corrigido.wav")
    dump = os.path.join(tmp, "beta.txt")
    r = subprocess.run([exe, args.wav, saida] + PRESET + [f"dumpbeta={dump}"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout, r.stderr, file=sys.stderr)
        sys.exit("ERRO: stream_test falhou")

    f0, fout = ler_beta(dump)
    if len(f0) == 0:
        sys.exit("ERRO: nenhum quadro vozeado no dump")

    beta = fout / f0
    cents = np.abs(1200.0 * np.log2(beta))
    desloc = np.abs(beta - 1.0)          # dF/F da reamostragem, exato

    print("-" * 78)
    print("1. DISTRIBUICAO DA CORRECAO REAL")
    print("-" * 78)
    print(f"quadros vozeados: {len(f0)}   (de {len(open(dump).readlines()) - 1} no total)")
    print()
    print("  percentil |  correcao  | desloc. de formante |   F1 700Hz    F2 1220Hz   F3 2600Hz")
    print("  ----------+------------+---------------------+---------------------------------------")
    ps = [50, 75, 90, 95, 99]
    for p in ps:
        c = np.percentile(cents, p)
        d = 2 ** (c / 1200.0) - 1
        hz = "  ".join(f"{f * d:7.1f} Hz" for _, f in FORMANTES_REF)
        print(f"      p{p:<3d}  | {c:6.1f} ct  |      {d * 100:5.2f} %        | {hz}")
    c = cents.max()
    d = 2 ** (c / 1200.0) - 1
    hz = "  ".join(f"{f * d:7.1f} Hz" for _, f in FORMANTES_REF)
    print(f"      max   | {c:6.1f} ct  |      {d * 100:5.2f} %        | {hz}")
    print()
    print(f"  media |correcao| = {cents.mean():.1f} cents "
          f"({(2 ** (cents.mean() / 1200.0) - 1) * 100:.2f} % de deslocamento)")

    # -- 2. fracao do material acima de cada limiar --------------------------
    print()
    print("-" * 78)
    print("2. QUANTO DO MATERIAL CRUZA O LIMIAR PERCEPTUAL")
    print("-" * 78)
    print("  Limiares de discriminacao de formante (razao de Weber dF/F) da literatura:")
    print("  1 % = extremo conservador (nenhum estudo reporta deteccao abaixo)")
    print("  5 % = extremo tipico (todos reportam deteccao acima)")
    print()
    for nome, lim in [("conservador (1 %)", JND_CONSERVADOR), ("tipico (5 %)", JND_TIPICO)]:
        frac = float(np.mean(desloc > lim))
        ct = 1200 * np.log2(1 + lim)
        print(f"  acima do limiar {nome:<20s} = {ct:5.1f} ct : "
              f"{frac * 100:5.1f} % dos quadros vozeados")

    # -- 2b. o limite que NAO depende deste cantor ---------------------------
    print()
    print("-" * 78)
    print("2b. O LIMITE ESTRUTURAL — vale para qualquer cantor, nao so' este")
    print("-" * 78)
    print("  Os numeros acima sao deste material. Mas existe um teto que independe")
    print("  dele: a correcao leva a nota ao alvo MAIS PROXIMO da escala, entao o")
    print("  desvio corrigido nunca passa de METADE do maior intervalo entre notas")
    print("  permitidas. Isso limita beta por construcao, e portanto limita o")
    print("  deslocamento de formante — para qualquer entrada, por pior que seja.")
    print()
    print("   escala                  | maior salto | correcao max | desloc. max | veredito")
    print("   ------------------------+-------------+--------------+-------------+--------------")
    for nome, semitons in [("cromatica (12 notas)", 1), ("maior / menor (7 notas)", 2),
                           ("pentatonica (5 notas)", 3)]:
        ct_max = semitons * 100.0 / 2.0
        d = 2 ** (ct_max / 1200.0) - 1
        if d < JND_CONSERVADOR:
            v = "sempre abaixo"
        elif d < JND_TIPICO:
            v = "zona cinzenta"
        else:
            v = "acima"
        plural = "semitons" if semitons > 1 else "semitom "
        print(f"   {nome:<23s} | {semitons} {plural} |"
              f"  {ct_max:5.1f} cents |    {d * 100:5.2f} %   | {v}")
    print()
    print("  Leitura: na escala CROMATICA — que e' o padrao do plugin — o")
    print("  deslocamento de formante da reamostragem nao pode passar de 2,93 %,")
    print("  aconteca o que acontecer na entrada. Isso ja fica abaixo do limiar")
    print("  tipico de 5 %. Em escala diatonica o teto sobe para 5,95 % e entra na")
    print("  faixa audivel — mas so' na nota pior corrigida, nao no material todo.")

    # -- 3. verificacao empirica no sinal ------------------------------------
    print()
    print("-" * 78)
    print("3. VERIFICACAO NO SINAL — os formantes andam mesmo o previsto?")
    print("-" * 78)
    x, fs = carregar(args.wav)
    i0, seg = trecho_sustentado(x, fs)
    fr, env = envelope(seg, fs)
    base = picos(fr, env)
    if not base:
        print("  (nenhum pico de envelope isolavel no trecho; item pulado)")
    else:
        print(f"  trecho sustentado em t = {i0 / fs:.2f} s ({len(seg) / fs * 1000:.0f} ms)")
        print(f"  formantes da entrada: {', '.join(f'{f:.0f} Hz' for f in base)}")
        print()
        print("   correcao | motor         | formantes medidos            | erro medio")
        print("   ---------+---------------+------------------------------+-----------")
        for p in [50, 95, 99]:
            c = np.percentile(cents, p)
            b = 2 ** (c / 1200.0)
            fr2, env2 = envelope(reamostrar(seg, 1.0 / b), fs)
            pk = picos(fr2, env2)
            k = min(len(pk), len(base))
            if k == 0:
                continue
            err = float(np.mean([abs(pk[i] - base[i]) for i in range(k)]))
            txt = ", ".join(f"{f:6.0f}" for f in pk[:k])
            print(f"   p{p:<3d} {c:5.1f}ct | reamostragem  | {txt:<28s} | {err:6.1f} Hz")

        # o que o PSOLA realmente fez no mesmo trecho, alinhado pela latencia
        y, _ = carregar(saida)
        lat = 0
        for linha in r.stdout.splitlines():
            if "lat=" in linha:
                lat = int(linha.split("lat=")[1].split()[0])
        j0 = i0 + lat
        if j0 + len(seg) <= len(y):
            fr3, env3 = envelope(y[j0:j0 + len(seg)], fs)
            pk = picos(fr3, env3)
            k = min(len(pk), len(base))
            if k:
                err = float(np.mean([abs(pk[i] - base[i]) for i in range(k)]))
                txt = ", ".join(f"{f:6.0f}" for f in pk[:k])
                print(f"   (real)      | TD-PSOLA      | {txt:<28s} | {err:6.1f} Hz")
                print()
                print(f"   [alinhado pela latencia de {lat} amostras reportada pelo motor]")
                print()
                print(f"   ⚠ PISO DE RUIDO DO METODO: o TD-PSOLA preserva formantes por")
                print(f"     construcao, entao a linha dele DEVERIA dar ~0 Hz. Ela da"
                      f" {err:.0f} Hz.")
                print(f"     Esses {err:.0f} Hz sao o erro do proprio medidor — resolucao do")
                print( "     envelope cepstral, janela de 60 ms e escolha de picos. Portanto")
                print( "     so' as diferencas MAIORES que isso significam alguma coisa, e os")
                print( "     valores absolutos da tabela nao devem ser citados como medida")
                print( "     de deslocamento. A medida confiavel e' a do item 1, que e'")
                print( "     analitica: dF/F = |beta - 1|, exata por definicao.")

    # -- 4. veredito ---------------------------------------------------------
    p95 = 2 ** (np.percentile(cents, 95) / 1200.0) - 1
    print()
    print("=" * 78)
    print("VEREDITO")
    print("=" * 78)
    print(f"  p95 do deslocamento de formante sob reamostragem: {p95 * 100:.2f} %")
    if p95 < JND_CONSERVADOR:
        print("  => ABAIXO do limiar mais conservador da literatura em 95 % do material.")
        print("     A perda de preservacao de formantes NAO e' um impedimento ao v3.")
    elif p95 < JND_TIPICO:
        print("  => Entre o limiar conservador e o tipico. Indeciso pelo numero:")
        print("     provavelmente inaudivel em contexto musical, mas so' a escuta")
        print("     resolve. Um teste A/B cego com o mesmo usuario decide.")
    else:
        print("  => ACIMA do limiar tipico. Reamostrar pura e simplesmente mudaria a")
        print("     vogal em parte relevante do material. O v3 precisaria de correcao")
        print("     de formante, o que anula boa parte da simplicidade que o motiva.")
    teto_crom = 2 ** (50.0 / 1200.0) - 1
    print()
    print(f"  E o teto ESTRUTURAL na escala cromatica: {teto_crom * 100:.2f} % — abaixo do")
    print("  limiar tipico de 5 % para QUALQUER entrada, nao so' para este material.")
    print("  Este e' o resultado que sustenta a decisao, porque nao depende do cantor.")
    print()
    print("  Ressalvas de metodo:")
    print("   * Os limiares sao de vogais sinteticas isoladas em escuta atenta. Canto")
    print("     real tolera mais — o numero e' um limite SUPERIOR do problema.")
    print("   * A distribuicao do item 1 e' de UM cantor, razoavelmente afinado, com")
    print("     tol=15. Outro material desloca a distribuicao, mas NAO o teto de 2b.")
    print("   * Em escala diatonica o teto sobe para 5,95 % e entra na faixa audivel.")
    print("   * Nada aqui mede o artefato de EMENDA do ponteiro movel (modulacao de")
    print("     amplitude na troca de ciclo). Isso e' outro risco do v3, e so' a")
    print("     implementacao mede.")
    print("  Ver docs/pesquisa-latencia-antares.md §7.")
    print("=" * 78)

    shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
