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

// ---------------------------------------------------------------------------
//  Piso da varredura da GUARDA CONTRA A SUBHARMONICA (ver candidato(), abaixo).
//
//  A guarda procura evidencia de um periodo mais CURTO que o admitido pela
//  tessitura, entao precisa de um piso proprio, abaixo do 'tauMin' da tessitura
//  mais aguda que o projeto oferece (Instrumento, 2000 Hz -> 22 amostras a
//  44,1 kHz). Oito amostras sao 5,5 kHz a 44,1 kHz: acima de qualquer
//  fundamental de voz, e longe do regime tau -> 0, onde a CMNDF nao quer dizer
//  nada -- dp[1] vale 1,0 por construcao, seja qual for o sinal.
//
//  E' constante do DETECTOR, nao parametro de usuario: ela nao expressa um gosto,
//  expressa ate onde a funcao de diferenca ainda tem significado.
// ---------------------------------------------------------------------------
inline constexpr int TAU_GUARDA_MIN = 8;

// ---------------------------------------------------------------------------
//  Limiar da GUARDA, e por que ele NAO e' o limiar da busca principal.
//
//  candidato() e' chamada 100 vezes por quadro, com limiares 's' varridos de uma
//  distribuicao Beta -- e' assim que o pYIN gera candidatos em vez de decidir de
//  uma vez. Varios desses limiares sao PERMISSIVOS de proposito (chegam perto de
//  1,0), porque o peso deles na agregacao e' minusculo e quem decide no fim e' o
//  HMM. Reaproveitar esse 's' na guarda seria um erro de tipo: um limiar feito
//  para GERAR hipoteses baratas passaria a REJEITAR quadros.
//
//  Medido em 03/09/2026, profundidade do vale abaixo de tauMin:
//    verdadeiro positivo (349 Hz cantado num preset de teto 330)  ->  dp = 0,001
//    falso positivo (take real, teto de 1000 Hz, longe de estourar) -> dp = 0,14 a 0,43
//  Duas ordens de grandeza separam os dois casos, entao a guarda exige um vale
//  FUNDO em termos absolutos, e nao "abaixo do limiar do momento".
//
//  O valor e' 0,10 porque e' o limiar absoluto do YIN original (de Cheveigne e
//  Kawahara, 2002, secao II-D) -- o mesmo numero que aquele algoritmo usa para
//  dizer "isto e' periodo, nao ruido". Nao e' constante escolhida a dedo para
//  fazer este take passar: e' o criterio de periodicidade da literatura, e a
//  medicao acima mostra que ele cai no meio da lacuna entre os dois casos.
//
//  Sem isto, tres quadros de exemplo-antes.wav viravam nao-vozeados por engano.
//  Tres em 854 parece pouco, e nao e': um quadro nao-vozeado PARTE uma regiao
//  vozeada, a cadeia de marcas do PSOLA re-ancora, e a correlacao da saida caia
//  para 0,77. Um erro de 0,35 % na trilha custava a frase inteira.
// ---------------------------------------------------------------------------
inline constexpr double LIMIAR_GUARDA = 0.10;

