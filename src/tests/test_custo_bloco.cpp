// ---------------------------------------------------------------------------
//  test_custo_bloco.cpp — o CUSTO POR BLOCO do motor de sintese.
//
//  Nao havia precedente disto no repositorio. Os scripts de python/ medem taxa
//  AGREGADA (xRT sobre o arquivo inteiro), e taxa agregada esconde exatamente o
//  defeito que produz o estalo: um motor que roda a 8x tempo real na media pode
//  estourar o orcamento do host em um bloco a cada cinco, e o que o cantor ouve
//  e' o estouro, nao a media.
//
//  A Causa 3 do spec (docs/spec-encaixe-e-estabilidade.md §2) e' que o custo por
//  bloco do TD-PSOLA CRESCE com a duracao da nota. A sintese incremental e' uma
//  funcao pura que redescobre a cadeia de marcas do zero a cada chamada,
//  recuando ate o inicio da regiao vozeada para achar a mesma ancora que a
//  versao em lote usaria. Numa nota de 3 segundos, no instante t = 3 s ela refaz
//  3 segundos de marcas e graos para entregar 128 amostras.
//
//  ---------------------------------------------------------------------------
//  ESTE E' O UNICO TESTE DA SUITE SENSIVEL A MAQUINA, e o desenho leva isso em
//  conta em tres pontos:
//
//   1) A ASERCAO E' SOBRE RAZAO, NAO SOBRE MILISSEGUNDOS. "O bloco no fim da
//      nota nao custa mais que o do inicio" e' uma propriedade ESTRUTURAL --
//      vale numa maquina rapida e numa lenta, num laptop em bateria e num
//      servidor de CI carregado. Um limiar absoluto em ms ("nunca passar de
//      2,90 ms") pareceria mais direto e seria instavel: ele mede a maquina,
//      nao o algoritmo.
//
//   2) O NUMERO ABSOLUTO E' DIAGNOSTICO, NUNCA CRITERIO DE FALHA. Ele e'
//      impresso porque e' o que o texto do TCC cita, e porque e' ele que diz se
//      o motor cabe no orcamento de 2,90 ms de um bloco de 128 a 44,1 kHz. Mas
//      nenhum 'checar' olha para ele.
//
//   3) A ESTATISTICA E' PERCENTIL, NAO MAXIMO. So metade dos blocos faz
//      trabalho (com nHop = 256 e bloco = 128, a sintese avanca a cada dois
//      blocos; os outros saem cedo). A mediana cairia bem em cima dessa
//      fronteira e oscilaria. O percentil 90 fica solidamente entre os blocos
//      que trabalham e ignora um pico isolado de escalonamento do sistema
//      operacional. O maximo continua sendo impresso como diagnostico.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_custo_bloco.cpp -o test_custo_bloco
//  Rodar:     ./test_custo_bloco      (0 = tudo certo, 1 = falhou)
// ---------------------------------------------------------------------------
#define DR_WAV_IMPLEMENTATION
#include "../c1_streaming/autotune_stream.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>

static int falhas = 0;

static void checar(bool ok, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (!ok) ++falhas;
    std::printf("  %s  ", ok ? "ok  " : "FALHA");
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}

// Diagnostico: sai no relatorio, nao entra em nenhuma asercao.
static void diag(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    std::printf("        . ");
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}

// ---------------------------------------------------------------------------
//  Sinal de teste: silencio, uma nota LONGA sustentada, silencio.
//
//  A nota longa e' o ponto inteiro do arquivo. O custo do PSOLA incremental so
//  cresce DENTRO de uma regiao vozeada -- ele volta ao normal quando a nota
//  acaba, porque a busca da ancora para no primeiro quadro nao-vozeado. Um
//  sinal de notas curtas nao exibiria o defeito, e um teste montado sobre ele
//  passaria com o motor quebrado.
//
//  Perfil harmonico e envelope sao os mesmos de test_deteccao.cpp, pela mesma
//  razao: senoide pura nao produz a cadeia de marcas que o PSOLA percorre.
// ---------------------------------------------------------------------------
static std::vector<float> sinalNotaLonga(int fs, double segNota, double f0 = 220.0,
                                         double segSilencio = 0.25) {
    const int nSil  = (int)(segSilencio * fs);
    const int nNota = (int)(segNota * fs);
    std::vector<float> x((size_t)(2 * nSil + nNota), 0.0f);
    const int nHarm = std::min(24, std::max(1, (int)((0.45 * fs) / f0)));
    for (int i = 0; i < nNota; ++i) {
        const double t = (double)i / fs;
        double s = 0.0, norm = 0.0;
        for (int h = 1; h <= nHarm; ++h) {
            const double a = 1.0 / std::pow((double)h, 1.2);
            s += a * std::sin(2.0 * PI * h * f0 * t);
            norm += a;
        }
        const double ataque = 1.0 - std::exp(-t / 0.020);
        const double queda  = 1.0 - std::exp(-(segNota - t) / 0.020);
        x[(size_t)(nSil + i)] = (float)(0.7 * (s / norm) * ataque * std::max(0.0, queda));
    }
    return x;
}

