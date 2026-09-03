// ---------------------------------------------------------------------------
//  test_retune.cpp — verificacao da cadeia de correcao da Etapa 3.
//
//  A Etapa 3 trocou a matematica do CorretorAltura: o filtro deixou de agir
//  sobre o ALVO e passou a agir sobre a CORRECAO, via dois estados:
//
//      outCents = LP(alvo) + k*(real - LP(real))
//
//  Isso e' uma generalizacao, nao uma troca -- e este teste existe para provar
//  isso, e nao para acreditar. Quatro afirmacoes que o plano fez e que so um
//  teste sustenta:
//
//    1) k = 0 (+ ataqueNoAlvo) reproduz a Etapa 2 AMOSTRA A AMOSTRA. Se isso
//       falhar, a fusao do Glide com o Retune Speed perdeu alguma coisa.
//    2) No ataque a nota nasce na altura REAL do cantor, seja qual for k.
//    3) k = 1 e' algebricamente "filtro sobre a correcao" (a forma da patente).
//    4) O ganho do vibrato preservado segue G(f_v) = f_v/sqrt(f_v^2 + f_c^2),
//       com f_c = 1/(2*pi*tau) -- ou seja, o compromisso "corrigir rapido come
//       vibrato" e' quantitativo e previsivel, nao folclore.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_retune.cpp -o test_retune
//  Rodar:     ./test_retune          (0 = tudo certo, 1 = falhou)
// ---------------------------------------------------------------------------
#include "../core/dsp.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>

static int falhas = 0;

static void checar(bool ok, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (!ok) ++falhas;
    std::printf("  %s  ", ok ? "ok  " : "FALHA");
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}

// ---------------------------------------------------------------------------
//  A malha da ETAPA 2, reimplementada aqui literalmente. Nao e' duplicacao
//  descuidada: e' o oraculo do teste 1. Ela precisa ficar CONGELADA neste
//  arquivo mesmo que o dsp.h evolua -- e' contra este texto que a nao-regressao
//  e' medida. Copiada de dsp.h como estava no commit 63ee0f0 (Etapa 2).
// ---------------------------------------------------------------------------
struct CorretorEtapa2 {
    double fs = 44100.0, estado = 0.0; bool tinhaNota = false;
    double proxima(double f0Hz, double tolCents, double glideMs) {
        if (f0Hz <= 0.0) { tinhaNota = false; return 0.0; }
        const double alvoHz    = notaAlvo(f0Hz, tolCents);
        const double alvoCents = 1200.0 * std::log2(alvoHz / FMIN);
        const double tau       = glideMs / 1000.0;
        const double alpha     = (tau > 0.0) ? std::exp(-1.0 / (tau * fs)) : 0.0;
        estado    = tinhaNota ? (alpha * estado + (1.0 - alpha) * alvoCents) : alvoCents;
        tinhaNota = true;
        return FMIN * std::pow(2.0, estado / 1200.0);
    }
};

// Trilha de F0 sintetica com tudo que estressa a malha: silencio, ataques,
// vibrato, deriva lenta de afinacao e um salto de nota.
static std::vector<double> trilhaF0(int fs, int N) {
    std::vector<double> f0((size_t)N, 0.0);
    for (int i = 0; i < N; ++i) {
        const double t = (double)i / fs;
        if (t < 0.10 || (t > 0.95 && t < 1.10) || t > 2.60) continue;   // nao-vozeado
        const double base   = (t < 1.10) ? 220.0 : 261.63;              // A3 -> C4
        const double deriva = (t < 1.10) ? (-30.0 + 20.0 * t) : (18.0 - 6.0 * t);
        const double vib    = 22.0 * std::sin(2.0 * PI * 5.5 * t);
        f0[(size_t)i] = base * std::pow(2.0, (deriva + vib) / 1200.0);
    }
    return f0;
}

