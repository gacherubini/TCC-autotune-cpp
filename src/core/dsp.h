// ============================================================================
//  dsp.h — funções de DSP compartilhadas entre o autotune OFFLINE (main.cpp)
//  e o autotune CAUSAL/STREAMING (autotune_rt.cpp).
//
//  Cada .cpp que usar isto deve definir DR_WAV_IMPLEMENTATION ANTES de incluir,
//  exatamente em UMA unidade de tradução (cada executável tem a sua).
// ============================================================================
#ifndef DSP_H
#define DSP_H

#include <cstdio>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cctype>

#include "dr_wav.h"

static const double PI = 3.14159265358979323846;

// Análise
static const int    N_FRAME = 1024;
static const int    N_HOP   = 256;
inline double       FMIN    = 80.0;    // mutável: o autotune_rt pode sobrescrever por flag (fmin=)
inline double       FMAX    = 1000.0;  // idem (fmax=). binDe/fDeBin e o termo PSOLA da latência leem isto.
// pYIN
static const double RES_CENTS  = 20.0;
static const int    W_TRANS    = 12;
static const double SIGMA_TRANS= 2.0;
static const double LOG_STAY_V = std::log(0.99);
static const double LOG_SWITCH = std::log(0.01);
static const double LOG_STAY_UV= std::log(0.99);
static const double EPS        = 1e-9;
static const double NEG_INF    = -1e300;

// ---------- CMNDF de um quadro ----------
inline void calcularCMNDF(const std::vector<float>& x, long long ini, int W, int tauMax,
                          std::vector<double>& dp) {
    std::vector<double> d(tauMax + 1, 0.0);
    for (int tau = 1; tau <= tauMax; ++tau) {
        double soma = 0.0;
        for (int j = 0; j < W; ++j) {
            double dif = (double)x[ini + j] - (double)x[ini + j + tau];
            soma += dif * dif;
        }
        d[tau] = soma;
    }
    dp.assign(tauMax + 1, 0.0);
    dp[0] = 1.0;
    double acum = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau) {
        acum += d[tau];
        dp[tau] = (acum > 0.0) ? d[tau] * tau / acum : 1.0;
    }
}

inline double candidato(const std::vector<double>& dp, int tauMin, int tauMax, double s, int fs) {
    int tauEst = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
        if (dp[tau] < s) { while (tau + 1 < tauMax && dp[tau + 1] < dp[tau]) tau++; tauEst = tau; break; }
    if (tauEst == -1) return 0.0;
    double mt = tauEst;
    if (tauEst > tauMin && tauEst < tauMax) {
        double s0 = dp[tauEst - 1], s1 = dp[tauEst], s2 = dp[tauEst + 1];
        double den = s0 - 2.0 * s1 + s2;
        if (den != 0.0) mt = tauEst + (s0 - s2) / (2.0 * den);
    }
    return (double)fs / mt;
}

inline int    binDe(double f) { return (int)std::lround(1200.0 * std::log2(f / FMIN) / RES_CENTS); }
inline double fDeBin(int b)   { return FMIN * std::pow(2.0, b * RES_CENTS / 1200.0); }

// Presets de tessitura (estilo "Vocal Range / Input Type" do Auto-Tune). Definem a
// FAIXA DE BUSCA do pitch [fmin, fmax] em Hz. Os limites seguem a classificação vocal
// PADRÃO (Fach/SATB), com fronteiras em notas reais — fonte: tabelas de tessitura
// (Bass E2–E4, Baritone G2–G4, Tenor C3–C5, Alto F3–F5, Mezzo A3–A5, Soprano C4–C6).
// Subir o fmin (vozes mais agudas) reduz o termo PSOLA da latência (fs/fmin). Devolve
// false se o nome não existir. Aceita sinônimos e os agrupamentos do Auto-Tune.
inline bool presetVoz(std::string nome, double& fmin, double& fmax) {
    for (auto& c : nome) c = (char)std::tolower((unsigned char)c);
    if      (nome == "baixo"    || nome == "bass")                       { fmin = 82;  fmax = 330;  }  // E2–E4
    else if (nome == "baritono" || nome == "baritone")                   { fmin = 98;  fmax = 392;  }  // G2–G4
    else if (nome == "tenor")                                            { fmin = 131; fmax = 523;  }  // C3–C5
    else if (nome == "contralto"|| nome == "alto")                       { fmin = 175; fmax = 698;  }  // F3–F5
    else if (nome == "mezzo"    || nome == "mezzosoprano")               { fmin = 220; fmax = 880;  }  // A3–A5
    else if (nome == "soprano")                                          { fmin = 262; fmax = 1047; }  // C4–C6
    // agrupamentos do Auto-Tune (combinam tessituras vizinhas):
    else if (nome == "lowmale"  || nome == "low" || nome == "baixomasc") { fmin = 82;  fmax = 392;  }  // bass+baritono
    else if (nome == "altotenor")                                        { fmin = 131; fmax = 698;  }  // tenor+alto
    else if (nome == "instrumento" || nome == "instrument" || nome == "amplo") { fmin = 50; fmax = 2000; }
    else return false;
    return true;
}

