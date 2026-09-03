// ---------------------------------------------------------------------------
//  test_deteccao.cpp — a SEAM de altura conhecida.
//
//  Todos os testes que existiam antes deste verificavam REPRODUTIBILIDADE: que
//  o motor continua fazendo o que fazia. Nenhum verificava VALIDADE -- que o que
//  ele faz esta certo. E' por isso que os tres defeitos de
//  docs/spec-encaixe-e-estabilidade.md atravessaram 37 casos de linha de base
//  sem serem vistos: os tres sao estaveis e reprodutiveis. Um checksum prova que
//  o comportamento nao mudou; ele nao prova que o comportamento e' correto.
//
//  Este arquivo e' o primeiro do repositorio a verificar a segunda coisa. Ele
//  alimenta o nucleo de streaming com voz SINTETICA de altura CONHECIDA e
//  assevera sobre as duas trilhas que o nucleo ja expoe -- a altura detectada
//  (getF0Samp) e o pitch-alvo (getFoutSamp). Nada aqui olha marcas de periodo,
//  colunas do Viterbi, posicao de ponteiro ou buffer interno: sao detalhes de
//  implementacao, e um teste que os trave impede a proxima correcao legitima.
//
//  A seam esta no ponto mais alto possivel de proposito. Testar a selecao de
//  candidato de periodo direto em dsp.h (no estilo dos outros testes) pegaria a
//  Causa 1, mas NAO a Causa 2: o piscar da nota-alvo nao existe na funcao
//  isolada, ele so aparece na trilha inteira. Uma seam, os dois defeitos, e os
//  dois motores de sintese -- porque os dois consomem estas mesmas duas trilhas.
//
//  ORACULOS CONGELADOS. As tres secoes abaixo registram o comportamento MEDIDO
//  em 03/09/2026, defeitos inclusos, e batem com as tabelas do spec. Os valores
//  ficam em constantes nomeadas para que a correcao apareca como uma troca de
//  UMA linha no diff, em vez de se esconder numa tolerancia afrouxada. E' a
//  mesma tecnica que test_retune.cpp usa com CorretorEtapa2.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_deteccao.cpp -o test_deteccao
//  Rodar:     ./test_deteccao      (0 = tudo certo, 1 = falhou)
//             Precisa de exemplo-antes.wav -- rode da raiz do repositorio.
// ---------------------------------------------------------------------------
#define DR_WAV_IMPLEMENTATION
#include "../c1_streaming/autotune_stream.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

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
//  O gerador de voz sintetica. E' parte da seam, nao um acessorio: e' ele que
//  torna a altura CONHECIDA, e sem altura conhecida nao ha como afirmar que a
//  deteccao esta errada -- so que ela mudou.
//
//  Perfil harmonico decrescente (1/h^1.2) porque uma senoide pura nao exercita
//  o defeito: a Causa 1 vive na ambiguidade entre o periodo T e o periodo 2T, e
//  essa ambiguidade e' criada pelos harmonicos. Envelope de ataque e queda
//  porque o detector precisa de fronteiras vozeado/nao-vozeado para exercitar o
//  HMM, e uma onda que comeca e termina em degrau produziria transitorio.
// ---------------------------------------------------------------------------
static std::vector<float> vozSintetica(int fs, double segundos, double f0) {
    const int N = (int)(segundos * fs);
    std::vector<float> x((size_t)N, 0.0f);
    const int nHarm = std::min(24, std::max(1, (int)((0.45 * fs) / f0)));
    for (int i = 0; i < N; ++i) {
        const double t = (double)i / fs;
        double s = 0.0, norm = 0.0;
        for (int h = 1; h <= nHarm; ++h) {
            const double a = 1.0 / std::pow((double)h, 1.2);
            s += a * std::sin(2.0 * PI * h * f0 * t);
            norm += a;
        }
        const double ataque = 1.0 - std::exp(-t / 0.020);
        const double queda  = 1.0 - std::exp(-(segundos - t) / 0.020);
        x[(size_t)i] = (float)(0.7 * (s / norm) * ataque * std::max(0.0, queda));
    }
    return x;
}