// Um bloco cronometrado: quando ele entrou (em segundos de sinal) e quanto
// custou (em ms de parede).
struct Bloco { double t; double ms; };

static std::vector<Bloco> cronometrar(const std::vector<float>& x, int fs,
                                      MotorSintese motor, int block,
                                      std::vector<float>* saida = nullptr) {
    StreamParams p;
    p.motor = motor;
    if (motor == MotorSintese::Ponteiro) p.look = 0;
    AutotuneStream eng; eng.prepare(fs, p);

    std::vector<float> out(x.size(), 0.0f);
    std::vector<Bloco> blocos;
    blocos.reserve(x.size() / (size_t)block + 1);
    for (size_t off = 0; off < x.size(); off += (size_t)block) {
        const int nb = (int)std::min((size_t)block, x.size() - off);
        const auto t0 = std::chrono::steady_clock::now();
        eng.process(&x[off], &out[off], nb);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        blocos.push_back({ (double)off / fs, ms });
    }
    if (saida) *saida = out;
    return blocos;
}

// Percentil (0..1) dos custos de uma janela de tempo [t0, t1) do sinal.
static double percentilNaJanela(const std::vector<Bloco>& b, double t0, double t1,
                                double q, double* maxOut = nullptr) {
    std::vector<double> v;
    double mx = 0.0;
    for (const auto& e : b)
        if (e.t >= t0 && e.t < t1) { v.push_back(e.ms); mx = std::max(mx, e.ms); }
    if (maxOut) *maxOut = mx;
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t i = (size_t)(q * (double)(v.size() - 1));
    return v[i];
}

// ---------------------------------------------------------------------------
//  SECAO 1 — o custo por bloco nao cresce com a duracao da nota.
//
//  A asercao central do ticket. Duas janelas dentro da MESMA nota sustentada:
//  uma logo depois de o pipeline encher, outra no fim da nota. Se a janela de
//  re-sintese recua ate o inicio da regiao vozeada, a segunda custa varias
//  vezes a primeira; com teto, as duas custam o mesmo.
// ---------------------------------------------------------------------------
static constexpr double SEG_NOTA   = 3.0;   // duracao da nota sustentada
static constexpr double SEG_SIL    = 0.25;  // silencio antes e depois
// Folga da razao. 1,6 aceita o ruido de medicao de uma maquina carregada e
// ainda reprova com sobra o crescimento medido antes do teto (4x e mais).
static constexpr double FATOR_MAX  = 1.6;

static void secaoCrescimento(int fs) {
    std::printf("== 1. o custo por bloco nao cresce com a duracao da nota ==\n");
    const int block = 128;
    const double orcamentoMs = 1000.0 * block / fs;   // 2,90 ms a 44,1 kHz

    const auto x = sinalNotaLonga(fs, SEG_NOTA, 220.0, SEG_SIL);
    const auto b = cronometrar(x, fs, MotorSintese::PSOLA, block);

    // Janelas: meio segundo cada, bem dentro da nota. A primeira comeca em
    // 0,80 s -- a nota entra em 0,25 s e o pipeline enche em ~71 ms (quadro +
    // look-ahead + guarda), entao 0,80 s ja e' regime, com folga.
    const double iniA = 0.80, fimA = 1.30;
    const double iniB = SEG_SIL + SEG_NOTA - 0.50, fimB = SEG_SIL + SEG_NOTA;

    double maxA = 0.0, maxB = 0.0;
    const double p90A = percentilNaJanela(b, iniA, fimA, 0.90, &maxA);
    const double p90B = percentilNaJanela(b, iniB, fimB, 0.90, &maxB);

    checar(p90A > 0.0 && p90B <= p90A * FATOR_MAX,
           "p90 do bloco: inicio da nota %.3f ms -> fim da nota %.3f ms (razao %.2fx, teto %.2fx)",
           p90A, p90B, p90A > 0.0 ? p90B / p90A : 0.0, FATOR_MAX);

    // ---- daqui para baixo e' tudo diagnostico: nenhum 'checar' olha ----
    double maxGeral = 0.0; long long estouros = 0, total = 0;
    for (const auto& e : b) {
        if (e.t < SEG_SIL) continue;            // priming nao conta
        ++total; maxGeral = std::max(maxGeral, e.ms);
        if (e.ms > orcamentoMs) ++estouros;
    }
    diag("orcamento de um bloco de %d amostras a %d Hz: %.2f ms", block, fs, orcamentoMs);
    diag("pior bloco: %.3f ms (%.1fx o orcamento) | maximos por janela: %.3f / %.3f ms",
         maxGeral, maxGeral / orcamentoMs, maxA, maxB);
    diag("blocos acima do orcamento: %lld de %lld (%.1f%%)",
         estouros, total, total ? 100.0 * (double)estouros / (double)total : 0.0);

    // Perfil no tempo, como o do spec: mostra o crescimento e o reset no fim
    // da regiao vozeada.
    diag("perfil (pior bloco por meio segundo):");
    for (double t = 0.0; t < SEG_SIL + SEG_NOTA + SEG_SIL; t += 0.5) {
        double mx = 0.0;
        percentilNaJanela(b, t, t + 0.5, 0.5, &mx);
        std::printf("            t=%4.2f s  %6.3f ms\n", t, mx);
    }
}

