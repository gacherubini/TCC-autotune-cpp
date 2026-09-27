// ---------------------------------------------------------------------------
//  test_vozes.cpp — a lista de tessituras da interface, presa ao DSP.
//
//  A lista do combo encolheu de 7 itens para 4 (D4 do spec de encaixe e
//  estabilidade). Encolher uma lista de combo parece inofensivo e nao e': ate
//  aqui, nomeVoz() era um switch sobre o indice com 'default: instrumento', e
//  nada no projeto ligava esse switch ao conteudo do combo. Encolher um sem o
//  outro faria o indice 1 mostrar "Alto-Tenor" e o DSP usar "baritono" -- sem
//  erro de compilacao, sem teste falhando, e sem sintoma alem de "a correcao
//  soa errada nesse preset". E' o tipo de defeito que este arquivo existe para
//  tornar impossivel.
//
//  Tres coisas sao afirmadas aqui, e nenhuma delas e' verificavel so olhando:
//
//    1) cada indice do combo resolve para um preset que existe de verdade;
//    2) o ROTULO bate com a FAIXA. O rotulo promete "C3-F5" em notas, o preset
//       entrega 131-698 em Hz, e as duas coisas sao escritas em lugares
//       diferentes por pessoas diferentes. Aqui elas sao confrontadas;
//    3) os nove presets continuam resolvendo pela linha de comando, com as
//       mesmas faixas. A interface encolheu; o nucleo nao. E' o que sustenta a
//       reprodutibilidade das medicoes por tessitura que o texto do TCC cita.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_vozes.cpp -o test_vozes
//  Rodar:     ./test_vozes         (0 = tudo certo, 1 = falhou)
// ---------------------------------------------------------------------------
#include "../core/dsp.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <string>

static int falhas = 0;

static void checar(bool ok, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (!ok) ++falhas;
    std::printf("  %s  ", ok ? "ok  " : "FALHA");
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}

// Extrai "C3" e "F5" de "Alto-Tenor (C3-F5)". Devolve false se o rotulo nao
// tiver a forma esperada -- o que ja e' uma falha: o rotulo PRECISA anunciar a
// faixa em notas (historia 9 do spec).
static bool faixaDoRotulo(const std::string& rotulo, std::string& grave, std::string& agudo) {
    const size_t a = rotulo.find('(');
    const size_t b = rotulo.find(')', a == std::string::npos ? 0 : a);
    if (a == std::string::npos || b == std::string::npos) return false;
    const std::string dentro = rotulo.substr(a + 1, b - a - 1);
    const size_t h = dentro.find('-');
    if (h == std::string::npos) return false;
    grave = dentro.substr(0, h);
    agudo = dentro.substr(h + 1);
    return !grave.empty() && !agudo.empty();
}

