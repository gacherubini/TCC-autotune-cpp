// ============================================================================
//  Protótipo de autotune — PASSOS 5 e 6
//  Detecta pitch (pYIN), escolhe a nota-alvo (cromática) e corrige a afinação
//  com TD-PSOLA, gravando um WAV corrigido.
//
//  Uso: autotune.exe <entrada.wav> [saida.wav] [forca 0..1]
// ============================================================================

#define DR_WAV_IMPLEMENTATION
#include "../core/dsp.h"   // constantes + helpers compartilhados (CMNDF, candidato, nota-alvo, WAV...)

// Suaviza o vozeamento da trajetória: tampa buracos curtos (interpolando o pitch)
// e apaga "ilhas" de nota curtas demais. Reduz o liga-desliga que causa cliques.
void suavizarVozeamento(std::vector<double>& tr, int minSeg, int maxGap) {
    long long n = (long long)tr.size();
    // 1) tampar buracos curtos (não vozeados) entre dois trechos com nota
    for (long long i = 0; i < n;) {
        if (tr[i] <= 0) {
            long long j = i; while (j < n && tr[j] <= 0) j++;
            if (i > 0 && j < n && (j - i) <= maxGap) {
                double a = tr[i - 1], b = tr[j];
                for (long long k = i; k < j; ++k) {
                    double f = (double)(k - i + 1) / (j - i + 1);
                    tr[k] = a * std::pow(b / a, f); // interpolação em pitch (geométrica)
                }
            }
            i = j;
        } else i++;
    }
    // 2) apagar trechos vozeados curtos demais (ruído isolado)
    for (long long i = 0; i < n;) {
        if (tr[i] > 0) {
            long long j = i; while (j < n && tr[j] > 0) j++;
            if ((j - i) < minSeg) for (long long k = i; k < j; ++k) tr[k] = 0;
            i = j;
        } else i++;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Uso: %s <entrada.wav> [saida.wav] [forca 0..1] [escala] [tol=cents] [glide=ms]\n", argv[0]);
        std::printf("  escala: crom (padrao) | C, G, F#... (maior) | Am, C#m... (menor)\n");
        std::printf("  tol=   : zona morta em cents (nao corrige desvios menores) — natural. Padrao 0.\n");
        std::printf("  glide= : suavizacao temporal da afinacao em ms (portamento). Padrao 0 (snap).\n");
        std::printf("  Ex.: %s voz.wav out.wav 1.0 Am tol=15 glide=40   (preset natural)\n", argv[0]);
        return 1;
    }
    const char* saida = (argc >= 3) ? argv[2] : "saida_corrigida.wav";
    bool bypass = (argc >= 4 && std::atof(argv[3]) < 0); // forca negativa = modo copia
    double forca = (argc >= 4) ? std::atof(argv[3]) : 1.0;
    if (forca < 0) forca = 0;
    if (forca > 1) forca = 1;
    // escala = argv[4] só se for um nome de escala (não um flag chave=valor)
    std::string a4 = (argc >= 5) ? argv[4] : "";
    const char* escalaTxt = (!a4.empty() && a4.find('=') == std::string::npos) ? argv[4] : "crom";
    definirEscala(escalaTxt);
    // flags opcionais chave=valor (ordem livre, a partir do argv[2])
    double tolCents = 0.0;   // zona morta em cents (0 = corrige tudo)
    double glideMs  = 0.0;   // glide/retune em ms  (0 = snap imediato)
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if      (a.rfind("tol=", 0) == 0)   tolCents = std::atof(a.c_str() + 4);
        else if (a.rfind("glide=", 0) == 0) glideMs  = std::atof(a.c_str() + 6);
    }
    if (tolCents < 0) tolCents = 0;
    if (glideMs  < 0) glideMs  = 0;

    // 1. Ler WAV -> mono
    unsigned int canais = 0, taxa = 0; drwav_uint64 n = 0;
    float* dados = drwav_open_file_and_read_pcm_frames_f32(argv[1], &canais, &taxa, &n, nullptr);
    if (!dados) { std::printf("ERRO: nao consegui abrir '%s'\n", argv[1]); return 1; }
    long long N = (long long)n;
    std::vector<float> x(N);
    for (long long i = 0; i < N; ++i) {
        float s = 0; for (unsigned c = 0; c < canais; ++c) s += dados[i * canais + c];
        x[i] = s / (float)canais;
    }
    drwav_free(dados, nullptr);
    int fs = (int)taxa;
    std::printf("Sinal: %.2f s | %u Hz | forca=%.2f\n", (double)N / fs, taxa, forca);

    // MODO COPIA (diagnóstico): grava o mono sem processar e sai.
    if (bypass) {
        gravarWav16(saida, x, taxa);
        std::printf("MODO COPIA (sem PSOLA, sem correcao) gravado em: %s\n", saida);
        return 0;
    }
    {
        static const char* pcn[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        std::printf("Escala: %s  (notas alvo: ", escalaTxt);
        for (int i = 0; i < 12; ++i) if (g_permitida[i]) std::printf("%s ", pcn[i]);
        std::printf(")\n");
        std::printf("Tolerancia: %.0f cents (zona morta) | Glide: %.0f ms\n", tolCents, glideMs);
    }

    // 2. pYIN -> F0 por quadro
    int W = N_FRAME / 2, tauMax = W, tauMin = std::max(1, (int)std::floor(fs / FMAX));
    long long numQ = (N >= N_FRAME) ? 1 + (N - N_FRAME) / N_HOP : 0;
    int nBins = binDe(FMAX) + 1, UV = nBins, nEst = nBins + 1;

    const int K = 100;
    std::vector<double> sLim(K), wLim(K); double sw = 0;
    for (int k = 0; k < K; ++k) { double s = (k + 0.5) / K; sLim[k] = s; wLim[k] = s * std::pow(1 - s, 17.0); sw += wLim[k]; }
    for (int k = 0; k < K; ++k) wLim[k] /= sw;

    std::vector<std::vector<double>> obs(numQ, std::vector<double>(nBins, 0.0));
    std::vector<double> pUnv(numQ, 1.0), dp;
    for (long long q = 0; q < numQ; ++q) {
        calcularCMNDF(x, q * N_HOP, W, tauMax, dp);
        double massa = 0;
        for (int k = 0; k < K; ++k) {
            double f = candidato(dp, tauMin, tauMax, sLim[k], fs);
            if (f > 0) { int b = binDe(f); if (b >= 0 && b < nBins) { obs[q][b] += wLim[k]; massa += wLim[k]; } }
        }
        pUnv[q] = 1.0 - massa;
    }

    // Viterbi
    std::vector<double> delta(nEst, NEG_INF), dPrev(nEst, NEG_INF);
    std::vector<std::vector<int>> psi(numQ, std::vector<int>(nEst, -1));
    auto emiss = [&](long long t, int s) { return (s == UV) ? std::log(pUnv[t] + EPS) : std::log(obs[t][s] + EPS); };
    if (numQ > 0) {
        for (int s = 0; s < nEst; ++s) dPrev[s] = emiss(0, s);
        for (long long t = 1; t < numQ; ++t) {
            double maxV = NEG_INF; int argV = -1;
            for (int b = 0; b < nBins; ++b) if (dPrev[b] > maxV) { maxV = dPrev[b]; argV = b; }
            for (int b2 = 0; b2 < nBins; ++b2) {
                double best = NEG_INF; int arg = -1;
                int lo = std::max(0, b2 - W_TRANS), hi = std::min(nBins - 1, b2 + W_TRANS);
                for (int b1 = lo; b1 <= hi; ++b1) {
                    double d = b2 - b1;
                    double v = dPrev[b1] + LOG_STAY_V - 0.5 * (d / SIGMA_TRANS) * (d / SIGMA_TRANS);
                    if (v > best) { best = v; arg = b1; }
                }
                double vu = dPrev[UV] + LOG_SWITCH;
                if (vu > best) { best = vu; arg = UV; }
                delta[b2] = best + emiss(t, b2); psi[t][b2] = arg;
            }
            double bUV = dPrev[UV] + LOG_STAY_UV; int aUV = UV;
            if (maxV + LOG_SWITCH > bUV) { bUV = maxV + LOG_SWITCH; aUV = argV; }
            delta[UV] = bUV + emiss(t, UV); psi[t][UV] = aUV;
            std::swap(delta, dPrev);
        }
    }
    std::vector<double> trackF0(numQ, 0.0);
    if (numQ > 0) {
        double best = NEG_INF; int s = UV;
        for (int e = 0; e < nEst; ++e) if (dPrev[e] > best) { best = dPrev[e]; s = e; }
        for (long long t = numQ - 1; t >= 0; --t) { trackF0[t] = (s == UV) ? 0.0 : fDeBin(s); if (t > 0) s = psi[t][s]; }
    }

    // Suaviza o vozeamento (tampa buracos <=4 quadros, remove ilhas <4 quadros)
    suavizarVozeamento(trackF0, 4, 4);

    // 3. F0 por amostra (segura o valor do quadro)
    std::vector<float> f0samp(N, 0.0f);
    for (long long q = 0; q < numQ; ++q)
        for (int k = 0; k < N_HOP; ++k) { long long i = q * N_HOP + k; if (i < N) f0samp[i] = (float)trackF0[q]; }

    // 3b. Trajetória de afinação-ALVO por amostra, com GLIDE (suavização temporal).
    // Suaviza em cents (domínio log) com um filtro de 1 polo, reiniciando no ATAQUE
    // de cada nota (não desliza a partir do silêncio). glide=0 -> alvo instantâneo.
    // É isto que tira o efeito "robô": a correção desliza entre notas (portamento).
    std::vector<float> foutSamp(N, 0.0f);
    {
        // Etapa 0 do plano: a malha (zona morta + glide + reset no ataque) mora
        // agora em dsp.h, compartilhada com o causal e com o streaming.
        ParamsCorrecao pc; pc.forca = forca; pc.tolCents = tolCents; pc.glideMs = glideMs;
        CorretorAltura corr; corr.prepare(fs);
        for (long long i = 0; i < N; ++i)
            foutSamp[i] = (float)corr.proxima(f0samp[i], pc);
    }

    // 4. PASSO 5 — prévia da correção (1 leitura por segundo)
    std::printf("\nCorrecao planejada (1 leitura por segundo):\n");
    long long passo = std::max(1LL, (long long)std::lround((double)fs / N_HOP));
    for (long long q = 0; q < numQ; q += passo) {
        double t = (double)(q * N_HOP) / fs, f = trackF0[q];
        if (f > 0) {
            double ft = notaAlvo(f, forca, tolCents);
            double dc = 1200.0 * std::log2(ft / f); // cents de correção aplicados
            std::printf("  t=%5.1fs  %6.1f Hz -> %-4s (%6.1f Hz)  correcao %+5.0f ct\n",
                        t, f, hzParaNota(ft).c_str(), ft, dc);
        } else {
            std::printf("  t=%5.1fs  --- (sem nota)\n", t);
        }
    }

    // 5/6. TD-PSOLA (marcas + reamostragem com preservação de duração + cobertura).
    // Função compartilhada em dsp.h — a mesma usada pelo motor causal (autotune_rt).
    std::vector<float> out = psolaSintetiza(x, N, f0samp, foutSamp, fs);

    // 6. Gravar WAV corrigido (16-bit PCM)
    if (!gravarWav16(saida, out, taxa)) { std::printf("ERRO ao criar '%s'\n", saida); return 1; }

    std::printf("\nPronto! Audio corrigido gravado em: %s\n", saida);
    std::printf("Compare ouvindo: entrada (%s) x saida (%s)\n", argv[1], saida);
    return 0;
}