int main() {
    const int fs = 44100, N = 3 * fs;
    const std::vector<double> f0 = trilhaF0(fs, N);
    definirEscala("crom");

    std::printf("== 1. k=0 + ataqueNoAlvo reproduz a Etapa 2 amostra a amostra ==\n");
    for (double glide : {0.0, 15.0, 40.0, 120.0}) {
        ParamsCorrecao p; p.tolCents = 15.0; p.retuneMs = glide;
        p.vibrato = 0.0; p.ataqueNoAlvo = true; p.semHisterese = true;
        CorretorAltura novo; novo.prepare(fs);
        CorretorEtapa2 velho; velho.fs = fs;

        long long difs = 0; double maiorDif = 0.0; int primeira = -1;
        for (int i = 0; i < N; ++i) {
            const double a = novo.proxima(f0[(size_t)i], p);
            const double b = velho.proxima(f0[(size_t)i], 15.0, glide);
            if (a != b) { if (primeira < 0) primeira = i; ++difs;
                          maiorDif = std::fmax(maiorDif, std::fabs(a - b)); }
        }
        checar(difs == 0, "glide=%-6.1f ms -> %d amostras identicas%s",
               glide, N, difs ? "" : " (bit a bit)");
        if (difs) std::printf("        %lld divergencias, 1a em i=%d, maior |dif| = %.3e Hz\n",
                              difs, primeira, maiorDif);
    }

    std::printf("\n== 2. no ataque a nota nasce na altura REAL do cantor ==\n");
    {
        // f0 escolhido bem fora da nota (A3=220 Hz): 45 cents acima. Se a nota
        // nascesse no alvo, a 1a amostra sairia 220 Hz.
        const double fAtaque = 220.0 * std::pow(2.0, 45.0 / 1200.0);
        for (double k : {0.0, 1.0, 2.0}) {
            ParamsCorrecao p; p.tolCents = 0.0; p.retuneMs = 40.0; p.vibrato = k;
            CorretorAltura c; c.prepare(fs);
            const double primeiro = c.proxima(fAtaque, p);
            checar(std::fabs(primeiro - fAtaque) < 1e-9,
                   "k=%.1f -> 1a amostra = %.4f Hz (cantor em %.4f)", k, primeiro, fAtaque);
        }
        // E com ataqueNoAlvo a nota nasce AFINADA -- o comportamento da Etapa 2.
        ParamsCorrecao p; p.tolCents = 0.0; p.retuneMs = 40.0;
        p.vibrato = 0.0; p.ataqueNoAlvo = true;
        CorretorAltura c; c.prepare(fs);
        const double primeiro = c.proxima(fAtaque, p);
        checar(std::fabs(primeiro - 220.0) < 1e-9,
               "ataqueNoAlvo -> 1a amostra = %.4f Hz (a nota, nao o cantor)", primeiro);
    }

    std::printf("\n== 3. k=1 e' o filtro sobre a CORRECAO (forma da patente) ==\n");
    {
        // Identidade a verificar:  LP(alvo) + real - LP(real) == real + LP(alvo-real)
        // O lado direito e' calculado aqui por um filtro so, sobre a correcao.
        ParamsCorrecao p; p.tolCents = 0.0; p.retuneMs = 40.0; p.vibrato = 1.0;
        CorretorAltura c; c.prepare(fs);
        const double alpha = std::exp(-1.0 / (0.040 * fs));
        double lpCorr = 0.0; bool tinha = false;
        double maiorDif = 0.0;
        for (int i = 0; i < N; ++i) {
            const double f = f0[(size_t)i];
            const double got = c.proxima(f, p);
            if (f <= 0.0) { tinha = false; continue; }
            const double real = 1200.0 * std::log2(f / FMIN);
            const double alvo = 1200.0 * std::log2(notaAlvo(f, 0.0) / FMIN);
            if (!tinha) { lpCorr = 0.0; tinha = true; }         // ataque: correcao = 0
            else        { lpCorr = alpha * lpCorr + (1.0 - alpha) * (alvo - real); }
            const double esperado = FMIN * std::pow(2.0, (real + lpCorr) / 1200.0);
            maiorDif = std::fmax(maiorDif, std::fabs(got - esperado));
        }
        // Tolerancia numerica: as duas formas somam em ordens diferentes.
        checar(maiorDif < 1e-6, "maior divergencia = %.3e Hz (duas formulacoes)", maiorDif);
    }

    std::printf("\n== 4. ganho do vibrato: G(f_v) = f_v/sqrt(f_v^2 + f_c^2) ==\n");
    {
        // Vibrato puro em torno de A3, dentro de meio semitom -> alvo constante.
        const double fv = 5.5, amp = 30.0;      // 5,5 Hz, +-30 cents
        auto amplitudeSaida = [&](double tauMs, double k) {
            ParamsCorrecao p; p.tolCents = 0.0; p.retuneMs = tauMs; p.vibrato = k;
            CorretorAltura c; c.prepare(fs);
            double lo = 1e9, hi = -1e9;
            for (int i = 0; i < N; ++i) {
                const double t = (double)i / fs;
                const double cents = amp * std::sin(2.0 * PI * fv * t);
                const double y = c.proxima(220.0 * std::pow(2.0, cents / 1200.0), p);
                if (t < 1.5) continue;          // descarta o transitorio do filtro
                const double yc = 1200.0 * std::log2(y / 220.0);
                lo = std::fmin(lo, yc); hi = std::fmax(hi, yc);
            }
            return (hi - lo) / 2.0;             // amplitude em cents
        };
        for (double tauMs : {10.0, 25.0, 100.0}) {
            const double fc = 1.0 / (2.0 * PI * (tauMs / 1000.0));
            const double G  = fv / std::sqrt(fv * fv + fc * fc);
            const double medido = amplitudeSaida(tauMs, 1.0);
            const double teorico = amp * G;
            const double erro = std::fabs(medido - teorico) / teorico;
            checar(erro < 0.03, "tau=%-5.0f ms (f_c=%4.1f Hz): %5.1f ct medido vs %5.1f teorico (%.1f%%)",
                   tauMs, fc, medido, teorico, 100.0 * erro);
        }
        // k escala o vibrato linearmente: 0 mata, 2 dobra.
        const double a1 = amplitudeSaida(25.0, 1.0);
        checar(amplitudeSaida(25.0, 0.0) < 0.02 * a1, "k=0 remove o vibrato");
        checar(std::fabs(amplitudeSaida(25.0, 2.0) - 2.0 * a1) < 0.02 * a1,
               "k=2 dobra o vibrato (linearidade em k)");
    }

    std::printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK",
                falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
