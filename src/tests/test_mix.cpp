// ---------------------------------------------------------------------------
//  test_mix.cpp — verificacao da mistura seco/molhado (Etapa 2 do plano).
//
//  A Etapa 2 removeu a "forca" e pos no lugar um Mix seco/molhado. A troca tem
//  dois riscos que so um teste pega:
//
//    1) EXATIDAO NOS EXTREMOS — mix=0 tem de devolver o seco BIT A BIT e mix=1 o
//       molhado BIT A BIT. Se os extremos passassem por multiplicacao em ponto
//       flutuante, o bypass deixaria de ser bypass (por poucos LSB, mas o teste
//       de identidade do projeto e' exato, e passaria a falhar por ruido).
//
//    2) ALINHAMENTO NO STREAMING — o molhado sai atrasado da latencia do motor.
//       Se o seco NAO for atrasado da mesma medida, a mistura soma o sinal a uma
//       copia deslocada de si mesmo: um filtro-pente. Ele nao quebra nenhum
//       teste de "roda sem travar" e some do radar com facilidade, porque em
//       mix=0 e mix=1 o sinal e' correto -- so as posicoes INTERMEDIARIAS ficam
//       erradas. Por isso a Secao 3 verifica o atraso diretamente.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_mix.cpp -o test_mix
//  Rodar:     ./test_mix          (0 = tudo certo, 1 = falhou)
// ---------------------------------------------------------------------------
#include "../c1_streaming/autotune_stream.h"
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

// Roda o motor sobre 'x' em blocos de 'block' amostras e devolve a saida.
static std::vector<float> rodar(const std::vector<float>& x, int fs, double mix, int block) {
    StreamParams p; p.mix = mix;
    AutotuneStream eng; eng.prepare(fs, p);
    std::vector<float> out(x.size(), 0.0f);
    for (size_t off = 0; off < x.size(); off += (size_t)block) {
        int nb = (int)std::min((size_t)block, x.size() - off);
        eng.process(&x[off], &out[off], nb);
    }
    return out;
}

int main() {
    std::printf("== 1. misturar(): exatidao nos extremos ==\n");
    // Valores escolhidos para que a media NAO seja representavel de forma trivial:
    // se algum extremo passasse pela multiplicacao, o bit final mudaria.
    const float seco = 0.1234567f, molhado = -0.7654321f;
    checar(misturar(seco, molhado, 1.0) == molhado, "mix=1.0 devolve o molhado bit a bit");
    checar(misturar(seco, molhado, 0.0) == seco,    "mix=0.0 devolve o seco bit a bit");
    // Fora da faixa: o motor ja limita, mas a funcao nao pode extrapolar sozinha.
    checar(misturar(seco, molhado, 2.0) == molhado, "mix>1 satura no molhado (nao extrapola)");
    checar(misturar(seco, molhado, -1.0) == seco,   "mix<0 satura no seco (nao extrapola)");

    std::printf("\n== 2. misturar(): cruzamento linear no meio ==\n");
    {
        const float m = misturar(seco, molhado, 0.5);
        const float esperado = (float)(0.5 * (double)molhado + 0.5 * (double)seco);
        checar(m == esperado, "mix=0.5 e' a media exata (%.7f)", m);
        // Monotonia: caminhar de 0 a 1 nao pode dar solavanco nem inverter.
        bool mono = true; float ant = misturar(seco, molhado, 0.0);
        for (int k = 1; k <= 100; ++k) {
            float v = misturar(seco, molhado, k / 100.0);
            if (v > ant) { mono = false; break; }   // molhado < seco aqui, logo desce
            ant = v;
        }
        checar(mono, "a varredura de 0 a 1 e' monotona (sem salto nos extremos)");
    }

    std::printf("\n== 3. streaming: mix=0 e' a entrada ATRASADA da latencia ==\n");
    {
        const int fs = 44100;
        const int N  = fs;                       // 1 s
        std::vector<float> x((size_t)N);
        // Sinal vozeado sintetico: 200 Hz com harmonicos e envelope, para que o
        // pYIN encontre pitch de verdade e o PSOLA rode (e nao um caso trivial).
        for (int i = 0; i < N; ++i) {
            double t = (double)i / fs, env = 0.5 * (1.0 - std::cos(2.0 * M_PI * t));
            x[(size_t)i] = (float)(env * (0.6 * std::sin(2*M_PI*200*t)
                                        + 0.3 * std::sin(2*M_PI*400*t)
                                        + 0.1 * std::sin(2*M_PI*600*t)));
        }

        StreamParams p0; p0.mix = 0.0;
        AutotuneStream sonda; sonda.prepare(fs, p0);
        const int lat = sonda.getLatencySamples();

        std::vector<float> y = rodar(x, fs, 0.0, 128);

        // A partir de 'lat', cada amostra de saida tem de ser EXATAMENTE a
        // amostra de entrada 'lat' posicoes atras. Antes de 'lat', silencio.
        long long difs = 0, primeira = -1;
        for (int i = lat; i < N; ++i)
            if (y[(size_t)i] != x[(size_t)(i - lat)]) { if (primeira < 0) primeira = i; ++difs; }
        checar(difs == 0, "%d amostras conferem apos o priming (lat=%d)%s",
               N - lat, lat, difs ? "" : "");
        if (difs) std::printf("        %lld divergencias, a 1a em i=%lld\n", difs, primeira);

        long long naoZero = 0;
        for (int i = 0; i < lat; ++i) if (y[(size_t)i] != 0.0f) ++naoZero;
        checar(naoZero == 0, "as %d amostras do priming sao silencio", lat);
    }

    std::printf("\n== 4. streaming: o atraso NAO depende do tamanho do bloco ==\n");
    {
        const int fs = 44100, N = fs / 2;
        std::vector<float> x((size_t)N);
        for (int i = 0; i < N; ++i) {
            double t = (double)i / fs;
            x[(size_t)i] = (float)(0.5 * std::sin(2*M_PI*220*t) + 0.2 * std::sin(2*M_PI*440*t));
        }
        // Se o seco fosse lido "de agora" em vez de pelo indice absoluto, o
        // resultado passaria a depender de onde caem as bordas dos blocos.
        //
        // ATENCAO ao que esta secao NAO prova: com mix=0 o caminho MOLHADO nem
        // e' lido, entao isto verifica a invariancia do caminho SECO apenas. A
        // invariancia do molhado e' outra questao -- e ha um achado aberto de
        // que ela falha em block=512 (ver docs/execucao-do-plano.md, Etapa 0).
        // Nao tomar este "ok" como prova de que aquele problema sumiu.
        std::vector<float> ref = rodar(x, fs, 0.0, 64);
        for (int b : {32, 128, 256, 512, 1024}) {
            std::vector<float> y = rodar(x, fs, 0.0, b);
            long long difs = 0;
            for (int i = 0; i < N; ++i) if (y[(size_t)i] != ref[(size_t)i]) ++difs;
            checar(difs == 0, "block=%-4d identico ao block=64", b);
        }
    }

    std::printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK",
                falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