int main() {
    std::printf("== 1. os quatro itens do combo resolvem, e o rotulo bate com a faixa ==\n");
    for (int i = 0; i < N_VOZES_UI; ++i) {
        const VozDaInterface& v = vozDaInterface(i);
        double fmin = 0.0, fmax = 0.0;
        const bool existe = presetVoz(v.preset, fmin, fmax);
        if (!existe) {
            checar(false, "indice %d -> preset '%s' NAO existe em presetVoz()", i, v.preset);
            continue;
        }
        std::string grave, agudo;
        if (!faixaDoRotulo(v.rotulo, grave, agudo)) {
            checar(false, "indice %d -> rotulo '%s' nao anuncia a faixa em notas", i, v.rotulo);
            continue;
        }
        // E' aqui que o texto do rotulo encontra o numero do preset.
        const std::string notaGrave = hzParaNota(fmin);
        const std::string notaAguda = hzParaNota(fmax);
        checar(grave == notaGrave && agudo == notaAguda,
               "%-22s -> %-11s %4.0f-%4.0f Hz = %s-%s",
               v.rotulo, v.preset, fmin, fmax, notaGrave.c_str(), notaAguda.c_str());
    }

    std::printf("\n== 2. a ordem da lista e' a escolhida, nao a herdada ==\n");
    {
        // A lista foi de 7 itens para 4, e a APVTS grava o INDICE. Um projeto
        // salvo com 'Contralto' (indice 3 da lista antiga) reabre no indice 3
        // da lista nova, e nao ha como preservar quando a lista encolhe. O que
        // da' para escolher e' ONDE ele cai -- e a escolha foi o preset mais
        // largo, o unico imune ao erro de oitava da Causa 1. Se alguem
        // reordenar a lista, este teste explica o que se perde.
        double fmin = 0.0, fmax = 0.0;
        presetVoz(vozDaInterface(3).preset, fmin, fmax);
        double larguraMax = 0.0;
        for (int i = 0; i < N_VOZES_UI; ++i) {
            double a = 0.0, b = 0.0;
            presetVoz(vozDaInterface(i).preset, a, b);
            larguraMax = std::fmax(larguraMax, b / a);
        }
        checar(std::fabs(fmax / fmin - larguraMax) < 1e-9,
               "indice 3 e' o preset MAIS LARGO (%s, %.0f-%.0f Hz): projetos salvos "
               "com a lista antiga pousam nele", vozDaInterface(3).preset, fmin, fmax);

        double pf = 0.0, px = 0.0;
        presetVoz(vozDaInterface(VOZ_UI_PADRAO).preset, pf, px);
        checar(std::string(vozDaInterface(VOZ_UI_PADRAO).preset) == "altotenor"
               && pf == 131.0 && px == 698.0,
               "padrao de fabrica = indice %d, %s (%.0f-%.0f Hz)",
               VOZ_UI_PADRAO, vozDaInterface(VOZ_UI_PADRAO).preset, pf, px);

        // Indice invalido nao pode virar uma tessitura estreita por acidente:
        // cai no mais tolerante, igual ao 'default' que o switch antigo tinha.
        checar(std::string(vozDaInterface(-1).preset) == "instrumento"
               && std::string(vozDaInterface(99).preset) == "instrumento",
               "indice fora da lista cai em 'instrumento'");
    }

    std::printf("\n== 3. os NOVE presets continuam resolvendo pela linha de comando ==\n");
    {
        // A interface encolheu, o nucleo nao. Os presets SATB sao dados de
        // medicao antes de serem itens de menu: foi com eles que a tabela de
        // erro de oitava e a de cobertura de tessitura do spec foram levantadas.
        struct Caso { const char* nome; double fmin, fmax; };
        const Caso nove[] = {
            { "baixo",       82.0,  330.0  }, { "baritono",  98.0,  392.0  },
            { "tenor",      131.0,  523.0  }, { "contralto", 175.0, 698.0  },
            { "mezzo",      220.0,  880.0  }, { "soprano",   262.0, 1047.0 },
            { "lowmale",     82.0,  392.0  }, { "altotenor", 131.0, 698.0  },
            { "instrumento", 50.0, 2000.0  },
        };
        for (const auto& c : nove) {
            double fmin = 0.0, fmax = 0.0;
            const bool ok = presetVoz(c.nome, fmin, fmax);
            checar(ok && fmin == c.fmin && fmax == c.fmax,
                   "voz=%-12s -> %4.0f-%4.0f Hz", c.nome, fmin, fmax);
        }
        // Os sinonimos que os CLIs aceitam continuam valendo.
        double a = 0.0, b = 0.0;
        checar(presetVoz("alto", a, b) && a == 175.0, "sinonimo 'alto' == contralto");
        checar(presetVoz("BASS", a, b) && a == 82.0 && b == 330.0,
               "presetVoz e' insensivel a caixa ('BASS')");
        checar(!presetVoz("naoexiste", a, b), "nome desconhecido devolve false");
    }

    std::printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK",
                falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