// ---------------------------------------------------------------------------
//  SECAO 2 — a invariancia ao tamanho de bloco continua valendo COM o teto.
//
//  E' a exigencia inegociavel do ticket. O teto tem de sair de uma grade fixa
//  (multiplo de nHop, constante para um preset), nunca do que chegou no bloco
//  atual -- senao 'winStart' deixa de ser funcao pura de 'synthFront' e a saida
//  passa a depender de onde o host corta o bloco.
//
//  O baseline.sh ja compara 64 == 512 == 1024 sobre o take real. Aqui a
//  varredura vai de 1 a 4096, que e' o que o ticket pede, e sobre um sinal com
//  nota longa -- que e' justamente o caso em que o teto ATUA. Comparar so com
//  notas curtas testaria o caminho em que o teto nunca dispara.
// ---------------------------------------------------------------------------
static void secaoInvariancia(int fs) {
    std::printf("\n== 2. invariancia ao tamanho de bloco, com o teto ativo ==\n");
    // 1,6 s de nota: passa do teto (que fica na casa de 6,6 mil amostras com
    // FMIN = 80 Hz) e mantem o teste barato mesmo com block = 1.
    const auto x = sinalNotaLonga(fs, 1.60, 220.0, 0.20);

    std::vector<float> ref;
    cronometrar(x, fs, MotorSintese::PSOLA, 128, &ref);

    for (int block : { 1, 3, 64, 256, 512, 1024, 4096 }) {
        std::vector<float> y;
        cronometrar(x, fs, MotorSintese::PSOLA, block, &y);
        long long difs = 0; size_t primeira = 0;
        for (size_t i = 0; i < ref.size(); ++i)
            if (y[i] != ref[i]) { if (!difs) primeira = i; ++difs; }
        checar(difs == 0, "PSOLA  block=%-5d == block=128  (%lld divergencias%s)",
               block, difs, difs ? "" : ", bit a bit");
        if (difs) diag("primeira divergencia em i=%zu", primeira);
    }

    // O motor de ponteiro nao passa por avancarPsola, mas a varredura custa
    // pouco e o ticket pede explicitamente que ele nao regrida.
    std::vector<float> refP;
    cronometrar(x, fs, MotorSintese::Ponteiro, 128, &refP);
    for (int block : { 1, 64, 512, 4096 }) {
        std::vector<float> y;
        cronometrar(x, fs, MotorSintese::Ponteiro, block, &y);
        long long difs = 0;
        for (size_t i = 0; i < refP.size(); ++i) if (y[i] != refP[i]) ++difs;
        checar(difs == 0, "ponteiro block=%-5d == block=128  (%lld divergencias%s)",
               block, difs, difs ? "" : ", bit a bit");
    }
}

// ---------------------------------------------------------------------------
//  SECAO 3 — o motor de ponteiro nao regride.
//
//  Ele e' O(1) por amostra (anel, interpolacao de 4 pontos, sem janelas nem
//  marcas) e ja media 0 % de estouros. O teto do PSOLA nao o toca, mas esta
//  secao existe para que uma mudanca futura em avancarPsola que por engano
//  afete o caminho comum apareca aqui, e nao no ouvido de quem toca.
// ---------------------------------------------------------------------------
static void secaoPonteiro(int fs) {
    std::printf("\n== 3. o motor de ponteiro continua plano ==\n");
    const int block = 128;
    const auto x = sinalNotaLonga(fs, SEG_NOTA, 220.0, SEG_SIL);
    const auto b = cronometrar(x, fs, MotorSintese::Ponteiro, block);

    double maxA = 0.0, maxB = 0.0;
    const double p90A = percentilNaJanela(b, 0.80, 1.30, 0.90, &maxA);
    const double p90B = percentilNaJanela(b, SEG_SIL + SEG_NOTA - 0.50,
                                             SEG_SIL + SEG_NOTA, 0.90, &maxB);
    checar(p90A > 0.0 && p90B <= p90A * FATOR_MAX,
           "p90 do bloco: inicio %.3f ms -> fim %.3f ms (razao %.2fx)",
           p90A, p90B, p90A > 0.0 ? p90B / p90A : 0.0);

    double maxGeral = 0.0;
    for (const auto& e : b) if (e.t >= SEG_SIL) maxGeral = std::max(maxGeral, e.ms);
    diag("pior bloco: %.3f ms (%.1fx o orcamento de %.2f ms)",
         maxGeral, maxGeral / (1000.0 * block / fs), 1000.0 * block / fs);
}

int main() {
    definirEscala("crom");
    const int fs = 44100;
    secaoCrescimento(fs);
    secaoInvariancia(fs);
    secaoPonteiro(fs);
    std::printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK",
                falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