// ---------------------------------------------------------------------------
//  candidato() — escolhe um periodo a partir da CMNDF, para um limiar 's'.
//
//  GUARDA CONTRA A SUBHARMONICA (D1 de docs/spec-encaixe-e-estabilidade.md).
//
//  A busca percorre a faixa a partir de 'tauMin', o periodo mais CURTO admitido
//  pela tessitura. Se a altura cantada for mais aguda que o teto do preset, o
//  periodo real cai ABAIXO de tauMin e fica invisivel para a busca -- mas o
//  DOBRO dele nao fica, porque um sinal periodico em T tambem e' periodico em
//  2T. A busca achava o 2T e reportava METADE da frequencia.
//
//  Medido em 03/09/2026: 100 % dos quadros acima do teto, com a quebra caindo
//  exatamente no fmax de cada preset. Sobre exemplo-antes.wav, o preset Baixo
//  divergia por uma oitava em 34,3 % dos quadros vozeados, e o Low Male em
//  16,2 %. Os numeros estao congelados em src/tests/test_deteccao.cpp.
//
//  O defeito era ASSIMETRICO, e e' isso que o tornava traicoeiro:
//    altura abaixo do fmin -> "sem voz", nao corrige. Silencioso, mas honesto.
//    altura acima do fmax  -> uma oitava abaixo: corrige para a nota ERRADA.
//
//  A correcao NAO e' alargar a faixa percorrida. Isso exigiria mais bins no HMM
//  (o espaco de estados tem binDe(FMAX)+1 posicoes, e o codigo ja descarta
//  candidatos fora dele), ou seja, mais custo de Viterbi por quadro. A correcao
//  e' REJEITAR o candidato quando ha evidencia de que o periodo real esta fora
//  da faixa. O resultado passa a espelhar o que ja acontecia abaixo do piso --
//  fora da faixa, "sem voz" -- e sobra um unico comportamento para lembrar.
//
//  Custo zero: calcularCMNDF() ja preenche 'dp' de tau = 1 ate tauMax. Os dados
//  abaixo de tauMin ja existiam, apenas nao eram consultados. A guarda e' uma
//  leitura a mais, nao um calculo a mais.
//
//  Devolver 0.0 nao forca o quadro a "nao-vozeado" na marra: cada limiar 's' e'
//  um voto, e retirar o voto desloca massa do histograma de pitch para 'pUnv'.
//  Quem decide continua sendo o HMM, com a evidencia certa na mao.
// ---------------------------------------------------------------------------
inline double candidato(const std::vector<double>& dp, int tauMin, int tauMax, double s, int fs) {
    //  O que conta como evidencia: um VALE QUE FECHA abaixo de tauMin, e nao
    //  qualquer amostra abaixo do limiar. A diferenca decide o caso de borda, e
    //  ele nao e' raro -- e' exatamente a nota mais aguda que o preset promete
    //  cobrir. Cantando o proprio fmax (330 Hz no Baixo), o periodo real e' 133,6
    //  amostras e o tauMin e' 133: o vale legitimo fica CENTRADO no limite, e o
    //  flanco esquerdo dele cai dentro da regiao que a guarda varre. Uma guarda
    //  que dispare com "existe dp[tau] < s" rejeitaria a nota do teto -- trocando
    //  o defeito da oitava por um buraco no agudo da propria tessitura.
    //
    //  Seguir o vale ate o fundo separa os dois casos pela fisica, sem constante
    //  de ajuste: se o fundo cai abaixo de tauMin, existe mesmo um periodo mais
    //  curto que o admitido (altura acima do fmax); se o vale so ATRAVESSA
    //  tauMin, o que se viu foi o flanco do periodo legitimo mais grave.
    const int guardaAte = std::min(tauMin, (int)dp.size());
    const double sGuarda = std::min(s, LIMIAR_GUARDA);
    for (int tau = TAU_GUARDA_MIN; tau < guardaAte; ++tau) {
        if (dp[tau] >= sGuarda) continue;
        int fundo = tau;   // mesma descida que a busca principal faz, abaixo
        while (fundo + 1 < tauMax && dp[fundo + 1] < dp[fundo]) ++fundo;
        if (fundo < tauMin) return 0.0;   // altura acima do fmax -> sem voz
        break;                            // vale do periodo legitimo: segue
    }

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

// ---------------------------------------------------------------------------
//  A lista de tessituras EXPOSTA NA INTERFACE (o "Input Type" do Auto-Tune).
//
//  Ela e' MENOR que presetVoz(): quatro itens contra nove. Os nove continuam
//  resolvendo pela linha de comando, e isso e' deliberado -- eles sao DADOS DE
//  MEDICAO antes de serem itens de menu. Foi com os presets SATB que a tabela de
//  erro de oitava e a de cobertura de tessitura do spec foram levantadas, e o
//  texto do TCC cita as duas: jogar os nove fora destruiria a capacidade de
//  reproduzir numeros que o trabalho afirma.
//
//  A tabela mora AQUI, e nao na GUI, pela mesma razao que montarEscala() mora
//  aqui: para que a interface e os testes leiam a MESMA fonte. O modo de falha
//  concreto que isso evita ja existia -- nomeVoz() era um switch sobre o indice
//  do combo, com 'default: instrumento'. Encolher a lista sem encolher o switch
//  faria o indice 1 devolver "baritono" enquanto o combo mostrava outra coisa:
//  o menu diria uma tessitura e o DSP usaria outra, sem erro de compilacao e sem
//  teste falhando, porque nao havia nada ligando os dois.
//
//  A ORDEM E' DELIBERADA, e a razao precisa ficar junto da tabela senao a
//  proxima reordenacao a desfaz sem perceber. A lista encolheu de 7 para 4, e a
//  APVTS grava o indice do AudioParameterChoice: um projeto salvo com
//  'Contralto' (indice 3 da lista antiga) reabre no indice 3 da lista NOVA. Por
//  isso o indice 3 e' 'Instrument', a faixa mais larga e a unica imune ao erro
//  de oitava. O pior pouso possivel seria 'Low Male', cujo teto de 392 Hz corta
//  18,5 % do take medido. Isto nao e' sorte a herdar em silencio: e' escolha.
//
//  Os rotulos trazem a faixa em NOTAS, nao em Hz, porque e' a leitura que o
//  cantor consegue usar ("minha nota mais grave e' um Mi2, entao Low Male
//  serve"). src/tests/test_vozes.cpp confere que cada rotulo bate com o
//  presetVoz() correspondente -- e' o que prende o texto ao numero.
// ---------------------------------------------------------------------------
struct VozDaInterface {
    const char* rotulo;   // o que o combo mostra
    const char* preset;   // o nome que presetVoz() entende
};

inline constexpr int N_VOZES_UI = 4;

// Padrao de fabrica: Alto-Tenor. A medicao da §3 do spec mostra que ele cobre
// 100 % do take do autor, contra 98,6 % do Contralto (o padrao ate 03/09/2026),
// que cortava 1,4 % dos graves. Preco registrado: preset mais largo significa
// fs/FMIN maior, logo mais latencia no PSOLA -- 337 amostras de guarda contra
// 252 do Contralto.
inline constexpr int VOZ_UI_PADRAO = 1;

inline const VozDaInterface& vozDaInterface(int idx) {
    static const VozDaInterface tabela[N_VOZES_UI] = {
        { "Soprano (C4-C6)",    "soprano"     },
        { "Alto-Tenor (C3-F5)", "altotenor"   },   // padrao de fabrica
        { "Low Male (E2-G4)",   "lowmale"     },
        { "Instrument (G1-B6)", "instrumento" },   // indice 3: ver acima
    };
    if (idx < 0 || idx >= N_VOZES_UI) idx = N_VOZES_UI - 1;
    return tabela[idx];
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

// Monta a string que definirEscala() entende a partir de dois indices de combo.
// Existe aqui, e nao na GUI, para que a interface e os testes usem a MESMA
// tabela — o mesmo motivo que levou a malha de correcao para dsp.h (Etapa 0).
//   tonica: 0=C, 1=C#, 2=D, ... 11=B
//   modo:   0=cromatico (ignora a tonica), 1=maior, 2=menor natural
inline std::string montarEscala(int tonica, int modo) {
    if (modo <= 0 || modo > 2) return "crom";
    static const char* RAIZ[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (tonica < 0 || tonica > 11) tonica = 0;
    return std::string(RAIZ[tonica]) + (modo == 2 ? "m" : "");
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

// Nota-alvo: nota mais próxima da escala, com zona morta (tolCents).
//
// ETAPA 2 do plano: o parâmetro "forca" (0..1, fração do desvio a corrigir) foi
// REMOVIDO daqui. Ele não tinha equivalente no Auto-Tune e misturava duas coisas
// distintas — "quanto corrigir" e "quanto do efeito ouvir". A segunda virou o
// Mix seco/molhado (misturar(), abaixo); a primeira simplesmente não existe mais:
// a correção agora é sempre integral, e o que se dosa é a mistura.
//
// A zona morta continua sendo o único controle que decide o ALVO. Vale notar,
// para quem vier depois: uma tolerância maior que meio semitom (tolCents >= 50)
// faz mov=0 sempre, logo alvo == f, logo beta = 1 no PSOLA. É por isso que
// `tol=600` é o caso de teste que exercita o PSOLA inteiro em identidade —
// exatamente a cobertura que a antiga `forca=0` dava. Ver docs/execucao-do-plano.md,
// Etapa 2, onde a equivalência está verificada por checksum.
inline double notaAlvo(double f, double tolCents) {
    double midi = 69.0 + 12.0 * std::log2(f / 440.0);
    int alvo = notaMaisProximaMidi(f);
    double errCents = (alvo - midi) * 100.0;
    double mag = std::fabs(errCents);
    double mov = (mag <= tolCents) ? 0.0
                                   : (errCents > 0 ? 1.0 : -1.0) * (mag - tolCents);
    double corrMidi = midi + mov / 100.0;
    return 440.0 * std::pow(2.0, (corrMidi - 69.0) / 12.0);
}

// ---------------------------------------------------------------------------
//  Mistura seco/molhado (Etapa 2 do plano) — o substituto da "forca".
//
//  A troca não é de grau, é de natureza:
//    - a FORCA agia DENTRO da correção: o PSOLA recebia um alvo intermediário e
//      sintetizava um sinal que não era nem o original nem o corrigido;
//    - o MIX age DEPOIS: os dois sinais existem por inteiro e são cruzados
//      linearmente. O que se ouve em 50% é metade do original mais metade do
//      corrigido — não um terceiro sinal sintetizado num alvo intermediário.
//
//  Isso é o que todo plugin de efeito faz, e é o que a Antares expõe. Também é
//  o que torna `mix = 0` um bypass EXATO: nenhuma operação sobra no caminho.
//
//  ⚠️ ALINHAMENTO — a armadilha desta função. O "seco" tem de ser a MESMA
//  amostra que o "molhado". No caminho offline os dois vetores já são paralelos
//  e não há o que fazer. No STREAMING o molhado sai atrasado da latência do
//  motor: misturar o seco instantâneo com o molhado atrasado somaria o sinal com
//  uma cópia deslocada de si mesmo — um filtro-pente, claramente audível. Por
//  isso o process() indexa os dois pelo MESMO índice absoluto (ver
//  autotune_stream.h). Quem replicar esta mistura em outro lugar precisa
//  atrasar o seco na mesma medida.
//
//  Os extremos são casos exatos de propósito: mix=1 devolve o molhado bit a bit
//  e mix=0 devolve o seco bit a bit, sem passar por multiplicação nenhuma. É o
//  que sustenta o teste de identidade da etapa (test_mix.cpp).
// ---------------------------------------------------------------------------
inline float misturar(float seco, float molhado, double mix) {
    if (mix >= 1.0) return molhado;
    if (mix <= 0.0) return seco;
    return (float)(mix * (double)molhado + (1.0 - mix) * (double)seco);
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
//  ETAPA 3 -- a matematica mudou aqui. Ate a Etapa 2 a malha era:
//
//      estado = alpha*estado + (1-alpha)*alvoCents          (filtro sobre o ALVO)
//
//  Dois defeitos, ambos diagnosticados na documentacao tecnica (secao 8.2):
//
//   1) O filtro agia sobre o ALVO, que e' quase CONSTANTE dentro de uma nota --
//      ele converge em ~tau e depois nao faz mais nada. Consequencia: o vibrato
//      do cantor era destruido, porque o alvo nao vibra e a saida seguia o alvo.
//   2) O reset de ataque era para o ALVO: a nota nascia exatamente afinada, sem
//      trajeto. E' o "duro, estatico" que o teste de usuario reprovou.
//
//  A cadeia nova mantem DOIS estados de filtro, um sobre o alvo e outro sobre a
//  altura REAL do cantor, e combina assim:
//
//      outCents = LP(alvo) + k*(real - LP(real))
//               = LP(alvo) + k*HP(real)
//
//  Escrevendo HP(x) = x - LP(x) para o complementar do passa-baixa, o que essa
//  forma faz e' separar o que o cantor faz DEVAGAR (deriva de afinacao, que deve
//  ser corrigida) do que ele faz DEPRESSA (vibrato, que deve ser preservado). O
//  filtro deixa de agir sobre o alvo e passa a agir sobre a CORRECAO.
//
//  Por que isso funde o Glide no Retune Speed, em vez de trocar um pelo outro:
//
//   k = 0  ->  outCents = LP(alvo). E' LITERALMENTE a linha da Etapa 2. O
//              comportamento antigo nao se perde: vira um caso particular.
//   k = 1  ->  outCents = LP(alvo) + real - LP(real) = real + LP(alvo - real)
//                       = real + LP(correcao). E' a formulacao da patente de
//              Hildebrand (US 5.973.252): o filtro sobre a correcao, nao sobre
//              o alvo. Vibrato preservado.
//   k > 1  ->  com o alvo constante dentro da nota, LP(alvo) -> alvo e a saida
//              vira alvo + k*vibrato: vibrato EXAGERADO (o "Natural Vibrato"
//              positivo da Antares).
//
//  Resposta do passa-altas a um vibrato de frequencia f_v, com frequencia de
//  corte f_c = 1/(2*pi*tau):   G(f_v) = f_v / sqrt(f_v^2 + f_c^2).
//  Vibrato de cantor fica em ~5-7 Hz; com tau = 25 ms, f_c ~ 6,4 Hz, o que
//  atenua o vibrato a ~0,7. Nao e' descuido: tau curto corrige rapido E come
//  vibrato -- e' o compromisso central deste controle, nao um defeito dele.
//
//  RESSALVA (registrada no plano, secao 11.1): k = 0 reproduz o REGIME antigo,
//  nao o ATAQUE. A Etapa 2 iniciava o estado em alvoCents (nota nasce afinada);
//  a cadeia nova inicia em realCents (nota nasce onde o cantor cantou). Para
//  reproduzir a Etapa 2 EXATAMENTE sao precisos DOIS valores neutros: k = 0 e
//  a flag 'ataqueNoAlvo'. Ela existe so para o teste de nao-regressao e NAO e'
//  parametro de plugin -- o deslize de entrada e' comportamento fixo do produto.
// ---------------------------------------------------------------------------
//  ETAPA 4 -- Humanize. Do manual da Antares, literalmente: "applies a slower
//  Retune Speed only during the sustained portion of longer notes".
//
//  O problema que ele resolve e' um efeito colateral direto da Etapa 3. Um tau
//  unico serve a dois momentos que querem coisas opostas:
//
//    - no ATAQUE quer-se tau CURTO: a nota nasce onde o cantor a colocou e
//      precisa chegar a afinacao rapido, senao a entrada soa desafinada;
//    - na SUSTENTACAO quer-se tau LONGO: e' onde vive a expressao (vibrato,
//      deriva intencional), e corrigir depressa ali achata tudo.
//
//  Humanize desacopla os dois: o tau efetivo cresce conforme a nota se sustenta.
//
//      tauEff = tau * (1 + humanize * HUM_FATOR * rampa(t))
//      rampa(t) = 1 - exp(-t / HUM_SUSTENTACAO)      t = tempo desde o ataque
//
//  A rampa e' suave de proposito. Um limiar duro ("depois de X ms, troque o
//  tau") produziria um degrau na constante de tempo no meio da nota, e degrau
//  em filtro e' transitorio audivel. A exponencial da a mesma ideia sem a
//  descontinuidade -- e usa a mesma primitiva que o resto da malha.
//
//  Por que sao CONSTANTES e nao controles: o projeto ja tem 8 parametros. Estes
//  dois definem o que "sustentacao" QUER DIZER, e essa e' uma decisao de
//  desenho, nao um gosto do usuario -- que ja escolhe a intensidade pelo
//  proprio Humanize. Ficam nomeados aqui para poderem ser discutidos no texto.
inline constexpr double HUM_SUSTENTACAO = 0.200;  // s: constante da rampa ataque->sustentacao
inline constexpr double HUM_FATOR       = 3.0;    // humanize=1 -> tau ate 4x na sustentacao

//  ETAPA 5 -- Create Vibrato. Aqui o plugin deixa de so CORRIGIR e passa a
//  GERAR: um vibrato sintetico somado a altura de saida, com forma, taxa e
//  profundidade proprias, mais uma modulacao de amplitude em sincronia.
//
//  Nao confundir com o 'vibrato' (k) da Etapa 3, que sao coisas opostas:
//    - k preserva o vibrato que o CANTOR fez (nao inventa nada);
//    - Create Vibrato inventa um que o cantor NAO fez.
//  Os dois convivem: da para preservar o do cantor e somar um por cima. Soam
//  mal juntos em profundidade alta, e isso e' escolha do usuario, nao defeito.
//
//  ATRASO DE ENTRADA (onset). Vibrato que comeca junto com a nota soa
//  sintetico -- cantor nenhum entra vibrando. A Antares expoe isso como "Onset
//  Delay"/"Onset Rate"; aqui e' uma rampa fixa com a mesma forma da do Humanize,
//  reusando o contador 'desdeAtaque' que a Etapa 4 ja mantem. Sem controle
//  proprio, pelo mesmo motivo: define o que "entrada da nota" quer dizer.
inline constexpr double VIB_ONSET = 0.300;   // s: constante da rampa de entrada do vibrato
inline constexpr double VIB_AMP_DB = 3.0;    // dB de modulacao de amplitude em vibAmp=1

// Formas de onda do Create Vibrato. Valor em [-1,1] para uma fase em [0,1).
enum class FormaVibrato { Nenhuma = 0, Senoide = 1, Triangular = 2, Quadrada = 3 };

inline double formaVibrato(FormaVibrato f, double fase) {
    switch (f) {
        case FormaVibrato::Senoide:    return std::sin(2.0 * PI * fase);
        // Triangular: sobe de -1 a 1 na primeira metade, desce na segunda.
        case FormaVibrato::Triangular: return (fase < 0.5) ? (4.0 * fase - 1.0)
                                                           : (3.0 - 4.0 * fase);
        case FormaVibrato::Quadrada:   return (fase < 0.5) ? 1.0 : -1.0;
        default:                       return 0.0;
    }
}

struct ParamsCorrecao {
    // Etapa 2: 'forca' saiu daqui. Ela nao era um parametro da MALHA (nao tem
    // dimensao de tempo, nem decide o alvo) -- era uma dosagem de efeito, e
    // dosagem de efeito e' mix, aplicado depois do PSOLA. Ver misturar().
    double tolCents = 0.0;   // zona morta (cents) ao redor da nota-alvo
    double retuneMs = 0.0;   // Retune Speed: constante de tempo da correcao (ms)
    double vibrato  = 1.0;   // k: 0 = sem vibrato, 1 = preservado, >1 = exagerado
    double humanize = 0.0;   // 0..1: quanto o Retune Speed AFROUXA na sustentacao

    // Etapa 5 -- Create Vibrato (gerado, nao preservado; ver 'vibrato' acima).
    FormaVibrato vibForma = FormaVibrato::Nenhuma; // Nenhuma = desligado
    double vibTaxa  = 5.5;   // Hz
    double vibProf  = 0.0;   // cents de profundidade (0 = desligado)
    double vibAmp   = 0.0;   // 0..1: modulacao de amplitude em sincronia

    // Flag INTERNA (nao e' parametro de usuario): restaura o reset de ataque da
    // Etapa 2, em que a nota nascia no alvo. Com ataqueNoAlvo=true E vibrato=0,
    // a saida e' identica a da Etapa 2 amostra a amostra -- e' o que torna a
    // Etapa 3 verificavel por nao-regressao. Ver test_retune.cpp, secao 1.
    bool   ataqueNoAlvo = false;
};

class CorretorAltura {
public:
    void prepare(double fsHz) { fs = fsHz; reset(); }
    void reset() { lpAlvo = 0.0; lpReal = 0.0; tinhaNota = false; desdeAtaque = 0;
                   fase = 0.0; ganho = 1.0; }

    // Ganho de amplitude da ULTIMA amostra calculada por proxima() (Etapa 5).
    // Fica separado do retorno porque proxima() devolve ALTURA, e altura e
    // amplitude entram no sinal em pontos diferentes do pipeline: a altura
    // vai para o PSOLA, o ganho e aplicado depois dele.
    double ultimoGanho() const { return ganho; }

    // Devolve o pitch-alvo em Hz para uma amostra cujo F0 detectado e f0Hz.
    // f0Hz <= 0 marca trecho nao-vozeado: devolve 0 e rearma o ataque.
    double proxima(double f0Hz, const ParamsCorrecao& p) {
        if (f0Hz <= 0.0) { tinhaNota = false; ganho = 1.0; return 0.0; }

        const double realCents = 1200.0 * std::log2(f0Hz / FMIN);
        const double alvoCents = 1200.0 * std::log2(notaAlvo(f0Hz, p.tolCents) / FMIN);
        // Etapa 4: o tau EFETIVO cresce com o tempo desde o ataque, se Humanize
        // estiver ligado. O ramo humanize==0 e' exato de proposito: ele tem de
        // devolver a Etapa 3 bit a bit, e nao "praticamente igual".
        double tau = p.retuneMs / 1000.0;
        if (p.humanize > 0.0 && tau > 0.0) {
            const double t     = (double)desdeAtaque / fs;
            const double rampa = 1.0 - std::exp(-t / HUM_SUSTENTACAO);
            tau *= 1.0 + p.humanize * HUM_FATOR * rampa;
        }
        const double alpha     = (tau > 0.0) ? std::exp(-1.0 / (tau * fs)) : 0.0;

        if (!tinhaNota) {
            // ATAQUE. Os dois estados nascem na altura REAL do cantor, e por
            // isso a saida do primeiro instante e' exatamente ela:
            //     out = lpAlvo + k*(real - lpReal) = real + k*0 = real
            // qualquer que seja k. A nota nasce onde o cantor a colocou e
            // desliza dali ate o alvo em ~tau. (Com ataqueNoAlvo, lpAlvo nasce
            // no alvo e o gesto de entrada desaparece -- comportamento da Etapa 2.)
            //
            // PRECO, que precisa ficar no texto: um ataque errado fica audivel
            // por ~tau. Com tau = 100 ms e o cantor entrando 200 cents fora, da
            // para ouvir. E' o custo direto da naturalidade.
            lpAlvo = p.ataqueNoAlvo ? alvoCents : realCents;
            lpReal = realCents;
            tinhaNota = true;
            desdeAtaque = 0;
            fase = 0.0;
        } else {
            ++desdeAtaque;
            // Regime. Mesmo alpha nos dois: sao o mesmo filtro, aplicado a dois
            // sinais. Usar constantes de tempo diferentes quebraria a identidade
            // LP(alvo) + real - LP(real) = real + LP(alvo - real) que sustenta
            // a interpretacao de "filtro sobre a correcao".
            lpAlvo = alpha * lpAlvo + (1.0 - alpha) * alvoCents;
            lpReal = alpha * lpReal + (1.0 - alpha) * realCents;
        }

        double outCents = lpAlvo + p.vibrato * (realCents - lpReal);

        // Etapa 5: Create Vibrato. Os dois ramos de desligado (forma Nenhuma e
        // profundidade zero) sao saidas exatas -- nao pode sobrar aritmetica no
        // caminho, senao a etapa deixa de reproduzir a Etapa 4 bit a bit.
        ganho = 1.0;
        if (p.vibForma != FormaVibrato::Nenhuma && (p.vibProf > 0.0 || p.vibAmp > 0.0)) {
            // A fase avanca com a nota, e zera no ataque: o vibrato gerado
            // comeca sempre do mesmo ponto da forma de onda, em vez de pegar o
            // LFO onde ele estivesse. Sem isso, notas iguais soariam diferentes
            // conforme o instante em que comecassem.
            fase += p.vibTaxa / fs;
            if (fase >= 1.0) fase -= std::floor(fase);
            const double t     = (double)desdeAtaque / fs;
            const double onset = 1.0 - std::exp(-t / VIB_ONSET);
            const double lfo   = formaVibrato(p.vibForma, fase) * onset;
            outCents += p.vibProf * lfo;
            // Amplitude em sincronia com a altura: e' o que a Antares chama de
            // "Amplitude Amount". Em voz real as duas andam juntas, e vibrato
            // so de altura soa mecanico.
            if (p.vibAmp > 0.0)
                ganho = std::pow(10.0, (p.vibAmp * VIB_AMP_DB * lfo) / 20.0);
        }
        return FMIN * std::pow(2.0, outCents / 1200.0);
    }

private:
    double fs        = 44100.0;
    double lpAlvo    = 0.0;    // passa-baixa do ALVO, em cents acima de FMIN
    double lpReal    = 0.0;    // passa-baixa da altura REAL, mesma unidade
    bool   tinhaNota = false;  // false = proxima amostra vozeada e um ataque
    long long desdeAtaque = 0; // amostras desde o ataque da nota atual (Etapa 4)
    double fase   = 0.0;       // fase do LFO do Create Vibrato, em [0,1) (Etapa 5)
    double ganho  = 1.0;       // ganho de amplitude da ultima amostra (Etapa 5)
};

// ---------------------------------------------------------------------------
//  Parsing das flags "chave=valor" da malha de correcao, num lugar so.
//
//  Existe pela mesma razao que CorretorAltura existe (Etapa 0): os tres CLIs
//  liam as MESMAS flags, cada um com sua copia do if/else-if. Com 4 parametros
//  isso era chato; com 10 (Etapas 3-5) vira fonte garantida de divergencia --
//  um CLI aceitando 'vibprof=' e outro ignorando em silencio e' exatamente o
//  tipo de bug que nao aparece em teste nenhum, porque a flag ignorada nao
//  reclama.
//
//  Devolve true se a string foi reconhecida como flag desta malha.
// ---------------------------------------------------------------------------
inline bool lerFlagCorrecao(const std::string& a, ParamsCorrecao& p) {
    auto num = [&](size_t n) { return std::atof(a.c_str() + n); };
    if      (a.rfind("tol=",      0) == 0) p.tolCents = num(4);
    // 'glide=' e' o nome que 'retune=' tinha ate a Etapa 3; fica como apelido
    // para que comandos e scripts anteriores nao quebrem em silencio.
    else if (a.rfind("glide=",    0) == 0) p.retuneMs = num(6);
    else if (a.rfind("retune=",   0) == 0) p.retuneMs = num(7);
    else if (a.rfind("vibrato=",  0) == 0) p.vibrato  = num(8);
    else if (a.rfind("humanize=", 0) == 0) p.humanize = num(9);
    else if (a.rfind("vibtaxa=",  0) == 0) p.vibTaxa  = num(8);
    else if (a.rfind("vibprof=",  0) == 0) p.vibProf  = num(8);
    else if (a.rfind("vibamp=",   0) == 0) p.vibAmp   = num(7);
    else if (a.rfind("vibforma=", 0) == 0) {
        const int v = std::atoi(a.c_str() + 9);
        p.vibForma = (v >= 0 && v <= 3) ? (FormaVibrato)v : FormaVibrato::Nenhuma;
    }
    // Flag INTERNA de teste (ver ParamsCorrecao::ataqueNoAlvo). Fica aqui, e nao
    // escondida, porque o baseline.sh precisa dela nos tres CLIs.
    else if (a.rfind("legado=",   0) == 0) p.ataqueNoAlvo = (std::atoi(a.c_str() + 7) != 0);
    else return false;
    return true;
}

// Limites dos parametros da malha. Separado do parsing porque quem monta
// ParamsCorrecao sem passar por linha de comando (o plugin) tambem precisa.
inline void sanearCorrecao(ParamsCorrecao& p) {
    if (p.tolCents < 0.0) p.tolCents = 0.0;
    if (p.retuneMs < 0.0) p.retuneMs = 0.0;
    if (p.vibrato  < 0.0) p.vibrato  = 0.0;
    if (p.humanize < 0.0) p.humanize = 0.0;
    if (p.humanize > 1.0) p.humanize = 1.0;
    if (p.vibTaxa  < 0.1) p.vibTaxa  = 0.1;
    if (p.vibProf  < 0.0) p.vibProf  = 0.0;
    if (p.vibAmp   < 0.0) p.vibAmp   = 0.0;
    if (p.vibAmp   > 1.0) p.vibAmp   = 1.0;
}

// Resumo de uma linha dos parametros da malha, para o log dos CLIs.
inline std::string resumoCorrecao(const ParamsCorrecao& p) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "tol=%.0f ct | retune=%.0f ms | vibrato=%.2f | humanize=%.2f | createvib=%s%s",
        p.tolCents, p.retuneMs, p.vibrato, p.humanize,
        p.vibForma == FormaVibrato::Nenhuma ? "off" :
        p.vibForma == FormaVibrato::Senoide ? "sen" :
        p.vibForma == FormaVibrato::Triangular ? "tri" : "qua",
        p.ataqueNoAlvo ? " | LEGADO" : "");
    return std::string(buf);
}

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

// ----------------------------------------------------------------------------
//  MotorPonteiro — v3: correcao de altura por PONTEIRO DE LEITURA MOVEL, no
//  lugar do TD-PSOLA. E' o mecanismo da patente do Auto-Tune (US 5.973.252,
//  Hildebrand 1999) e o que o produto chama de "Classic Mode". Especificacao
//  completa em docs/especificacao-v3-ponteiro.md; a razao de existir esta em
//  docs/analise-v1-v2-v3.md: enquanto a sintese for PSOLA, a latencia e'
//  proporcional a fs/FMIN por construcao. Este motor tem latencia FIXA de
//  MARGEM amostras, mais uma parte VARIAVEL (0..T) que depende da nota cantada.
//
//  Como funciona, por amostra:
//    1. a entrada e' escrita num anel (ponteiro W, avanca 1 por amostra);
//    2. a saida e' LIDA do mesmo anel num ponteiro fracionario R que avanca
//       beta = fAlvo/f0 por amostra (beta > 1 = le mais rapido = sobe a nota);
//    3. como os dois andam a taxas diferentes, a distancia W - R muda. Quando
//       ela sai de [MARGEM, MARGEM + T], soma-se ou subtrai-se EXATAMENTE um
//       periodo T = fs/f0 de R: um ciclo e' repetido (subindo) ou descartado
//       (descendo). Como o salto e' de um periodo inteiro, as duas pontas da
//       emenda estao no mesmo ponto do ciclo -- e' a mesma ideia que sustenta
//       o PSOLA, sem quadros, sem marcas e sem janelas.
//
//  O que ele NAO faz: nao preserva formantes. Reamostrar desloca o envelope
//  espectral por |beta - 1|. Medido em docs/pesquisa-latencia-antares.md §10:
//  teto de 2,93 % em escala cromatica, abaixo do limiar tipico de 5 %.
//
//  Invariante que o teste verifica: com beta = 1 a saida e' a entrada
//  atrasada de MARGEM amostras, BIT A BIT. Depende de tres coisas: R nunca
//  sai da grade inteira (soma 1.0 exata), a interpolacao em fracao zero
//  devolve a amostra crua, e nenhum salto dispara.
//
//  RT-safe: o anel e' alocado em prepare(); processar() nao aloca.
// ----------------------------------------------------------------------------
class MotorPonteiro {
public:
    // Distancia minima entre leitura e escrita, em amostras. E' a latencia FIXA
    // declarada ao host. 8 porque a interpolacao cubica le ate i+2 e a distancia
    // cai no maximo (beta - 1) < 1 por amostra antes de o salto disparar: sobra
    // folga. (A Antares declara 37; o interpolador dela e' mais longo.)
    static constexpr int MARGEM = 8;

    void prepare(int fsHz, double fminHz) {
        fs = fsHz;
        // Precisa caber: a margem, ate T atras (salto para tras), mais um T de
        // folga para o ponteiro velho do crossfade, mais os 2 pontos que a
        // interpolacao le a frente. Potencia de 2 para que o wrap seja um '&'.
        const double T = (double)fs / std::max(1.0, fminHz);
        long long precisa = MARGEM + (long long)std::ceil(2.0 * T) + 8;
        size_t n = 16; while ((long long)n < precisa) n <<= 1;
        anel.assign(n, 0.0f); mask = n - 1;
        reset();
    }

    void reset() {
        std::fill(anel.begin(), anel.end(), 0.0f);
        // W comeca em N (nao em 0) para que R = W - MARGEM seja >= 0 e o
        // priming leia zeros do anel, sem indice negativo.
        W = (long long)anel.size(); R = (double)W - MARGEM; Rvelho = R;
        xfResta = 0; xfLen = 1;
        nSaltos = 0; somaDist = 0.0; nDist = 0; maxDist = 0.0;
    }

    int latencia() const { return MARGEM; }

    // f0Hz <= 0: sem voz -> beta = 1, nenhum salto (a distancia fica onde esta).
    float processar(float x, double f0Hz, double fAlvoHz) {
        anel[(size_t)(W & (long long)mask)] = x; ++W;
        const double beta = (f0Hz > 0.0 && fAlvoHz > 0.0) ? fAlvoHz / f0Hz : 1.0;

        double y = ler(R);
        if (xfResta > 0) {
            // Crossfade linear entre a leitura ANTIGA (que continua andando a
            // beta) e a nova. Em sinal periodico as duas sao quase iguais e o
            // crossfade e' quase nulo; em consoante/ataque ele transforma o
            // degrau numa modulacao curta de amplitude.
            const double w = 1.0 - (double)xfResta / (double)xfLen;
            y = (1.0 - w) * ler(Rvelho) + w * y;
            Rvelho += beta; --xfResta;
            // O ponteiro velho de um salto para tras estava perto da escrita e
            // continua se aproximando: se ameacar ler o futuro, encerra antes.
            // Dentro da faixa real de beta deste projeto isto nunca dispara (maximo
            // medido 1,008; teto 1,0595 em maior/menor, abaixo do 1,0625 que seria
            // preciso) -- mas em beta = 1,3 dispara em 100% dos saltos para tras. Nao
            // e' uma valvula de seguranca rara: e' o caminho normal logo depois do
            // teto documentado de deslocamento.
            if ((double)W - Rvelho < 4.0) xfResta = 0;
        }
        R += beta;

        if (f0Hz > 0.0) {
            const double T = (double)fs / f0Hz;
            const double dist = (double)W - R;
            if      (dist < (double)MARGEM)     saltar(-T, T);   // repete um ciclo
            else if (dist > (double)MARGEM + T) saltar(+T, T);   // descarta um ciclo
            const double d2 = (double)W - R;
            somaDist += d2; ++nDist; if (d2 > maxDist) maxDist = d2;
        }
        return (float)y;
    }

    long long saltos()    const { return nSaltos; }
    double    distMedia() const { return nDist ? somaDist / (double)nDist : (double)MARGEM; }
    double    distMax()   const { return maxDist; }

private:
    // Catmull-Rom de 4 pontos em torno de floor(pos). Em fracao ZERO devolve a
    // amostra crua sem passar pela aritmetica -- e' o que garante a identidade
    // bit a bit com beta = 1.
    double ler(double pos) const {
        const long long i = (long long)std::floor(pos);
        const double f = pos - (double)i;
        const double x0 = anel[(size_t)(i & (long long)mask)];
        if (f == 0.0) return x0;
        const double xm1 = anel[(size_t)((i - 1) & (long long)mask)];
        const double x1  = anel[(size_t)((i + 1) & (long long)mask)];
        const double x2  = anel[(size_t)((i + 2) & (long long)mask)];
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    void saltar(double delta, double T) {
        Rvelho = R; R += delta;
        xfLen = std::max(1, std::min(64, (int)std::llround(T / 2.0)));
        xfResta = xfLen;
        ++nSaltos;
    }

    int fs = 44100;
    std::vector<float> anel; size_t mask = 0;
    long long W = 0;             // escrita (absoluto)
    double R = 0.0, Rvelho = 0.0;// leitura (absoluto, fracionario) e leitura antiga do crossfade
    int xfResta = 0, xfLen = 1;
    long long nSaltos = 0; double somaDist = 0.0; long long nDist = 0; double maxDist = 0.0;
};

#endif // DSP_H