// As duas trilhas publicas, para um sinal e um preset de tessitura. Roda no
// motor de PONTEIRO porque as trilhas sao decididas no estagio 1+2, que os dois
// motores compartilham: a sintese nao entra nesta conta, e o ponteiro e' O(1)
// por amostra (o PSOLA levaria minutos para as ~30 passadas deste arquivo).
struct Trilhas { std::vector<float> f0, fout; };

static Trilhas rodarNucleo(const std::vector<float>& x, int fs, const char* voz,
                           double tolCents, double retuneMs, int block = 128) {
    StreamParams p;
    presetVoz(voz, p.fmin, p.fmax);
    p.corr.tolCents = tolCents;
    p.corr.retuneMs = retuneMs;
    p.motor = MotorSintese::Ponteiro;   // == lowlat=1
    p.look  = 0;
    AutotuneStream eng; eng.prepare(fs, p);
    std::vector<float> out(x.size(), 0.0f);
    for (size_t off = 0; off < x.size(); off += (size_t)block) {
        const int nb = (int)std::min((size_t)block, x.size() - off);
        eng.process(&x[off], &out[off], nb);
    }
    return { eng.getF0Samp(), eng.getFoutSamp() };
}

// exemplo-antes.wav e' o unico audio versionado (o resto do *.wav esta no
// .gitignore). O caminho e' procurado em alguns lugares porque o teste roda
// tanto pelo baseline.sh (cwd = raiz) quanto a mao.
static std::vector<float> lerTake(int& fs) {
    static const char* tentativas[] = {
        "exemplo-antes.wav", "../exemplo-antes.wav", "../../exemplo-antes.wav"
    };
    for (const char* c : tentativas) {
        unsigned canais = 0, taxa = 0; drwav_uint64 nn = 0;
        float* d = drwav_open_file_and_read_pcm_frames_f32(c, &canais, &taxa, &nn, nullptr);
        if (!d) continue;
        std::vector<float> x((size_t)nn);
        for (drwav_uint64 i = 0; i < nn; ++i) {
            float s = 0.0f;
            for (unsigned k = 0; k < canais; ++k) s += d[i * canais + k];
            x[(size_t)i] = s / (float)canais;
        }
        drwav_free(d, nullptr);
        fs = (int)taxa;
        return x;
    }
    return {};
}

// ---------------------------------------------------------------------------
//  SECAO 1 — varredura de altura conhecida cruzando o teto de cada tessitura.
//
//  A busca de periodo percorre a faixa a partir do periodo mais CURTO admitido
//  pela tessitura. Se a altura cantada for mais aguda que o teto do preset, o
//  periodo real fica invisivel para a busca -- mas o DOBRO dele nao fica, porque
//  um sinal periodico em T tambem e' periodico em 2T. A busca encontra 2T e
//  reporta METADE da frequencia.
//
//  O defeito e' assimetrico, e e' isso que o torna traicoeiro: abaixo do piso o
//  detector reporta "sem voz" e nao corrige (silencioso, mas honesto); acima do
//  teto ele corrige para a nota errada.
// ---------------------------------------------------------------------------

// O que o detector faz ACIMA do teto da tessitura. Trocar esta constante e' a
// unica mudanca que a correcao da Causa 1 provoca neste arquivo -- por isso ela
// existe: a correcao vira uma linha do diff, e o comportamento anterior fica
// registrado no codigo para poder ser descrito no texto do TCC.
enum class AcimaDoTeto { OitavaAbaixo, SemVoz };
static constexpr AcimaDoTeto ACIMA_DO_TETO = AcimaDoTeto::OitavaAbaixo;

// Tolerancia relativa do detector. 4 % cobre a grade de 20 cents do HMM
// (RES_CENTS) com folga; nao e' frouxa o bastante para confundir uma oitava
// (que e' 100 %) com um acerto.
static constexpr double TOL_REL = 0.04;

