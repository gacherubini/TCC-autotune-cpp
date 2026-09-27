// ---------------------------------------------------------------------------
//  test_ponteiro.cpp — verificacao do motor v3 (ponteiro movel), isolado do
//  streaming (Etapa 6 do diario; spec em docs/especificacao-v3-ponteiro.md).
//
//  O que so um teste pega:
//    1) IDENTIDADE EXATA — com beta = 1 (ou sem voz) o motor tem de devolver a
//       entrada atrasada de MARGEM amostras BIT A BIT. E' a condicao para que
//       tol=600 e mix=0 continuem sendo os dois caminhos de identidade do
//       projeto tambem no motor novo.
//    2) A NOTA SAI CERTA — com beta = 1,05 a frequencia medida na saida tem de
//       subir 5 %. Isso verifica que o salto de um periodo nao "devolve" o
//       deslocamento (um erro classico: saltar de T e ler a T/beta).
//    3) SEM CLIQUE — o maior degrau amostra-a-amostra da saida nao pode passar
//       de 1,5x o da entrada. Em senoide o salto de um periodo e' invisivel;
//       se aparecer degrau, a emenda esta fora de fase.
//    4) DISTANCIA LIMITADA — a parte variavel da latencia fica em
//       [MARGEM-1, MARGEM+T+1]. Se passar disso, o teste de distancia esta errado.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_ponteiro.cpp -o test_ponteiro
//  Rodar:     ./test_ponteiro      (0 = tudo certo, 1 = falhou)
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
    std::vprintf(fmt, ap); std::printf("\n");
    va_end(ap);
}

static const int FS = 44100;

static std::vector<float> senoide(double f, double seg, double amp = 0.5) {
    std::vector<float> x((size_t)(seg * FS));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float)(amp * std::sin(2 * PI * f * i / FS));
    return x;
}

// Frequencia media por cruzamentos de zero ascendentes em [ini, fim).
static double freqPorCruzamentos(const std::vector<float>& y, size_t ini, size_t fim) {
    long long n = 0; size_t primeiro = 0, ultimo = 0;
    for (size_t i = ini + 1; i < fim; ++i)
        if (y[i - 1] <= 0.0f && y[i] > 0.0f) { if (n == 0) primeiro = i; ultimo = i; ++n; }
    if (n < 2) return 0.0;
    return (double)(n - 1) * FS / (double)(ultimo - primeiro);
}

static double maiorDegrau(const std::vector<float>& y, size_t ini, size_t fim) {
    double m = 0; for (size_t i = ini + 1; i < fim; ++i) m = std::max(m, (double)std::fabs(y[i] - y[i - 1]));
    return m;
}

// Roda o motor com f0/fAlvo constantes (ou funcao do indice) e devolve a saida.
template <class F0, class FA>
static std::vector<float> rodar(MotorPonteiro& m, const std::vector<float>& x, F0 f0, FA fa) {
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = m.processar(x[i], f0(i), fa(i));
    return y;
}

int main() {
    const int M = MotorPonteiro::MARGEM;

    std::printf("== 1. beta = 1: identidade atrasada de MARGEM, bit a bit ==\n");
    {
        MotorPonteiro m; m.prepare(FS, 80.0);
        auto x = senoide(220.0, 1.0);
        auto y = rodar(m, x, [](size_t){ return 220.0; }, [](size_t){ return 220.0; });
        bool ok = true;
        for (size_t i = (size_t)M; i < x.size(); ++i) if (y[i] != x[i - M]) { ok = false; break; }
        checar(m.latencia() == M, "latencia() == MARGEM (%d)", M);
        checar(ok, "y[n] == x[n-%d] em todas as amostras", M);
        checar(m.saltos() == 0, "nenhum salto com beta = 1");
    }

    std::printf("== 2. sem voz (f0 = 0): identidade atrasada ==\n");
    {
        MotorPonteiro m; m.prepare(FS, 80.0);
        auto x = senoide(220.0, 1.0);
        auto y = rodar(m, x, [](size_t){ return 0.0; }, [](size_t){ return 0.0; });
        bool ok = true;
        for (size_t i = (size_t)M; i < x.size(); ++i) if (y[i] != x[i - M]) { ok = false; break; }
        checar(ok, "y[n] == x[n-%d] sem voz", M);
        checar(m.saltos() == 0, "nenhum salto sem voz");
    }

    auto casoBeta = [&](double f0, double beta, const char* nome) {
        std::printf("== %s: f0 = %.0f Hz, beta = %.2f ==\n", nome, f0, beta);
        MotorPonteiro m; m.prepare(FS, 80.0);
        auto x = senoide(f0, 2.0);
        auto y = rodar(m, x, [=](size_t){ return f0; }, [=](size_t){ return f0 * beta; });
        const size_t ini = FS / 4, fim = x.size() - FS / 4;   // regiao estavel
        const double fMed = freqPorCruzamentos(y, ini, fim), fEsp = f0 * beta;
        checar(std::fabs(fMed - fEsp) / fEsp < 0.005, "frequencia medida %.2f Hz, esperada %.2f (tol 0,5%%)", fMed, fEsp);
        checar(m.saltos() > 0, "houve saltos (%lld)", m.saltos());
        const double dIn = maiorDegrau(x, ini, fim), dOut = maiorDegrau(y, ini, fim);
        checar(dOut <= 1.5 * dIn, "maior degrau saida %.4f <= 1,5 x entrada %.4f (sem clique)", dOut, dIn);
        const double T = FS / f0;
        checar(m.distMax() <= M + T + 1.0, "distMax %.1f <= MARGEM + T + 1 = %.1f", m.distMax(), M + T + 1.0);
        checar(m.distMedia() >= M - 1.0, "distMedia %.1f >= MARGEM - 1", m.distMedia());
    };
    casoBeta(220.0, 1.05, "3. sobe");
    casoBeta(220.0, 0.95, "4. desce");

    std::printf("== 5. salto de nota no meio (220 -> 330 Hz), beta = 1,03 ==\n");
    {
        MotorPonteiro m; m.prepare(FS, 80.0);
        const size_t meio = FS;                      // 1 s de cada
        std::vector<float> x(2 * FS);
        for (size_t i = 0; i < x.size(); ++i) {
            double f = (i < meio) ? 220.0 : 330.0;
            // fase continua na troca, para que o degrau (se houver) seja do motor
            static double fase = 0; fase += 2 * PI * f / FS; x[i] = (float)(0.5 * std::sin(fase));
        }
        auto y = rodar(m, x, [=](size_t i){ return i < meio ? 220.0 : 330.0; },
                             [=](size_t i){ return (i < meio ? 220.0 : 330.0) * 1.03; });
        const double dIn = maiorDegrau(x, FS / 4, x.size() - FS / 4);
        const double dOut = maiorDegrau(y, FS / 4, x.size() - FS / 4);
        checar(dOut <= 1.5 * dIn, "maior degrau %.4f <= 1,5 x %.4f na troca de nota", dOut, dIn);
        checar(m.distMax() <= M + FS / 220.0 + 1.0, "distMax %.1f respeita o T maior (220 Hz)", m.distMax());
        const double f2 = freqPorCruzamentos(y, meio + FS / 4, x.size() - FS / 8);
        checar(std::fabs(f2 - 330.0 * 1.03) / (330.0 * 1.03) < 0.005, "segunda nota sai em %.2f Hz (esp. %.2f)", f2, 330.0 * 1.03);
    }

    std::printf("\n%s (%d falha(s))\n", falhas ? "FALHOU" : "TUDO CERTO", falhas);
    return falhas ? 1 : 0;
}