inline std::string hzParaNota(double f) {
    if (f <= 0.0) return "-";
    static const char* nomes[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    double midi = 69.0 + 12.0 * std::log2(f / 440.0);
    int m = (int)std::lround(midi);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%s%d", nomes[((m % 12) + 12) % 12], m / 12 - 1);
    return std::string(buf);
}

// Quais classes de nota (0=C ... 11=B) são permitidas como alvo. Padrão: todas.
inline bool g_permitida[12] = {true,true,true,true,true,true,true,true,true,true,true,true};

// Define a escala: "C","G","F#" (maior) | "Am","C#m" (menor) | "crom"/vazio (cromático).
inline void definirEscala(const char* txt) {
    for (int i = 0; i < 12; ++i) g_permitida[i] = true; // padrão cromático
    if (!txt) return;
    std::string s = txt;
    if (s.empty() || s == "crom" || s == "cromatico" || s == "cromatica") return;

    int pc = -1;
    switch (std::toupper(s[0])) {
        case 'C': pc = 0; break;  case 'D': pc = 2; break;  case 'E': pc = 4; break;
        case 'F': pc = 5; break;  case 'G': pc = 7; break;  case 'A': pc = 9; break;
        case 'B': pc = 11; break; default: return; // não reconheceu -> fica cromático
    }
    size_t p = 1;
    if (p < s.size() && s[p] == '#') { pc = (pc + 1) % 12; p++; }
    else if (p < s.size() && s[p] == 'b') { pc = (pc + 11) % 12; p++; }

    std::string resto = s.substr(p);
    for (auto& ch : resto) ch = std::tolower(ch);
    bool menor = (resto == "m" || resto == "min" || resto == "menor" || resto == "minor");

    int maior[7] = {0, 2, 4, 5, 7, 9, 11};   // escala maior
    int menorN[7] = {0, 2, 3, 5, 7, 8, 10};  // escala menor natural
    int* iv = menor ? menorN : maior;

    for (int i = 0; i < 12; ++i) g_permitida[i] = false;
    for (int i = 0; i < 7; ++i) g_permitida[(pc + iv[i]) % 12] = true;
}

// Acha, entre as notas permitidas pela escala atual (g_permitida), a mais
// próxima (em semitons) da frequência 'f'. Retorna o número MIDI dessa nota
// (ex.: 69 = A4 = 440 Hz). Usada por notaAlvo() e pela GUI do plugin (nota-alvo
// e desvio em cents "antes"/"depois" no medidor).
inline int notaMaisProximaMidi(double f) {
    double midi = 69.0 + 12.0 * std::log2(f / 440.0);
    int base = (int)std::lround(midi);
    int alvo = base; double menorDist = 1e9;
    for (int m = base - 7; m <= base + 7; ++m) {
        int pc = ((m % 12) + 12) % 12;
        if (g_permitida[pc]) {
            double dist = std::fabs(m - midi);
            if (dist < menorDist) { menorDist = dist; alvo = m; }
        }
    }
    return alvo;
}

// Nota-alvo: nota mais próxima da escala, com "forca" 0..1 e zona morta (tolCents).
inline double notaAlvo(double f, double forca, double tolCents) {
    double midi = 69.0 + 12.0 * std::log2(f / 440.0);
    int alvo = notaMaisProximaMidi(f);
    double errCents = (alvo - midi) * 100.0;
    double mag = std::fabs(errCents);
    double mov = (mag <= tolCents) ? 0.0
                                   : (errCents > 0 ? 1.0 : -1.0) * (mag - tolCents);
    double corrMidi = midi + (forca * mov) / 100.0;
    return 440.0 * std::pow(2.0, (corrMidi - 69.0) / 12.0);
}

// ---------------------------------------------------------------------------
//  Malha de correcao de altura, amostra a amostra.
//
//  Ate a Etapa 0 do plano (docs/plano-de-implementacao.md) este bloco estava
//  copiado LITERALMENTE em tres arquivos: offline_causal/main.cpp (caminho A),
//  offline_causal/autotune_rt.cpp (caminho B) e c1_streaming/autotune_stream.h
//  (caminho C1). Como a verificacao do projeto compara C1 contra o gold, uma
//  divergencia entre as copias quebraria essa comparacao EM SILENCIO -- o teste
//  passaria a comparar coisas diferentes sem acusar erro.
//
//  Comportamento (INALTERADO nesta etapa -- a Etapa 3 e que muda a matematica):
//    - alvo   = notaAlvo(f0, forca, tol), a nota da escala com zona morta;
//    - estado = filtro de 1 polo sobre o ALVO, em cents ("glide");
//    - no ataque de nota o estado nasce no proprio alvo, sem trajeto ate ela.
// ---------------------------------------------------------------------------
struct ParamsCorrecao {
    double forca    = 1.0;   // 0..1: fracao do desvio que e corrigida
    double tolCents = 0.0;   // zona morta (cents) ao redor da nota-alvo
    double glideMs  = 0.0;   // constante de tempo do deslize ate o alvo
};

class CorretorAltura {
public:
    void prepare(double fsHz) { fs = fsHz; reset(); }
    void reset() { estado = 0.0; tinhaNota = false; }

    // Devolve o pitch-alvo em Hz para uma amostra cujo F0 detectado e f0Hz.
    // f0Hz <= 0 marca trecho nao-vozeado: devolve 0 e rearma o ataque.
    double proxima(double f0Hz, const ParamsCorrecao& p) {
        if (f0Hz <= 0.0) { tinhaNota = false; return 0.0; }
        const double alvoHz    = notaAlvo(f0Hz, p.forca, p.tolCents);
        const double alvoCents = 1200.0 * std::log2(alvoHz / FMIN);
        const double tau       = p.glideMs / 1000.0;
        const double alpha     = (tau > 0.0) ? std::exp(-1.0 / (tau * fs)) : 0.0;
        estado    = tinhaNota ? (alpha * estado + (1.0 - alpha) * alvoCents) : alvoCents;
        tinhaNota = true;
        return FMIN * std::pow(2.0, estado / 1200.0);
    }

private:
    double fs        = 44100.0;
    double estado    = 0.0;    // pitch-alvo suavizado, em cents acima de FMIN
    bool   tinhaNota = false;  // false = proxima amostra vozeada e um ataque
};

// Grava um WAV mono em 16-bit PCM (formato universal, sem glitch de player).
inline bool gravarWav16(const char* caminho, const std::vector<float>& s, unsigned taxa) {
    std::vector<drwav_int16> pcm(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        double v = s[i]; if (v > 1.0) v = 1.0; if (v < -1.0) v = -1.0;
        pcm[i] = (drwav_int16)std::lround(v * 32767.0);
    }
    drwav_data_format fmt;
    fmt.container = drwav_container_riff; fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.channels = 1; fmt.sampleRate = taxa; fmt.bitsPerSample = 16;
    drwav wav;
    if (!drwav_init_file_write(&wav, caminho, &fmt, nullptr)) return false;
    drwav_write_pcm_frames(&wav, pcm.size(), pcm.data());
    drwav_uninit(&wav);
    return true;
}

// ----------------------------------------------------------------------------
//  TD-PSOLA: dada a entrada x, a trajetória de pitch REAL por amostra (f0samp) e a
//  de pitch-ALVO por amostra (foutSamp, já com força+tolerância+glide aplicados),
//  sintetiza a saída corrigida. É LOCAL (look-ahead ~1 período), então serve tanto
//  ao caminho offline quanto ao causal/streaming. Com foutSamp==f0samp (beta=1) a
//  saída == entrada (identidade). Passos: marcas por correlação (preservação de
//  fase) -> reamostragem com PRESERVAÇÃO DE DURAÇÃO -> reconstrução por cobertura.
// ----------------------------------------------------------------------------
inline std::vector<float> psolaSintetiza(const std::vector<float>& x, long long N,
        const std::vector<float>& f0samp, const std::vector<float>& foutSamp, int fs) {
    // 5a. Marcas de análise (1 por período, alinhadas por correlação à anterior).
    std::vector<long long> marcas;
    {
        long long nn = 0;
        while (nn < N) {
            while (nn < N && f0samp[nn] <= 0) nn++;
            if (nn >= N) break;
            long long hi0 = std::min(N - 1, nn + (long long)(fs / f0samp[nn]));
            long long m = nn;
            for (long long k = nn; k <= hi0; ++k) if (std::fabs(x[k]) > std::fabs(x[m])) m = k;
            marcas.push_back(m);
            while (m < N && f0samp[m] > 0) {
                int T = std::max(2, (int)std::llround(fs / f0samp[m]));
                long long cand = m + T;
                if (cand >= N || f0samp[cand] <= 0) break;
                int busca = std::max(1, T / 4), Wc = T / 2;
                double melhor = -1e300; long long pos = cand;
                for (int off = -busca; off <= busca; ++off) {
                    long long c = cand + off;
                    if (c - Wc < 0 || c + Wc >= N || m - Wc < 0 || m + Wc >= N) continue;
                    double corr = 0;
                    for (int k = -Wc; k <= Wc; ++k) corr += (double)x[m + k] * (double)x[c + k];
                    if (corr > melhor) { melhor = corr; pos = c; }
                }
                m = pos;
                marcas.push_back(m);
            }
            // pula o resto da nota (evita marcas a 1 amostra no fim -> spikes)
            nn = m + 1;
            while (nn < N && f0samp[nn] > 0) nn++;
        }
    }

    // 5b. Síntese OLA com preservação de duração (reamostragem das marcas por região).
    std::vector<double> y(N, 0.0), wsum(N, 0.0);
    {
        auto betaDe = [&](size_t idx) {
            long long a = marcas[idx];
            double fsrc = f0samp[a];
            double falvo = (foutSamp[a] > 0) ? (double)foutSamp[a] : fsrc;
            return (fsrc > 0) ? falvo / fsrc : 1.0;
        };
        size_t i0 = 0, NM = marcas.size();
        while (i0 < NM) {
            size_t i1 = i0;
            while (i1 + 1 < NM && f0samp[(marcas[i1] + marcas[i1 + 1]) / 2] > 0) i1++;
            int M = (int)(i1 - i0 + 1);
            if (M >= 2) {
                std::vector<double> cum(M, 0.0);
                for (int k = 1; k < M; ++k)
                    cum[k] = cum[k - 1] + 0.5 * (betaDe(i0 + k - 1) + betaDe(i0 + k));
                double total = cum[M - 1];
                // Posições de síntese em PASSOS INTEIROS de beta (tc = 0,1,2,...),
                // ANCORADAS no início da região (cum[0]=0). Cada grão j cai onde a
                // soma acumulada 'cum' atinge 'j' — e isso depende SÓ das marcas
                // ATÉ ali, não de onde a região termina (cum[M-1]=total). Logo o
                // espaçamento é INVARIANTE A TRUNCAMENTO: re-sintetizar a mesma
                // região com extent maior/menor (ex.: nota ainda em curso no
                // streaming) não desloca os grãos já posicionados. Antes era
                // 'tc = j*total/(K-1)' com 'K=round(total)+1', que forçava o
                // último grão exatamente em marcas[i1] (o FIM da região) e, com
                // isso, fazia TODO grão depender de 'total' -> drift de fase no
                // PSOLA online (a região era truncada em 'decis' a cada bloco).
                // O resto (<1 período) sobra no fim e é coberto pelo blend seco.
                int K = std::max(2, (int)std::floor(total) + 1);
                for (int j = 0; j < K; ++j) {
                    double tc = (double)j;
                    int k = 1; while (k < M && cum[k] < tc) k++;
                    double fi;
                    if (k >= M) fi = M - 1;
                    else { double d = cum[k] - cum[k - 1];
                           fi = (k - 1) + (d > 1e-9 ? (tc - cum[k - 1]) / d : 0.0); }
                    int il = (int)std::floor(fi); if (il > M - 1) il = M - 1;
                    int ir = std::min(il + 1, M - 1); double frac = fi - il;
                    int ia = i0 + (int)std::llround(fi);
                    long long a  = marcas[ia];
                    double sPos  = (double)marcas[i0 + il] * (1.0 - frac)
                                 + (double)marcas[i0 + ir] * frac;
                    long long si = (long long)std::llround(sPos);
                    long long Tana;
                    if (ia + 1 <= (int)i1)      Tana = marcas[ia + 1] - marcas[ia];
                    else if (ia - 1 >= (int)i0) Tana = marcas[ia] - marcas[ia - 1];
                    else                        Tana = (f0samp[a] > 0) ? (long long)std::llround(fs / f0samp[a]) : 2;
                    int L = (int)std::max(2LL, Tana);
                    for (int kk = -L; kk <= L; ++kk) {
                        long long ai = a + kk, yi = si + kk;
                        if (ai >= 0 && ai < N && yi >= 0 && yi < N) {
                            double w = 0.5 * (1.0 - std::cos(PI * (kk + L) / (double)L));
                            y[yi]   += (double)x[ai] * w;
                            wsum[yi] += w;
                        }
                    }
                }
            }
            i0 = i1 + 1;
        }
    }

    // 5c. Reconstrução por cobertura (funde wet->seco pela rampa Hann; sem clique).
    std::vector<float> out(N);
    double pico = 0.0;
    for (long long i = 0; i < N; ++i) {
        double wet = (wsum[i] > EPS) ? (y[i] / wsum[i]) : (double)x[i];
        double w = wsum[i]; if (w > 1.0) w = 1.0; if (w < 0.0) w = 0.0;
        double v = w * wet + (1.0 - w) * (double)x[i];
        out[i] = (float)v;
        pico = std::max(pico, std::fabs(v));
    }
    if (pico > 1.0) for (long long i = 0; i < N; ++i) out[i] = (float)(out[i] / pico);
    return out;
}

#endif // DSP_H