static void secaoVarredura(int fs) {
    std::printf("== 1. varredura de altura conhecida, cruzando o teto da tessitura ==\n");
    // As mesmas nove alturas e os mesmos tres presets da tabela do spec §2.
    const double alturas[] = { 220, 262, 294, 330, 349, 392, 440, 494, 520 };
    const char* presets[]  = { "baixo", "lowmale", "contralto" };

    for (const char* voz : presets) {
        double fmin = 0.0, fmax = 0.0;
        presetVoz(voz, fmin, fmax);
        for (double alvo : alturas) {
            const auto x  = vozSintetica(fs, 0.5, alvo);
            const auto tr = rodarNucleo(x, fs, voz, 0.0, 0.0);

            long long nVoz = 0, nCerto = 0, nOitava = 0;
            for (float f : tr.f0) {
                if (f <= 0.0f) continue;
                ++nVoz;
                if (std::fabs(f - alvo)       < TOL_REL * alvo)       ++nCerto;
                if (std::fabs(f - alvo * 0.5) < TOL_REL * alvo * 0.5) ++nOitava;
            }
            const double pCerto  = nVoz ? (double)nCerto  / (double)nVoz : 0.0;
            const double pOitava = nVoz ? (double)nOitava / (double)nVoz : 0.0;

            if (alvo <= fmax) {
                // Dentro da faixa a altura reportada tem de bater com a real.
                checar(nVoz > 0 && pCerto > 0.95,
                       "%-10s %4.0f Hz (dentro)  -> %.0f%% dos quadros na altura certa",
                       voz, alvo, 100.0 * pCerto);
            } else if (ACIMA_DO_TETO == AcimaDoTeto::OitavaAbaixo) {
                // O DEFEITO, congelado: 100 % dos quadros uma oitava abaixo.
                checar(nVoz > 0 && pOitava > 0.95 && nCerto == 0,
                       "%-10s %4.0f Hz (ACIMA)   -> %.0f%% uma oitava abaixo, %lld na altura certa",
                       voz, alvo, 100.0 * pOitava, nCerto);
            } else {
                // Depois da guarda: fora da faixa, "sem voz" -- a mesma regra
                // que ja vale abaixo do piso.
                checar(nVoz == 0,
                       "%-10s %4.0f Hz (ACIMA)   -> %lld quadros reportados (esperado 0)",
                       voz, alvo, nVoz);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  SECAO 2 — cobertura de vozeamento por preset, sobre o take real.
//
//  Esta secao nao mede defeito nenhum. Ela existe para ser o DENOMINADOR da
//  correcao da Causa 1. Uma guarda que rejeita quadros acima do teto e'
//  aprovavel por uma guarda que rejeita TUDO: "zero quadros na oitava errada" e'
//  o que uma guarda muda demais tambem entrega. O numero que impede isso e' a
//  fracao de quadros vozeados entre os que a referencia ve DENTRO da faixa do
//  preset -- esses a guarda nao pode tocar.
//
//  O modo de falha que ele pega mora fora da varredura sintetica: numa voz de
//  fundamental fraco ou segundo harmonico forte, a CMNDF mergulha legitimamente
//  em T/2, e uma guarda mal calibrada le esse mergulho como "o periodo real esta
//  acima do fmax" e descarta um quadro que estava certo. O sintoma trocado seria
//  "a correcao some em partes da frase" -- mais barato que a oitava errada, mas
//  ainda um defeito, e invisivel para qualquer asercao que so olhe oitava.
// ---------------------------------------------------------------------------
struct Cobertura {
    const char* voz;
    double pctOitava;    // quadros que divergem do preset largo por uma oitava
    double pctNaFaixa;   // quadros vozeados entre os que a referencia ve na faixa
};

// Medido em 03/09/2026 sobre exemplo-antes.wav. Os 34,3 % e 16,2 % sao os
// numeros que o spec cita; os "na faixa" sao o piso que a guarda nao pode
// derrubar.
static const Cobertura COBERTURA_ANTES[] = {
    { "baixo",       34.3, 99.5 },
    { "baritono",    15.9, 99.5 },
    { "tenor",        0.0, 99.6 },
    { "contralto",    0.0, 99.6 },
    { "mezzo",        0.0, 99.8 },
    { "soprano",      0.0, 99.8 },
    { "lowmale",     16.2, 99.1 },
    { "altotenor",    0.0, 99.6 },
    { "instrumento",  0.0, 100.0 },
};

// Quanto a guarda pode custar em quadros vozeados DENTRO da faixa, em pontos
// percentuais. Orcamento fixado no ticket 02.
static constexpr double ORCAMENTO_PERDA_PP = 2.0;

// A divergencia de oitava que se espera depois da guarda. Zero -- mas a
// constante existe para que o ticket 02 seja uma linha, como na secao 1.
static constexpr bool GUARDA_ATIVA = (ACIMA_DO_TETO == AcimaDoTeto::SemVoz);

static void secaoCobertura(const std::vector<float>& take, int fs) {
    std::printf("\n== 2. cobertura de vozeamento por preset (o denominador da guarda) ==\n");
    const auto ref = rodarNucleo(take, fs, "instrumento", 0.0, 0.0);

    for (const auto& c : COBERTURA_ANTES) {
        double fmin = 0.0, fmax = 0.0;
        presetVoz(c.voz, fmin, fmax);
        const auto tr = rodarNucleo(take, fs, c.voz, 0.0, 0.0);

        long long nComum = 0, nOitava = 0, nNaFaixa = 0, nVozNaFaixa = 0;
        const size_t n = std::min(tr.f0.size(), ref.f0.size());
        for (size_t i = 0; i < n; ++i) {
            if (ref.f0[i] <= 0.0f) continue;
            ++nComum;
            if (tr.f0[i] > 0.0f) {
                const double r = ref.f0[i] / tr.f0[i];
                if (r > 1.8 && r < 2.2) ++nOitava;
            }
            if (ref.f0[i] >= fmin && ref.f0[i] <= fmax) {
                ++nNaFaixa;
                if (tr.f0[i] > 0.0f) ++nVozNaFaixa;
            }
        }
        const double pOitava  = nComum   ? 100.0 * nOitava     / (double)nComum   : 0.0;
        const double pNaFaixa = nNaFaixa ? 100.0 * nVozNaFaixa / (double)nNaFaixa : 0.0;

        // Oitava: congelada no valor de hoje ate a guarda entrar; zero depois.
        const bool okOitava = GUARDA_ATIVA ? (pOitava < 0.5)
                                           : (std::fabs(pOitava - c.pctOitava) < 1.5);
        // Cobertura dentro da faixa: nunca pode cair mais que o orcamento.
        const bool okFaixa = (pNaFaixa >= c.pctNaFaixa - ORCAMENTO_PERDA_PP);

        checar(okOitava && okFaixa,
               "%-12s oitava %5.1f%% (era %.1f)  |  vozeado na faixa %5.1f%% (era %.1f, piso %.1f)",
               c.voz, pOitava, c.pctOitava, pNaFaixa, c.pctNaFaixa,
               c.pctNaFaixa - ORCAMENTO_PERDA_PP);
    }
}

// ---------------------------------------------------------------------------
//  SECAO 3 — o piscar da nota-alvo, sobre o take real.
//
//  A trilha de altura chega crua a malha de correcao, sem histerese. Ruido de
//  poucos cents na estimativa faz a nota-alvo trocar de semitom, e a malha
//  persegue cada troca. O efeito audivel e' a saida nunca PARAR em cima de um
//  semitom -- ela fica perto, oscilando.
//
//  A metrica e' a duracao das notas: um trecho maximo em que a nota-alvo nao
//  troca de semitom. Com um alvo que troca a cada 41 ms, um Retune Speed de
//  200 ms nunca alcanca -- ele corre atras e nao chega. E' por isso que no
//  Auto-Tune (alvo estavel) um Retune lento soa como bypass limpo, e aqui soa
//  como enjoo.
// ---------------------------------------------------------------------------

// Medido em 03/09/2026 na configuracao do usuario (contralto, tol=15,
// retune=0, Low Latency), sobre os 5 s de exemplo-antes.wav.
static constexpr double MEDIANA_ANTES_MS = 40.6;
static constexpr double PCT_CURTAS_ANTES = 76.0;
static constexpr double LIMIAR_CURTA_MS  = 80.0;

// Faixas de aceitacao. O teste nao pode exigir o valor exato: uma diferenca de
// arredondamento noutro compilador desloca uma fronteira de nota e move a
// mediana em alguns ms. As faixas sao largas o bastante para isso e estreitas o
// bastante para que a melhora do ticket 04 (que multiplica a mediana) apareca.
static constexpr double MEDIANA_TOL_MS = 4.0;
static constexpr double PCT_TOL        = 6.0;

// Ligada pelo ticket 04: as afirmacoes deixam de ser "reproduz o defeito" e
// passam a ser "melhorou contra o defeito congelado acima".
static constexpr bool HISTERESE_ATIVA = false;

// Duracao (ms) de cada nota emitida: trecho maximo de amostras vozeadas em que
// notaMaisProximaMidi(fout) nao muda. Com retune = 0 o fout E' o alvo, entao
// isto le exatamente a decisao do motor, sem passar pelo filtro.
static std::vector<double> duracoesDeNota(const std::vector<float>& fout, int fs) {
    std::vector<double> ms;
    size_t i = 0;
    const size_t n = fout.size();
    while (i < n) {
        while (i < n && fout[i] <= 0.0f) ++i;
        if (i >= n) break;
        const int nota = notaMaisProximaMidi((double)fout[i]);
        const size_t ini = i;
        while (i < n && fout[i] > 0.0f
               && notaMaisProximaMidi((double)fout[i]) == nota) ++i;
        ms.push_back(1000.0 * (double)(i - ini) / (double)fs);
    }
    return ms;
}

static void secaoPiscar(const std::vector<float>& take, int fs) {
    std::printf("\n== 3. piscar da nota-alvo (contralto, tol=15, retune=0, lowlat) ==\n");
    const auto tr = rodarNucleo(take, fs, "contralto", 15.0, 0.0);
    auto ms = duracoesDeNota(tr.fout, fs);
    if (ms.empty()) { checar(false, "nenhuma nota emitida"); return; }

    std::sort(ms.begin(), ms.end());
    const double mediana = ms[ms.size() / 2];
    long long curtas = 0;
    for (double d : ms) if (d < LIMIAR_CURTA_MS) ++curtas;
    const double pctCurtas = 100.0 * (double)curtas / (double)ms.size();

    if (!HISTERESE_ATIVA) {
        checar(std::fabs(mediana - MEDIANA_ANTES_MS) < MEDIANA_TOL_MS,
               "%zu notas em %.1f s | mediana %.1f ms (congelado %.1f)",
               ms.size(), (double)take.size() / fs, mediana, MEDIANA_ANTES_MS);
        checar(std::fabs(pctCurtas - PCT_CURTAS_ANTES) < PCT_TOL,
               "notas abaixo de %.0f ms: %.0f%% (congelado %.0f%%)",
               LIMIAR_CURTA_MS, pctCurtas, PCT_CURTAS_ANTES);
    } else {
        checar(mediana > MEDIANA_ANTES_MS + MEDIANA_TOL_MS,
               "%zu notas | mediana %.1f ms, acima dos %.1f congelados",
               ms.size(), mediana, MEDIANA_ANTES_MS);
        checar(pctCurtas < PCT_CURTAS_ANTES - PCT_TOL,
               "notas abaixo de %.0f ms: %.0f%%, abaixo dos %.0f%% congelados",
               LIMIAR_CURTA_MS, pctCurtas, PCT_CURTAS_ANTES);
    }
}

int main() {
    definirEscala("crom");

    int fs = 0;
    const std::vector<float> take = lerTake(fs);
    if (take.empty()) {
        std::printf("ERRO: nao achei exemplo-antes.wav. Rode da raiz do repositorio.\n");
        return 1;
    }

    secaoVarredura(44100);
    secaoCobertura(take, fs);
    secaoPiscar(take, fs);

    std::printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK",
                falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
