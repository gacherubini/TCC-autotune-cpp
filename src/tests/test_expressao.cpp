// ---------------------------------------------------------------------------
//  test_expressao.cpp — Humanize (Etapa 4) e Create Vibrato (Etapa 5).
//
//  As duas etapas seguem o mesmo padrao das anteriores: cada uma introduz
//  parametros cujo valor NEUTRO tem de reproduzir a etapa anterior bit a bit.
//  Aqui isso e' mais delicado que nas outras, porque os dois recursos mexem em
//  coisas que ja estavam funcionando:
//
//    - Humanize mexe no TAU do filtro, que e' o coracao da Etapa 3;
//    - Create Vibrato SOMA na altura de saida e MULTIPLICA a amplitude.
//
//  Um caminho neutro que "quase" nao faz nada (multiplicar por 1.0 calculado,
//  somar 0.0 calculado) passaria despercebido numa escuta e quebraria a
//  identidade bit a bit. Por isso as secoes 1 e 3 exigem igualdade EXATA.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_expressao.cpp -o test_expressao
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
    std::vprintf(fmt, ap); std::printf("\n"); va_end(ap);
}

// ---------------------------------------------------------------------------
//  A malha da ETAPA 3, congelada. Mesmo papel do oraculo de test_retune.cpp:
//  e' contra ESTE texto que a neutralidade das Etapas 4 e 5 e' medida, e ele
//  nao pode acompanhar a evolucao do dsp.h.
// ---------------------------------------------------------------------------
struct CorretorEtapa3 {
    double fs = 44100.0, lpAlvo = 0.0, lpReal = 0.0; bool tinhaNota = false;
    double proxima(double f0Hz, double tolCents, double retuneMs, double k) {
        if (f0Hz <= 0.0) { tinhaNota = false; return 0.0; }
        const double real  = 1200.0 * std::log2(f0Hz / FMIN);
        const double alvo  = 1200.0 * std::log2(notaAlvo(f0Hz, tolCents) / FMIN);
        const double tau   = retuneMs / 1000.0;
        const double alpha = (tau > 0.0) ? std::exp(-1.0 / (tau * fs)) : 0.0;
        if (!tinhaNota) { lpAlvo = real; lpReal = real; tinhaNota = true; }
        else { lpAlvo = alpha*lpAlvo + (1-alpha)*alvo; lpReal = alpha*lpReal + (1-alpha)*real; }
        return FMIN * std::pow(2.0, (lpAlvo + k*(real - lpReal)) / 1200.0);
    }
};

static std::vector<double> trilha(int fs, int N) {
    std::vector<double> f0((size_t)N, 0.0);
    for (int i = 0; i < N; ++i) {
        const double t = (double)i / fs;
        if (t < 0.10 || (t > 0.95 && t < 1.10) || t > 2.60) continue;
        const double base = (t < 1.10) ? 220.0 : 261.63;
        const double dv   = (t < 1.10) ? (-30.0 + 20.0*t) : (18.0 - 6.0*t);
        f0[(size_t)i] = base * std::pow(2.0, (dv + 22.0*std::sin(2*PI*5.5*t)) / 1200.0);
    }
    return f0;
}

// Amplitude (em cents, meia excursao) da saida em regime, para uma nota longa
// com vibrato do cantor. 'ini' recorta a janela de medicao.
static double amplitudeCents(const ParamsCorrecao& p, int fs, double ini, double fim,
                             double ampEntrada, double fv) {
    CorretorAltura c; c.prepare(fs);
    double lo = 1e9, hi = -1e9;
    const int N = (int)(fim * fs) + 1;
    for (int i = 0; i < N; ++i) {
        const double t = (double)i / fs;
        const double y = c.proxima(220.0 * std::pow(2.0, ampEntrada*std::sin(2*PI*fv*t)/1200.0), p);
        if (t < ini) continue;
        const double yc = 1200.0 * std::log2(y / 220.0);
        lo = std::fmin(lo, yc); hi = std::fmax(hi, yc);
    }
    return (hi - lo) / 2.0;
}

