// ---------------------------------------------------------------------------
//  test_escalas.cpp — verificacao de definirEscala() para as 24 tonalidades.
//
//  Existe por causa da Etapa 1 do plano (docs/plano-de-implementacao.md): a
//  interface do plugin expunha 6 das 24 tonalidades, embora o motor sempre
//  tenha suportado todas. Este teste prova duas coisas:
//
//    1) REGRESSAO — as 6 escalas que o plugin oferecia antes continuam
//       produzindo exatamente o mesmo conjunto de notas permitidas;
//    2) COBERTURA — as 24 tonalidades (12 tonicas x maior/menor) produzem o
//       conjunto correto, com o numero certo de notas e os intervalos certos.
//
//  Compilar:  c++ -std=c++17 -O2 -I external src/tests/test_escalas.cpp -o test_escalas
//  Rodar:     ./test_escalas          (0 = tudo certo, 1 = falhou)
// ---------------------------------------------------------------------------
#include "../core/dsp.h"
#include <cstdio>
#include <cstring>
#include <string>

static int falhas = 0;

// Le g_permitida como uma string de 12 caracteres ("C.D.E F.G.A.B" -> "101011010101")
static std::string mascara() {
    std::string s(12, '0');
    for (int i = 0; i < 12; ++i) s[i] = g_permitida[i] ? '1' : '0';
    return s;
}

static void checar(const char* escala, const std::string& esperado, const char* porque) {
    definirEscala(escala);
    std::string got = mascara();
    bool ok = (got == esperado);
    if (!ok) ++falhas;
    std::printf("  %s  %-6s -> %s  %s\n", ok ? "ok  " : "FALHA", escala, got.c_str(),
                ok ? porque : ("ESPERADO " + esperado + " | " + porque).c_str());
}

// Monta a mascara esperada para uma tonica (0=C..11=B) e um modo.
static std::string esperada(int pc, bool menor) {
    static const int maior[7]  = {0, 2, 4, 5, 7, 9, 11};
    static const int menorN[7] = {0, 2, 3, 5, 7, 8, 10};
    const int* iv = menor ? menorN : maior;
    std::string s(12, '0');
    for (int i = 0; i < 7; ++i) s[(pc + iv[i]) % 12] = '1';
    return s;
}

int main() {
    static const char* TONICAS[12] =
        {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

    std::printf("== 1. REGRESSAO: as 6 escalas que o plugin oferecia antes ==\n");
    checar("crom", "111111111111", "cromatica: todas as 12");
    checar("C",  esperada(0,  false), "Do maior");
    checar("Am", esperada(9,  true ), "La menor");
    checar("G",  esperada(7,  false), "Sol maior");
    checar("Em", esperada(4,  true ), "Mi menor");
    checar("F",  esperada(5,  false), "Fa maior");
    checar("Dm", esperada(2,  true ), "Re menor");

    std::printf("\n== 2. COBERTURA: as 24 tonalidades ==\n");
    for (int pc = 0; pc < 12; ++pc) {
        for (int m = 0; m < 2; ++m) {
            std::string nome = std::string(TONICAS[pc]) + (m ? "m" : "");
            definirEscala(nome.c_str());
            std::string got = mascara(), exp = esperada(pc, m != 0);
            int n = 0; for (char c : got) if (c == '1') ++n;
            bool ok = (got == exp) && (n == 7);
            if (!ok) ++falhas;
            std::printf("  %s  %-4s -> %s (%d notas)\n", ok ? "ok  " : "FALHA",
                        nome.c_str(), got.c_str(), n);
        }
    }

    std::printf("\n== 3. BEMOIS equivalem aos sustenidos enarmonicos ==\n");
    struct { const char* b; const char* s; } enarm[] = {
        {"Db","C#"}, {"Eb","D#"}, {"Gb","F#"}, {"Ab","G#"}, {"Bb","A#"},
        {"Bbm","A#m"}, {"Ebm","D#m"} };
    for (auto& e : enarm) {
        definirEscala(e.b); std::string a = mascara();
        definirEscala(e.s); std::string b = mascara();
        bool ok = (a == b);
        if (!ok) ++falhas;
        std::printf("  %s  %-4s == %-4s  (%s)\n", ok ? "ok  " : "FALHA", e.b, e.s, a.c_str());
    }

    std::printf("\n== 4. ENTRADA INVALIDA cai em cromatico (nao trava) ==\n");
    for (const char* s : {"", "H", "xyz", "cromatica"}) {
        definirEscala(s);
        bool ok = (mascara() == "111111111111");
        if (!ok) ++falhas;
        std::printf("  %s  \"%s\" -> cromatico\n", ok ? "ok  " : "FALHA", s);
    }

    std::printf("\n== 5. OS 36 COMBOS DA GUI (12 tonicas x 3 modos) ==\n");
    // Percorre exatamente o que os dois ComboBox do plugin podem produzir.
    // montarEscala() e a MESMA funcao que a GUI chama (dsp.h), entao isto
    // verifica o caminho real, nao uma copia da logica.
    int combosOk = 0;
    for (int modo = 0; modo <= 2; ++modo) {
        for (int tonica = 0; tonica < 12; ++tonica) {
            std::string txt = montarEscala(tonica, modo);
            definirEscala(txt.c_str());
            std::string got = mascara();
            std::string exp = (modo == 0) ? std::string("111111111111")
                                          : esperada(tonica, modo == 2);
            if (got == exp) ++combosOk; else {
                ++falhas;
                std::printf("  FALHA tonica=%d modo=%d -> \"%s\" deu %s, esperado %s\n",
                            tonica, modo, txt.c_str(), got.c_str(), exp.c_str());
            }
        }
    }
    std::printf("  %s  %d/36 combos corretos\n", combosOk == 36 ? "ok  " : "FALHA", combosOk);

    std::printf("\n== 6. INDICE FORA DA FAIXA nao quebra ==\n");
    for (int t : {-1, 12, 99}) {
        definirEscala(montarEscala(t, 1).c_str());
        int n = 0; for (char c : mascara()) if (c == '1') ++n;
        bool ok = (n == 7);            // cai em C maior, nao trava nem zera
        if (!ok) ++falhas;
        std::printf("  %s  tonica=%-3d -> %d notas\n", ok ? "ok  " : "FALHA", t, n);
    }
    for (int m : {-1, 3, 99}) {
        definirEscala(montarEscala(0, m).c_str());
        bool ok = (mascara() == "111111111111");   // modo invalido -> cromatico
        if (!ok) ++falhas;
        std::printf("  %s  modo=%-3d   -> cromatico\n", ok ? "ok  " : "FALHA", m);
    }

    std::printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "TUDO OK",
                falhas, falhas == 1 ? "" : "s");
    return falhas ? 1 : 0;
}
