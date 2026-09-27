// ============================================================================
//  autotune_rt — CAMINHO B: autotune CAUSAL / streaming
//
//  Versão de tempo real do autotune: a detecção de pitch usa um Viterbi de
//  LAG FIXO (decisão do quadro t olhando no máximo 'look' quadros à frente), em
//  vez do Viterbi global (offline). Processa de forma estritamente causal e
//  REPORTA a latência algorítmica (ms) e o fator de tempo real (xRT).
//
//  O resto do pipeline (marcas + PSOLA com preservação de duração + cobertura) é
//  LOCAL (look-ahead ~1 período) e reaproveitado de dsp.h — o mesmo do offline.
//
//  Uso: autotune_rt.exe <in.wav> [out.wav] [mix] [escala] [tol=] [retune=] [vibrato=] [humanize=] [vib*=] [look=L] [block=N]
// ============================================================================
#define DR_WAV_IMPLEMENTATION
#include "../core/dsp.h"
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Uso: %s <in.wav> [out.wav] [mix] [escala] [tol=] [retune=] [vibrato=] [humanize=] [vibforma=] [vibtaxa=] [vibprof=] [vibamp=] [look=] [block=] [frame=] [hop=] [voz=] [fmin=] [fmax=] [dumpf0=]\n", argv[0]);
        std::printf("  look=  : quadros de look-ahead do Viterbi causal (0=guloso, +qualidade c/ +latencia). Padrao 4.\n");
        std::printf("  block= : tamanho do bloco de audio (afeta latencia). Padrao %d.\n", N_HOP);
        std::printf("  frame= : tamanho do quadro de analise (menor=menos latencia, mas detecta menos graves). Padrao %d.\n", N_FRAME);
        std::printf("  hop=   : passo entre quadros. Padrao %d.\n", N_HOP);
        std::printf("  voz=   : preset de tessitura (estilo 'Vocal Range' do Auto-Tune): baixo/baritono/tenor/contralto/mezzo/soprano | lowmale/altotenor | instrumento.\n");
        std::printf("  fmin=  : freq minima de busca (Hz). Domina o termo PSOLA da latencia (fs/fmin). Sobrescreve voz=.\n");
        std::printf("  fmax=  : freq maxima de busca (Hz). Sobrescreve voz=.\n");
        std::printf("  dumpf0=: grava o F0 detectado por quadro num .txt (analise; nao afeta a saida).\n");
        return 1;
    }
    const char* saida = (argc >= 3) ? argv[2] : "saida_rt.wav";
    // ETAPA 2: 3o posicional era 'forca', agora e' 'mix' (seco/molhado).
    double mix = (argc >= 4) ? std::atof(argv[3]) : 1.0;
    if (mix < 0) mix = 0;
    if (mix > 1) mix = 1;
    std::string a4 = (argc >= 5) ? argv[4] : "";
    const char* escalaTxt = (!a4.empty() && a4.find('=') == std::string::npos) ? argv[4] : "crom";
    definirEscala(escalaTxt);
    // Etapa 5: flags da malha lidas por lerFlagCorrecao() (dsp.h), num lugar so.
    ParamsCorrecao pc;
    int look = 4, block = N_HOP, nFrame = N_FRAME, nHop = N_HOP;
    double fminFlag = FMIN, fmaxFlag = FMAX;   // padrão = constantes do dsp.h
    std::string dumpF0Path, vozNome;
    bool fminExpl = false, fmaxExpl = false;   // fmin=/fmax= explícitos vencem o preset voz=
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if      (lerFlagCorrecao(a, pc)) { /* flag da malha (dsp.h) */ }
        else if (a.rfind("look=", 0)   == 0) look     = std::atoi(a.c_str() + 5);
        else if (a.rfind("block=", 0)  == 0) block    = std::atoi(a.c_str() + 6);
        else if (a.rfind("frame=", 0)  == 0) nFrame   = std::atoi(a.c_str() + 6);
        else if (a.rfind("hop=", 0)    == 0) nHop     = std::atoi(a.c_str() + 4);
        else if (a.rfind("voz=", 0)    == 0) vozNome  = a.c_str() + 4;
        else if (a.rfind("fmin=", 0)   == 0) { fminFlag = std::atof(a.c_str() + 5); fminExpl = true; }
        else if (a.rfind("fmax=", 0)   == 0) { fmaxFlag = std::atof(a.c_str() + 5); fmaxExpl = true; }
        else if (a.rfind("dumpf0=", 0) == 0) dumpF0Path = a.c_str() + 7;
    }
    // Preset de tessitura: define a faixa; fmin=/fmax= explícitos têm prioridade.
    if (!vozNome.empty()) {
        double pf = 0, px = 0;
        if (presetVoz(vozNome, pf, px)) {
            if (!fminExpl) fminFlag = pf;
            if (!fmaxExpl) fmaxFlag = px;
        } else {
            std::printf("AVISO: voz='%s' desconhecida (baixo/baritono/tenor/contralto/mezzo/soprano | lowmale/altotenor | instrumento). Ignorando.\n", vozNome.c_str());
        }
    }
    sanearCorrecao(pc);
    if (look < 0) look = 0;
    if (block < 1) block = 1;
    if (nFrame < 128) nFrame = 128;        // mínimo p/ o YIN cobrir até FMAX (tauMax>=fs/FMAX)
    if (nHop < 1) nHop = 1;
    if (nHop > nFrame) nHop = nFrame;
    // Faixa de pitch (sobrescreve as constantes globais ANTES de qualquer cálculo).
    // FMIN domina o termo PSOLA da latência (fs/FMIN = período mais longo); subir FMIN
    // baixa a latência, mas para de corrigir notas abaixo dele. FMAX = nota mais aguda.
    if (fminFlag < 20)  fminFlag = 20;
    if (fmaxFlag <= fminFlag + 1) fmaxFlag = fminFlag + 1;
    FMIN = fminFlag;
    FMAX = fmaxFlag;

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
    if (FMAX > 0.45 * fs) FMAX = 0.45 * fs;   // segurança: tauMin = fs/FMAX >= ~2 amostras
    double fDetMin = (double)fs / (nFrame / 2);  // menor freq que a janela do YIN alcança
    std::printf("Sinal: %.2f s | %u Hz | mix=%.2f | look=%d | frame=%d | hop=%d | block=%d\n",
                (double)N / fs, taxa, mix, look, nFrame, nHop, block);
    std::printf("Faixa de pitch: FMIN=%.0f Hz .. FMAX=%.0f Hz%s%s%s\n", FMIN, FMAX,
                vozNome.empty() ? "" : "  [voz=", vozNome.empty() ? "" : vozNome.c_str(), vozNome.empty() ? "" : "]");
    if (FMIN < fDetMin)
        std::printf("  (atencao: FMIN abaixo do alcance da janela ~%.0f Hz; aumente frame= p/ valer)\n", fDetMin);
    std::printf("Pitch detectavel a partir de ~%.0f Hz (janela YIN = frame/2 = %d amostras)\n",
                fDetMin, nFrame / 2);
    {
        static const char* pcn[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        std::printf("Escala: %s (", escalaTxt);
        for (int i = 0; i < 12; ++i) if (g_permitida[i]) std::printf("%s ", pcn[i]);
        std::printf(") | %s\n", resumoCorrecao(pc).c_str());
    }

    auto t_ini = std::chrono::high_resolution_clock::now();

    // 2. PITCH CAUSAL — Viterbi de LAG FIXO.
    int W = nFrame / 2, tauMax = W, tauMin = std::max(1, (int)std::floor(fs / FMAX));
    long long numQ = (N >= nFrame) ? 1 + (N - nFrame) / nHop : 0;
    int nBins = binDe(FMAX) + 1, UV = nBins, nEst = nBins + 1;

    const int K = 100;
    std::vector<double> sLim(K), wLim(K); double sw = 0;
    for (int k = 0; k < K; ++k) { double s = (k + 0.5) / K; sLim[k] = s; wLim[k] = s * std::pow(1 - s, 17.0); sw += wLim[k]; }
    for (int k = 0; k < K; ++k) wLim[k] /= sw;

    std::vector<std::vector<int>> psi(numQ, std::vector<int>(nEst, -1));
    std::vector<int> bestAt(numQ, UV);
    std::vector<double> delta(nEst, NEG_INF), dPrev(nEst, NEG_INF), dp;
    std::vector<double> obs(nBins, 0.0); double pUnv = 1.0;

    for (long long q = 0; q < numQ; ++q) {
        // observação do quadro q (idêntica ao offline, mas só deste quadro)
        calcularCMNDF(x, q * nHop, W, tauMax, dp);
        std::fill(obs.begin(), obs.end(), 0.0);
        double massa = 0;
        for (int k = 0; k < K; ++k) {
            double f = candidato(dp, tauMin, tauMax, sLim[k], fs);
            if (f > 0) { int b = binDe(f); if (b >= 0 && b < nBins) { obs[b] += wLim[k]; massa += wLim[k]; } }
        }
        pUnv = 1.0 - massa;
        auto emiss = [&](int s) { return (s == UV) ? std::log(pUnv + EPS) : std::log(obs[s] + EPS); };

        if (q == 0) {
            for (int s = 0; s < nEst; ++s) dPrev[s] = emiss(s);
        } else {
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
                delta[b2] = best + emiss(b2); psi[q][b2] = arg;
            }
            double bUV = dPrev[UV] + LOG_STAY_UV; int aUV = UV;
            if (maxV + LOG_SWITCH > bUV) { bUV = maxV + LOG_SWITCH; aUV = argV; }
            delta[UV] = bUV + emiss(UV); psi[q][UV] = aUV;
            std::swap(delta, dPrev);
        }
        // melhor estado do quadro q (horizonte de decisão)
        double best = NEG_INF; int sb = UV;
        for (int e = 0; e < nEst; ++e) if (dPrev[e] > best) { best = dPrev[e]; sb = e; }
        bestAt[q] = sb;
    }

    // Decisão de LAG FIXO: o quadro t é decidido olhando o horizonte t+look
    // (no máx. numQ-1) e retrocedendo. look=0 -> guloso. Quanto maior, mais perto
    // do Viterbi global, porém mais latência.
    std::vector<double> trackF0(numQ, 0.0);
    for (long long t = 0; t < numQ; ++t) {
        long long e = std::min(t + look, numQ - 1);
        int s = bestAt[e];
        for (long long f = e; f > t; --f) s = psi[f][s];
        trackF0[t] = (s == UV) ? 0.0 : fDeBin(s);
    }
    // (sem suavização de vozeamento: a offline é não-causal; aqui confiamos no
    //  Viterbi. É uma diferença esperada que pode custar um pouco de qualidade.)

    // Dump opcional do F0 por quadro (Hz; 0 = sem nota) p/ analise externa (ex.: achar
    // a nota mais grave realmente cantada). Reaproveita o detector real, sem afetar a saida.
    if (!dumpF0Path.empty()) {
        FILE* fp = std::fopen(dumpF0Path.c_str(), "w");
        if (fp) {
            for (long long t = 0; t < numQ; ++t) std::fprintf(fp, "%.4f\n", trackF0[t]);
            std::fclose(fp);
            std::printf("F0 por quadro gravado em: %s (%lld quadros, hop=%d)\n", dumpF0Path.c_str(), numQ, nHop);
        } else std::printf("AVISO: nao consegui escrever dumpf0='%s'\n", dumpF0Path.c_str());
    }

    // 3. F0 real por amostra
    std::vector<float> f0samp(N, 0.0f);
    for (long long q = 0; q < numQ; ++q)
        for (int k = 0; k < nHop; ++k) { long long i = q * nHop + k; if (i < N) f0samp[i] = (float)trackF0[q]; }

    // 3b. Pitch-ALVO por amostra (Etapa 3: LP(alvo) + k*HP(real), reset no ataque)
    std::vector<float> foutSamp(N, 0.0f);
    std::vector<float> ganhoSamp(N, 1.0f);   // Etapa 5
    {
        // Etapa 0 do plano: malha compartilhada (ver dsp.h / CorretorAltura).
        CorretorAltura corr; corr.prepare(fs);
        for (long long i = 0; i < N; ++i) {
            foutSamp[i]  = (float)corr.proxima(f0samp[i], pc);
            ganhoSamp[i] = (float)corr.ultimoGanho();   // Etapa 5
        }
    }

    // 4. TD-PSOLA (compartilhado com o offline; local -> causal)
    std::vector<float> out = psolaSintetiza(x, N, f0samp, foutSamp, fs);

    // 4a. ETAPA 5 — modulacao de amplitude do Create Vibrato (depois do PSOLA).
    if (pc.vibAmp > 0.0) for (long long i = 0; i < N; ++i) out[i] *= ganhoSamp[i];

    // 4b. ETAPA 2 — mistura seco/molhado. Fica DENTRO da regiao cronometrada de
    // proposito: e' custo de processamento real, e omiti-lo inflaria o xRT a
    // nosso favor. Alinhamento trivial aqui (o PSOLA preserva a duracao).
    if (mix < 1.0) for (long long i = 0; i < N; ++i) out[i] = misturar(x[i], out[i], mix);

    auto t_fim = std::chrono::high_resolution_clock::now();
    double procS = std::chrono::duration<double>(t_fim - t_ini).count();

    // 5. Latência algorítmica (orçamento) e xRT
    long long latPitch = (long long)look * nHop + nFrame;     // look-ahead + quadro de análise
    long long latPsola = (long long)std::llround(fs / FMIN);  // ~1 período (o mais longo) p/ o grão
    long long latTot   = latPitch + latPsola + block;         // + bloco do callback
    double durS = (double)N / fs;
    std::printf("\n--- TEMPO REAL ---\n");
    std::printf("Latencia algoritmica: %.1f ms  (pitch %.1f + psola %.1f + bloco %.1f)\n",
                1000.0 * latTot / fs, 1000.0 * latPitch / fs, 1000.0 * latPsola / fs, 1000.0 * block / fs);
    std::printf("Processamento: %.3f s para %.2f s de audio  ->  xRT = %.3f  (%s)\n",
                procS, durS, procS / durS, (procS < durS ? "VIAVEL em tempo real" : "NAO viavel (xRT>=1)"));

    if (!gravarWav16(saida, out, taxa)) { std::printf("ERRO ao criar '%s'\n", saida); return 1; }
    std::printf("Gravado: %s\n", saida);
    return 0;
}