int main() {
    const int fs = 44100, N = 3*fs;
    const std::vector<double> f0 = trilha(fs, N);
    definirEscala("crom");

    std::printf("== 1. Etapa 4 neutra: humanize=0 == Etapa 3, bit a bit ==\n");
    for (double ret : {0.0, 25.0, 120.0}) for (double k : {0.0, 1.0, 2.0}) {
        ParamsCorrecao p; p.tolCents = 15.0; p.retuneMs = ret; p.vibrato = k; p.humanize = 0.0;
        CorretorAltura nv; nv.prepare(fs);
        CorretorEtapa3 vl; vl.fs = fs;
        long long d = 0;
        for (int i = 0; i < N; ++i)
            if (nv.proxima(f0[(size_t)i], p) != vl.proxima(f0[(size_t)i], 15.0, ret, k)) ++d;
        checar(d == 0, "retune=%-5.0f k=%.1f -> identico", ret, k);
    }

    std::printf("\n== 2. Etapa 5 neutra: sem Create Vibrato == Etapa 4, bit a bit ==\n");
    {
        // Tres formas de "desligado" que precisam ser exatas.
        struct Caso { const char* nome; FormaVibrato f; double prof, amp; };
        const Caso casos[] = {
            {"forma=Nenhuma",                 FormaVibrato::Nenhuma,     30.0, 0.5},
            {"prof=0 e amp=0 (forma senoide)",FormaVibrato::Senoide,      0.0, 0.0},
            {"prof=0 e amp=0 (forma quadrada)",FormaVibrato::Quadrada,    0.0, 0.0},
        };
        for (const auto& c : casos) {
            ParamsCorrecao p; p.tolCents = 15.0; p.retuneMs = 25.0; p.humanize = 0.4;
            ParamsCorrecao q = p;
            q.vibForma = c.f; q.vibProf = c.prof; q.vibAmp = c.amp;
            CorretorAltura a, b; a.prepare(fs); b.prepare(fs);
            long long d = 0; bool ganhoLimpo = true;
            for (int i = 0; i < N; ++i) {
                if (a.proxima(f0[(size_t)i], p) != b.proxima(f0[(size_t)i], q)) ++d;
                if (b.ultimoGanho() != 1.0) ganhoLimpo = false;
            }
            checar(d == 0 && ganhoLimpo, "%s -> identico (e ganho == 1,0)", c.nome);
        }
    }

    std::printf("\n== 3. Humanize afrouxa o tau na sustentacao ==\n");
    {
        // Com tau maior, o passa-altas corta menos -> MAIS vibrato do cantor
        // sobrevive na sustentacao. E' o efeito que o Humanize existe para dar.
        ParamsCorrecao base; base.retuneMs = 25.0; base.vibrato = 1.0;
        ParamsCorrecao hum = base; hum.humanize = 1.0;
        const double semH = amplitudeCents(base, fs, 1.5, 3.0, 30.0, 5.5);
        const double comH = amplitudeCents(hum,  fs, 1.5, 3.0, 30.0, 5.5);
        checar(comH > semH * 1.3, "sustentacao: %.1f ct sem Humanize -> %.1f ct com (x%.2f)",
               semH, comH, comH / semH);

        // ... e quase nao mexe no ATAQUE. Cuidado com o enunciado: a rampa NAO
        // e' zero depois do ataque, e' 1-exp(-t/0,2), que em 1,5 ms ja vale
        // ~0,0075. Ou seja, o Humanize passa a agir IMEDIATAMENTE, so que com
        // peso desprezivel. A afirmacao verificavel e' sobre MAGNITUDE, nao
        // sobre igualdade exata -- e a primeira versao deste teste exigia
        // igualdade exata e falhou com razao.
        ParamsCorrecao a = base, b = hum;
        CorretorAltura ca, cb; ca.prepare(fs); cb.prepare(fs);
        const double f = 220.0 * std::pow(2.0, 40.0/1200.0);   // entra 40 ct alto
        double maxCt = 0.0;
        for (int i = 0; i < 64; ++i) {          // ~1,5 ms depois do ataque
            const double ya = ca.proxima(f, a), yb = cb.proxima(f, b);
            maxCt = std::fmax(maxCt, std::fabs(1200.0 * std::log2(yb / ya)));
        }
        // 0,05 cent e' ~3 ordens de grandeza abaixo do limiar de percepcao
        // (~5 ct); o gesto de ataque continua sendo o da Etapa 3.
        checar(maxCt < 0.05, "nos primeiros 1,5 ms o Humanize desvia %.4f ct (limite 0,05)", maxCt);

        // E a acao CRESCE ao longo do trajeto ate a nota. Atencao ao que se mede
        // aqui: com F0 CONSTANTE os dois filtros convergem para o MESMO alvo,
        // qualquer que seja o tau -- entao o desvio volta a zero em regime. O
        // Humanize muda o TRANSITORIO, nao o ponto de chegada. (A versao
        // anterior deste teste olhava o desvio no instante 1 s e achava 0,00 ct,
        // concluindo erradamente que o Humanize nao estava agindo.)
        //
        // O que ele muda em REGIME e' o quanto de vibrato sobrevive -- e isso ja
        // esta medido na primeira verificacao desta secao, com entrada que se move.
        double maxTraj = 0.0;
        for (int i = 64; i < fs; ++i) {
            const double ya = ca.proxima(f, a), yb = cb.proxima(f, b);
            maxTraj = std::fmax(maxTraj, std::fabs(1200.0 * std::log2(yb / ya)));
        }
        checar(maxTraj > 1.0, "no trajeto ate a nota o desvio chega a %.2f ct", maxTraj);
    }

    std::printf("\n== 4. formaVibrato(): as quatro formas ==\n");
    {
        checar(std::fabs(formaVibrato(FormaVibrato::Senoide, 0.25) - 1.0) < 1e-12, "senoide(1/4) = +1");
        checar(std::fabs(formaVibrato(FormaVibrato::Senoide, 0.75) + 1.0) < 1e-12, "senoide(3/4) = -1");
        checar(std::fabs(formaVibrato(FormaVibrato::Triangular, 0.25) - 0.0) < 1e-12, "triangular(1/4) = 0");
        checar(std::fabs(formaVibrato(FormaVibrato::Triangular, 0.5) - 1.0) < 1e-12, "triangular(1/2) = +1");
        checar(formaVibrato(FormaVibrato::Quadrada, 0.2) == 1.0 &&
               formaVibrato(FormaVibrato::Quadrada, 0.8) == -1.0, "quadrada alterna +1/-1");
        checar(formaVibrato(FormaVibrato::Nenhuma, 0.3) == 0.0, "nenhuma = 0 em qualquer fase");
        // Nenhuma forma pode escapar de [-1,1] -- e' o que da sentido a
        // 'vibprof' ser lido como profundidade em cents.
        bool dentro = true;
        for (int k = 0; k <= 1000; ++k) {
            const double ph = k / 1000.0;
            for (auto f : {FormaVibrato::Senoide, FormaVibrato::Triangular, FormaVibrato::Quadrada})
                if (std::fabs(formaVibrato(f, ph)) > 1.0 + 1e-12) dentro = false;
        }
        checar(dentro, "as tres formas ficam em [-1,1] na varredura de fase");
    }

    std::printf("\n== 5. Create Vibrato: profundidade, taxa e atraso de entrada ==\n");
    {
        // Sem vibrato do cantor e sem correcao a mover (nota exata): o que
        // sobrar na saida e' SO o vibrato gerado.
        ParamsCorrecao p; p.retuneMs = 0.0; p.vibrato = 0.0;
        p.vibForma = FormaVibrato::Senoide; p.vibTaxa = 6.0; p.vibProf = 40.0;
        CorretorAltura c; c.prepare(fs);
        std::vector<double> yc; yc.reserve((size_t)(3*fs));
        for (int i = 0; i < 3*fs; ++i)
            yc.push_back(1200.0 * std::log2(c.proxima(220.0, p) / 220.0));

        double lo = 1e9, hi = -1e9;
        for (int i = (int)(2.0*fs); i < 3*fs; ++i) { lo = std::fmin(lo, yc[(size_t)i]); hi = std::fmax(hi, yc[(size_t)i]); }
        const double prof = (hi - lo) / 2.0;
        checar(std::fabs(prof - 40.0) < 0.5, "profundidade em regime = %.2f ct (pedido 40)", prof);

        // Taxa: pelo PERIODO MEDIO entre cruzamentos por zero subindo, e nao
        // contando cruzamentos numa janela de 1 s -- essa contagem devolve 5 ou
        // 6 conforme onde a fase pega as bordas da janela, o que fez a primeira
        // versao deste teste falhar sem que nada estivesse errado.
        std::vector<int> cruz;
        for (int i = (int)(1.0*fs)+1; i < 3*fs; ++i)
            if (yc[(size_t)i-1] < 0.0 && yc[(size_t)i] >= 0.0) cruz.push_back(i);
        double taxa = 0.0;
        if (cruz.size() >= 2)
            taxa = (double)fs * (double)(cruz.size() - 1) / (double)(cruz.back() - cruz.front());
        checar(std::fabs(taxa - 6.0) < 0.05, "taxa medida = %.3f Hz em %zu ciclos (pedido 6)",
               taxa, cruz.size());

        // Atraso de entrada: logo apos o ataque a excursao tem de ser MUITO
        // menor que em regime. Sem isso o vibrato "liga" junto com a nota e soa
        // sintetico.
        double loI = 1e9, hiI = -1e9;
        for (int i = 0; i < (int)(0.05*fs); ++i) { loI = std::fmin(loI, yc[(size_t)i]); hiI = std::fmax(hiI, yc[(size_t)i]); }
        const double profIni = (hiI - loI) / 2.0;
        checar(profIni < 0.25 * prof, "nos primeiros 50 ms: %.2f ct (regime %.2f) — entrada suave",
               profIni, prof);
    }

    std::printf("\n== 6. Amplitude Amount ==\n");
    {
        ParamsCorrecao p; p.retuneMs = 0.0; p.vibrato = 0.0;
        p.vibForma = FormaVibrato::Senoide; p.vibTaxa = 6.0; p.vibProf = 40.0; p.vibAmp = 1.0;
        CorretorAltura c; c.prepare(fs);
        double gLo = 1e9, gHi = -1e9;
        for (int i = 0; i < 3*fs; ++i) {
            c.proxima(220.0, p);
            if (i > (int)(2.0*fs)) { gLo = std::fmin(gLo, c.ultimoGanho()); gHi = std::fmax(gHi, c.ultimoGanho()); }
        }
        const double picoDb = 20.0 * std::log10(gHi);
        checar(std::fabs(picoDb - VIB_AMP_DB) < 0.1, "pico = %+.2f dB (esperado %+.1f)", picoDb, VIB_AMP_DB);
        checar(std::fabs(20.0*std::log10(gLo) + VIB_AMP_DB) < 0.1, "vale = %+.2f dB (simetrico)",
               20.0*std::log10(gLo));

        // Em trecho nao-vozeado o ganho tem de voltar a 1: senao o ultimo valor
        // do LFO ficaria "preso" e multiplicaria silencio (ou o proximo ataque).
        c.proxima(0.0, p);
        checar(c.ultimoGanho() == 1.0, "trecho nao-vozeado devolve ganho 1,0");
    }

    std::printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK", falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
