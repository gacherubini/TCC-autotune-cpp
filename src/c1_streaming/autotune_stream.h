// ============================================================================
//  autotune_stream.h — CAMINHO C1: núcleo de STREAMING do autotune
//
//  Esta classe encapsula o autotune num formato compatível com um plugin de
//  áudio "real": o host chama prepare() uma vez (com a taxa de amostragem e os
//  parâmetros) e depois chama process() repetidamente, em blocos de tamanho
//  ARBITRÁRIO (definido pelo host, não pelo algoritmo).
//
//  TAREFA 1: apenas o ESQUELETO + um process() de IDENTIDADE (passthrough),
//  que copia a entrada para a saída sem nenhuma alteração. Objetivo: provar
//  que a estrutura (build + driver headless + leitura/escrita de WAV)
//  funciona ANTES de implementar a detecção de pitch causal e o PSOLA em
//  blocos (tarefas seguintes).
//
//  TAREFA 2 (este arquivo): adiciona o BUFFER CIRCULAR de entrada (ringIn) e
//  o laço de "fatiamento" de quadros — passos 1 e 2 do pipeline de streaming:
//    1) acumular as amostras recebidas em blocos de tamanho arbitrário num
//       buffer circular (ringIn), que sempre guarda pelo menos as últimas
//       'nFrame' amostras;
//    2) sempre que houver amostras suficientes acumuladas (>= nFrame) E já
//       tiver passado pelo menos 'nHop' amostras desde o último quadro,
//       "disparar" um novo quadro de análise (copiar as últimas nFrame
//       amostras do ring para o buffer 'work', contíguo).
//  Ainda NÃO há análise de pitch nem correção: process() apenas zera a saída
//  e registra (em framesDisparados) o índice teórico de início de cada
//  quadro disparado, para validar que o disparo ocorre nos passos corretos
//  (k*nHop) independentemente do tamanho de bloco do host.
// ============================================================================
#ifndef AUTOTUNE_STREAM_H
#define AUTOTUNE_STREAM_H
#include "../core/dsp.h"   // primitivas + constantes (NÃO define DR_WAV_IMPLEMENTATION aqui)

// ----------------------------------------------------------------------------
//  Parâmetros do streaming. Espelham as flags de linha de comando do
//  autotune_rt (forca, escala já é global via definirEscala/g_permitida,
//  tolCents, glideMs, look, frame/hop, fmin/fmax) para que o driver de teste
//  possa repassar os mesmos argumentos usados nas versões offline/causal.
// ----------------------------------------------------------------------------
struct StreamParams {
    double forca    = 1.0;     // 0..1: o quanto puxa em direção à nota da escala
    double tolCents = 0.0;     // zona morta (cents) ao redor da nota-alvo
    double glideMs  = 0.0;     // tempo de "deslize" até a nota-alvo (1 polo)
    int    look     = 4;       // look-ahead (em quadros) do Viterbi causal
    int    nFrame   = N_FRAME; // tamanho do quadro de análise de pitch
    int    nHop     = N_HOP;   // passo entre quadros de análise
    double fmin     = FMIN;    // faixa de busca de pitch (Hz)
    double fmax     = FMAX;
};

// ----------------------------------------------------------------------------
//  AutotuneStream — motor de streaming. Ciclo de vida esperado pelo host:
//      eng.prepare(fs, params);     // 1x, ao iniciar/trocar configuração
//      while (tem audio) eng.process(in, out, n);  // repetido, n = block size
//
//  TAREFA 1: process() era IDENTIDADE pura (out[i] = in[i]).
//  TAREFA 2: process() já mantém o buffer circular de entrada e dispara os
//  quadros de análise nos passos corretos (mas a saída ainda é zero —
//  a correção de pitch entra nas próximas tarefas).
// ----------------------------------------------------------------------------
class AutotuneStream {
public:
    // Configura a taxa de amostragem e os parâmetros, e (re)calcula a
    // latência algorítmica esperada. Também ajusta FMIN/FMAX globais (usados
    // por binDe/fDeBin e pelo termo PSOLA da latência em dsp.h), igual ao que
    // o autotune_rt faz a partir das flags de linha de comando.
    void prepare(double fsHz, const StreamParams& pr) {
        fs = (int)fsHz;
        p  = pr;
        FMIN = p.fmin;   // grade de pitch (binDe/fDeBin) usa estes globais
        FMAX = p.fmax;

        // Folga (look-ahead) do PSOLA online, em amostras. Precisa cobrir o
        // alcance do grão da ÚLTIMA marca da janela (cuja largura Tana é estimada
        // pela marca ANTERIOR, pois a próxima ainda não existe no streaming):
        // 2 períodos do tom mais grave garantem que esse grão "instável" nunca
        // alcança a região já finalizada -> streaming == lote. Usada IGUAL no
        // orçamento de latência e em avancarPsola() (devem casar exatamente).
        psolaGuard = 2 * (int)std::llround((double)fs / FMIN);

        // Orçamento de latência: quadro de análise + look-ahead do Viterbi +
        // folga do PSOLA (psolaGuard).
        latSamples = p.nFrame + p.look * p.nHop + psolaGuard;

        // -----------------------------------------------------------------
        // TAREFA 3: monta os "limiares" de Beta (pYIN) e as dimensões do HMM
        // (bins de pitch + estado "não-vozeado") — EXATAMENTE como o
        // autotune_rt faz antes do laço de quadros. Isso precisa acontecer
        // AQUI (antes de reset()), porque reset() usa nEst/look para
        // dimensionar o "anel" de colunas do Viterbi (psiRing).
        //
        //   sLim[k]/wLim[k]: K=100 limiares de threshold (distribuição Beta)
        //                    e seus pesos (normalizados a somar 1), usados
        //                    por candidato() para gerar os "votos" de pitch
        //                    de cada quadro (igual ao offline/autotune_rt).
        //   nBins/UV/nEst:   grade de pitch (bins de 'RES_CENTS' cents entre
        //                    FMIN e FMAX) + 1 estado extra "não-vozeado" (UV).
        //   W/tauMax/tauMin: janela do CMNDF (metade do quadro) e faixa de
        //                    períodos (tau) correspondente a [FMIN,FMAX].
        // -----------------------------------------------------------------
        K = 100; sLim.resize(K); wLim.resize(K); double sw = 0;
        for (int k = 0; k < K; ++k) { double s = (k + 0.5) / K; sLim[k] = s; wLim[k] = s * std::pow(1 - s, 17.0); sw += wLim[k]; }
        for (int k = 0; k < K; ++k) wLim[k] /= sw;
        nBins = binDe(FMAX) + 1; UV = nBins; nEst = nBins + 1;
        W = p.nFrame / 2; tauMax = W; tauMin = std::max(1, (int)std::floor((double)fs / FMAX));

        // A partir da Tarefa 2, prepare() SEMPRE termina chamando reset():
        // é o reset() que aloca o buffer circular (ringIn) e o buffer de
        // trabalho (work), e zera os contadores de estado. Sem isso, o
        // primeiro process() encontraria ringIn vazio (cap=0) e nunca
        // dispararia quadros.
        reset();
    }

    // Latência algorítmica total, em amostras (informativo para o host).
    int getLatencySamples() const { return latSamples; }

    // -------------------------------------------------------------------
    //  updateLiveParams() — CAMINHO C2 (plugin): atualiza, EM TEMPO REAL e
    //  sem realocar nada, os parâmetros que NÃO mudam as dimensões do motor
    //  (força, zona morta em cents, glide em ms). É seguro chamar dentro do
    //  process()/processBlock (RT-safe: só escreve escalares lidos depois por
    //  emitirAmostras). Já os parâmetros ESTRUTURAIS (look, fmin/fmax via voz,
    //  frame, hop) mudam ringbuffers/HMM/latência → exigem prepare() de novo.
    // -------------------------------------------------------------------
    void updateLiveParams(double forca, double tolCents, double glideMs) {
        p.forca = forca; p.tolCents = tolCents; p.glideMs = glideMs;
    }

    // -------------------------------------------------------------------
    //  Estado público de depuração (TAREFA 2):
    //  framesDisparados guarda, para cada quadro de análise disparado, o
    //  índice TEÓRICO de sua primeira amostra dentro do sinal (k*nHop), na
    //  ordem em que os quadros são disparados. Serve apenas para o teste
    //  de validação (bench_frames.py): comparamos essa sequência com o
    //  fatiamento direto do sinal inteiro (range(0, N-nFrame+1, nHop)).
    //  A Tarefa 3 vai trocar/usar esse "gancho" para chamar a análise de
    //  pitch real sobre o quadro recém-fatiado (work[]).
    // -------------------------------------------------------------------
    std::vector<long long> framesDisparados;   // só p/ teste (Tarefa 2)

    // -------------------------------------------------------------------
    //  TAREFA 3: trilha de F0 (Hz; 0 = não-vozeado), um valor por quadro
    //  EMITIDO pelo Viterbi de lag fixo (após o atraso de 'look' quadros).
    //  Usada por bench_pitch.py para comparar com a trilha do autotune_rt.
    // -------------------------------------------------------------------
    const std::vector<double>& getTrilhaF0() const { return trilhaF0; }

    // -------------------------------------------------------------------
    //  GUI do plugin (CAMINHO C2): valores "atuais" do pitch para o
    //  afinador em tempo real. Refletem a ÚLTIMA amostra empurrada a
    //  f0samp/foutSamp (passos 3 e 3b do autotune_rt); 0 = sem voz ainda
    //  (ou trecho não-vozeado).
    // -------------------------------------------------------------------
    float getF0Atual()   const { return f0samp.empty()   ? 0.0f : f0samp.back();   }
    float getFoutAtual() const { return foutSamp.empty() ? 0.0f : foutSamp.back(); }

    // Limpa/realoca o estado interno: buffer circular de entrada (ringIn),
    // buffer de trabalho (work) e todos os contadores do "relógio" de
    // disparo de quadros. Chamado por prepare() e pode ser chamado de novo
    // pelo host para reiniciar o stream sem reconfigurar parâmetros.
    void reset() {
        // ringIn precisa caber pelo menos 'nFrame' amostras; a folga de +8
        // é só uma margem de segurança para a aritmética de índices abaixo
        // (garante cap > nFrame, usado na conta de 'start' em fatiarQuadro).
        ringIn.assign(p.nFrame + 8, 0.0f);
        cap = (int)ringIn.size(); wpos = 0; buffered = 0; desdeUltimo = 0;
        primeiroQuadro = true; qIdx = 0; framesDisparados.clear();
        work.assign(p.nFrame, 0.0f);

        // -----------------------------------------------------------------
        // TAREFA 3: estado do HMM causal (Viterbi de lag fixo) + "anel" de
        // colunas psi.
        //   dPrev/delta: log-probabilidades acumuladas (coluna anterior /
        //                coluna sendo calculada) — mesmo papel do offline.
        //   obs/dp:      observação (histograma de pitch) e CMNDF do quadro
        //                atual (recalculados a cada quadro em passoPitch()).
        //   psiRing:     em vez de guardar TODAS as colunas psi (como o
        //                offline faz, em 'numQ' colunas), guardamos só as
        //                últimas 'look+1' — o suficiente para retroceder
        //                'look' passos e emitir o quadro (t-look).
        //   psiCount:    quantas colunas já foram empilhadas no anel (índice
        //                lógico do próximo quadro a entrar).
        //   primeiroPasso: true até o 1º quadro (sem transição, só emissão).
        //   trilhaF0:    F0 (Hz; 0=não-vozeado) de cada quadro EMITIDO, na
        //                ordem (t = 0,1,2,...) — saída desta tarefa.
        // -----------------------------------------------------------------
        dPrev.assign(nEst, NEG_INF); delta.assign(nEst, 0.0); obs.assign(nBins, 0.0);
        psiRing.assign(p.look + 1, std::vector<int>(nEst, UV));
        psiCount = 0; trilhaF0.clear(); primeiroPasso = true;

        // -----------------------------------------------------------------
        // Buffers de ACUMULAÇÃO que alimentam o PSOLA online (avancarPsola
        // re-sintetiza janelas deslizantes sobre eles):
        //   xAll:     TODAS as amostras de entrada recebidas, em ordem
        //             (igual ao 'x' que o autotune_rt lê do WAV inteiro).
        //   f0samp:   F0 real por amostra (passo 3 do autotune_rt), montado
        //             causalmente: cada quadro emitido "carimba" os próximos
        //             nHop sample-slots com seu F0.
        //   foutSamp: pitch-ALVO por amostra (passo 3b do autotune_rt), com
        //             força+tolerância+glide aplicados, montado em paralelo
        //             a f0samp (mesmo laço de glide de 1 polo).
        //   corretor: estado da malha de correcao (glide + reset no ataque),
        //             compartilhada com o gold e o causal via dsp.h.
        // -----------------------------------------------------------------
        xAll.clear(); f0samp.clear(); foutSamp.clear();
        corretor.prepare(fs);

        // -----------------------------------------------------------------
        // TAREFA 5: estado da SÍNTESE INCREMENTAL (PSOLA "online" via
        // re-síntese em janela deslizante + overlap-save).
        //   outBuf:    saída finalizada, indexada por amostra ABSOLUTA. Para
        //              C1 (correção headless) usamos um vetor crescente em vez
        //              de um anel circular — mais simples e sem bugs de wrap.
        //              O anel limitado é um refinamento da etapa C2.
        //   synthFront: nº de amostras de saída já FINALIZADAS em outBuf.
        //   lida:      nº de amostras já ENTREGUES ao host via process().
        // -----------------------------------------------------------------
        outBuf.clear(); synthFront = 0; lida = 0;
    }

    // -------------------------------------------------------------------
    //  process() — TAREFA 2: ring buffer + disparo de quadros.
    //
    //  Para cada amostra recebida:
    //    (a) escreve no buffer circular ringIn (sobrescrevendo a amostra
    //        mais antiga quando o ring já está cheio);
    //    (b) atualiza os contadores 'buffered' (quantas amostras válidas já
    //        passaram, até o limite cap) e 'desdeUltimo' (quantas amostras
    //        desde o último quadro disparado);
    //    (c) enquanto houver amostras suficientes para formar um quadro
    //        (buffered >= nFrame) E já tiver avançado pelo menos um hop
    //        (desdeUltimo >= nHop), fatia um novo quadro de 'nFrame'
    //        amostras (as mais recentes) para 'work[]' e registra seu
    //        índice teórico.
    //
    //  O 'while' (em vez de 'if') é importante: se o bloco do host for
    //  maior que 'nHop', várias condições de disparo podem ser satisfeitas
    //  dentro de UMA única amostra processada — então disparamos todos os
    //  quadros pendentes antes de seguir para a próxima amostra. Isso é o
    //  que garante que o resultado independe do tamanho de bloco do host.
    //
    //  out[i] ainda é 0 (sem correção/síntese) — isso vem nas Tarefas 4+.
    // -------------------------------------------------------------------
    void process(const float* in, float* out, int n) {
        for (int i = 0; i < n; ++i) {
            // Acumula CADA amostra de entrada, em ordem — espelha o vetor 'x'
            // (sinal inteiro) que o autotune_rt lê do WAV. É a base que o PSOLA
            // online (avancarPsola) re-sintetiza em janela deslizante.
            xAll.push_back(in[i]);
            ringIn[wpos] = in[i]; wpos = (wpos + 1) % cap;
            if (buffered < cap) ++buffered;
            ++desdeUltimo;
            while (buffered >= p.nFrame && desdeUltimo >= p.nHop) {
                fatiarQuadro();                 // copia últimas nFrame p/ work[]
                // O PRIMEIRO quadro dispara assim que há nFrame amostras
                // acumuladas (não esperamos um hop completo extra antes
                // dele); a partir do segundo quadro, cada disparo consome
                // exatamente um hop. 'desdeUltimo -= nHop' (em vez de = 0)
                // preserva o "troco" de amostras, permitindo múltiplos
                // disparos na mesma amostra quando o bloco do host é grande.
                if (primeiroQuadro) { desdeUltimo = 0; primeiroQuadro = false; }
                else                  desdeUltimo -= p.nHop;
                framesDisparados.push_back(qIdx * (long long)p.nHop);   // índice inicial teórico
                ++qIdx;
                passoPitch();                    // TAREFA 3: análise + Viterbi causal deste quadro
            }
        }
        // TAREFA 5: avança a síntese incremental (finaliza as amostras de
        // saída cuja decisão de pitch já estabilizou) e ENTREGA ao host as
        // 'n' próximas amostras finalizadas. Enquanto o pipeline ainda está
        // "enchendo" (priming = latência algorítmica), entregamos silêncio.
        avancarPsola();
        // O host recebe 'n' amostras por 'n' amostras de entrada: a saída é o
        // sinal corrigido ATRASADO de 'latSamples'. Para a amostra absoluta
        // 'lida' na linha-do-tempo do host, entregamos a amostra finalizada
        // que está 'latSamples' atrás (outBuf[lida - lat]). Durante o priming
        // (lida < lat) ou enquanto a síntese ainda não finalizou essa posição,
        // entregamos silêncio. 'lida' avança SEMPRE de 'n' (amostra-a-amostra
        // com a entrada) — é o índice que materializa o atraso fixo.
        for (int i = 0; i < n; ++i) {
            long long src = lida - (long long)latSamples;
            out[i] = (src >= 0 && src < synthFront) ? outBuf[(size_t)src] : 0.0f;
            ++lida;
        }
    }

private:
    // -------------------------------------------------------------------
    //  fatiarQuadro() — copia as últimas 'nFrame' amostras escritas em
    //  ringIn (terminando na posição (wpos-1), com wrap-around) para o
    //  buffer contíguo 'work[]'.
    //
    //  Matemática do índice 'start' (a posição da amostra mais ANTIGA do
    //  quadro dentro do ring):
    //    queremos voltar 'nFrame' posições a partir de 'wpos' (a próxima
    //    posição livre, ou seja, 1 além da última amostra escrita), com
    //    aritmética modular (wrap). Em C++, o operador % de inteiros pode
    //    retornar valores negativos se o operando for negativo, então
    //    somamos 'cap' antes do módulo para garantir um resultado >= 0:
    //        start = (wpos - nFrame + cap) % cap
    //    Como cap = nFrame + 8 > nFrame, o termo (wpos - nFrame + cap) já é
    //    sempre >= 0 (pois wpos >= 0 e cap - nFrame = 8 > 0), então o '% cap'
    //    serve apenas para "dobrar" valores >= cap de volta para [0, cap).
    //
    //  Exemplo mental (cap=1032, nFrame=1024): após escrever exatamente
    //  1024 amostras a partir de wpos=0, temos wpos=1024. Então
    //  start = (1024 - 1024 + 1032) % 1032 = 1032 % 1032 = 0 — correto, as
    //  1024 amostras escritas ocupam ringIn[0..1023].
    // -------------------------------------------------------------------
    void fatiarQuadro() {
        int start = (wpos - p.nFrame + cap) % cap;
        for (int k = 0; k < p.nFrame; ++k) work[k] = ringIn[(start + k) % cap];
    }

    // -------------------------------------------------------------------
    //  calcularCMNDF_local() — calcula o CMNDF (Cumulative Mean Normalized
    //  Difference Function, base do YIN) do quadro recém-fatiado em
    //  'work[]'. Como 'work' já é o quadro ISOLADO (contíguo, nFrame
    //  amostras, mais recente primeiro... na verdade em ordem cronológica),
    //  chamamos calcularCMNDF com início 0 — equivalente ao
    //  'calcularCMNDF(x, q*nHop, W, tauMax, dp)' do autotune_rt, mas sobre
    //  uma cópia local que já começa exatamente onde o quadro q começaria
    //  no sinal inteiro. W=nFrame/2 (definido em prepare()).
    // -------------------------------------------------------------------
    void calcularCMNDF_local() {
        static thread_local std::vector<float> tmp;
        tmp = work;                                // work tem nFrame amostras
        calcularCMNDF(tmp, 0, W, tauMax, dp);
    }

    // -------------------------------------------------------------------
    //  passoPitch() — TAREFA 3: análise de pitch CAUSAL de UM quadro
    //  (o que acabou de ser fatiado em work[]) + um passo do Viterbi de
    //  lag fixo, espelhando EXATAMENTE a matemática do autotune_rt (mesmas
    //  constantes W_TRANS/SIGMA_TRANS/LOG_STAY_*/LOG_SWITCH, mesmas fórmulas
    //  de transição/emissão), porém de forma "online":
    //    - guardamos só as últimas (look+1) colunas 'psi' (psiRing), em vez
    //      de TODAS as colunas (numQ) como o offline faz;
    //    - a cada novo quadro, EMITIMOS o F0 do quadro (t-look) retrocedendo
    //      'look' passos a partir do melhor estado da coluna mais recente —
    //      o mesmo que o offline faz no laço final 'for f=e..t+1: s=psi[f][s]',
    //      só que aqui o "horizonte" e é sempre a coluna recém-calculada.
    // -------------------------------------------------------------------
    void passoPitch() {
        // 3. observação deste quadro (work[] tem nFrame amostras contíguas):
        //    CMNDF -> K=100 candidatos de pitch (limiares de Beta) -> histo-
        //    grama 'obs' por bin de pitch + massa não-vozeada 'pUnv'.
        calcularCMNDF_local();                 // preenche dp a partir de work[]
        std::fill(obs.begin(), obs.end(), 0.0); double massa = 0;
        for (int k = 0; k < K; ++k) {
            double f = candidato(dp, tauMin, tauMax, sLim[k], fs);
            if (f > 0) { int b = binDe(f); if (b >= 0 && b < nBins) { obs[b] += wLim[k]; massa += wLim[k]; } }
        }
        double pUnv = 1.0 - massa;
        auto emiss = [&](int s) { return (s == UV) ? std::log(pUnv + EPS) : std::log(obs[s] + EPS); };

        // 4. um passo do Viterbi (forward): calcula a coluna 'delta' deste
        //    quadro a partir de 'dPrev' (coluna anterior), guardando os
        //    "ponteiros de retrocesso" (psiCol) — igual ao offline, mas só
        //    para esta coluna.
        std::vector<int> psiCol(nEst, UV);
        if (primeiroPasso) {
            // 1º quadro: sem transição, só emissão (igual ao offline q==0).
            for (int s = 0; s < nEst; ++s) dPrev[s] = emiss(s);
            primeiroPasso = false;
        } else {
            // estado vozeado de maior probabilidade na coluna anterior
            // (usado pela transição UV -> vozeado, "retomar o pitch antigo").
            double maxV = NEG_INF; int argV = -1;
            for (int b = 0; b < nBins; ++b) if (dPrev[b] > maxV) { maxV = dPrev[b]; argV = b; }
            // transições para cada bin vozeado b2: ou veio de um bin vizinho
            // b1 (gaussiana de largura SIGMA_TRANS, janela +-W_TRANS), ou veio
            // do estado não-vozeado (LOG_SWITCH).
            for (int b2 = 0; b2 < nBins; ++b2) {
                double best = NEG_INF; int arg = -1;
                int lo = std::max(0, b2 - W_TRANS), hi = std::min(nBins - 1, b2 + W_TRANS);
                for (int b1 = lo; b1 <= hi; ++b1) {
                    double d = b2 - b1;
                    double v = dPrev[b1] + LOG_STAY_V - 0.5 * (d / SIGMA_TRANS) * (d / SIGMA_TRANS);
                    if (v > best) { best = v; arg = b1; }
                }
                double vu = dPrev[UV] + LOG_SWITCH;
                if (vu > best) { best = vu; arg = UV; }
                delta[b2] = best + emiss(b2); psiCol[b2] = arg;
            }
            // transição para o estado não-vozeado: ou ficou não-vozeado
            // (LOG_STAY_UV), ou veio do melhor bin vozeado (LOG_SWITCH).
            double bUV = dPrev[UV] + LOG_STAY_UV; int aUV = UV;
            if (maxV + LOG_SWITCH > bUV) { bUV = maxV + LOG_SWITCH; aUV = argV; }
            delta[UV] = bUV + emiss(UV); psiCol[UV] = aUV;
            std::swap(delta, dPrev);
        }

        // guarda a coluna psi deste quadro no anel circular (últimas look+1)
        psiRing[psiCount % (p.look + 1)] = psiCol; ++psiCount;

        // 5. EMISSÃO de lag fixo: assim que houver 'look' colunas após a do
        //    quadro a emitir, retrocede 'look' passos a partir do melhor
        //    estado da coluna mais recente (dPrev) — equivalente ao laço
        //    final do offline 'for f=e..t+1: s=psi[f][s]' com e=t+look.
        if (psiCount > p.look) {
            double best = NEG_INF; int s = UV;
            for (int e = 0; e < nEst; ++e) if (dPrev[e] > best) { best = dPrev[e]; s = e; }
            for (int step = 0; step < p.look; ++step) {
                int col = (int)((psiCount - 1 - step) % (p.look + 1));
                s = psiRing[col][s];
            }
            double f0q = (s == UV) ? 0.0 : fDeBin(s);
            trilhaF0.push_back(f0q);     // bench_pitch.py continua usando isso
            // TAREFA 4: este quadro emitido governa os próximos 'nHop'
            // sample-slots de f0samp/foutSamp (passos 3 e 3b do autotune_rt).
            emitirAmostras(f0q);
        }
    }

    // -------------------------------------------------------------------
    //  emitirAmostras() — TAREFA 4, passo 2: para o quadro recém-emitido
    //  (F0 = f0q, em Hz; 0 = não-vozeado), preenche 'nHop' posições de
    //  f0samp (passo 3) e foutSamp (passo 3b do autotune_rt), aplicando o
    //  mesma malha de correcao de dsp.h (glide + reset no ataque, via
    //  CorretorAltura). É a versão CAUSAL/incremental do laço 3b: em vez de
    //  varrer todo o vetor f0samp[N] de uma vez, processa 'nHop' amostras
    //  por chamada, na ordem em que os quadros são emitidos — produzindo
    //  EXATAMENTE a mesma sequência de estados de glide que o offline.
    // -------------------------------------------------------------------
    void emitirAmostras(double f0q) {
        // Etapa 0 do plano: a malha de correcao mora em dsp.h (CorretorAltura),
        // identica a usada pelo gold e pelo causal. O estado (glide + ataque)
        // vive em 'corretor', membro desta classe, e atravessa as chamadas de
        // process() como o resto do estado de streaming.
        ParamsCorrecao pc; pc.forca = p.forca; pc.tolCents = p.tolCents; pc.glideMs = p.glideMs;
        for (int k=0;k<p.nHop;++k) {
            f0samp.push_back((float)f0q);
            foutSamp.push_back((float)corretor.proxima(f0q, pc));
        }
    }

    // -------------------------------------------------------------------
    //  avancarPsola() — TAREFA 5: avança a SÍNTESE incremental por
    //  re-síntese em JANELA DESLIZANTE com overlap-save.
    //
    //  Ideia: em vez de tornar a detecção de marcas do PSOLA "online" (de
    //  alto risco), reaproveitamos a MESMA psolaSintetiza JÁ VALIDADA. A
    //  cada chamada re-sintetizamos uma JANELA do sinal acumulado que vai de
    //  uma 'margem' ANTES do que já foi finalizado até a fronteira de decisão
    //  atual, e COMETEMOS apenas o "miolo" novo [synthFront, alvo). A margem
    //  dá às marcas de período contexto suficiente para estabilizar, de modo
    //  que as amostras cometidas batem com o que o PSOLA em lote produziria.
    // -------------------------------------------------------------------
    void avancarPsola() {
        // Fronteira de DECISÃO em amostras = quantas amostras de f0/fout já
        // foram emitidas. Limitamos a xAll por segurança (xAll cresce por
        // amostra de entrada; f0samp por quadro emitido — então normalmente
        // xAll.size() >= f0samp.size(), mas o clamp evita qualquer leitura
        // fora de limites).
        long long decis = std::min<long long>((long long)f0samp.size(), (long long)xAll.size());
        // Deixa ~1 período de folga (look-ahead do PSOLA) antes de finalizar.
        long long guarda = psolaGuard;                   // mesma folga do latSamples
        long long alvo = decis - guarda;                 // até onde podemos finalizar agora
        if (alvo <= synthFront) return;                  // nada novo a finalizar

        // Janela re-sintetizada: começa 'margem' antes do que já foi
        // finalizado, para dar contexto às marcas de período; termina na
        // fronteira de decisão.
        long long margem = (long long)p.nFrame;           // ~1 quadro de histórico
        long long winStart = std::max(0LL, synthFront - margem);

        // CRÍTICO p/ casar com o PSOLA em lote: as MARCAS de período do PSOLA
        // são detectadas por REGIÃO vozeada, encadeadas a partir do 1º pico da
        // região. Se 'winStart' cair NO MEIO de uma região vozeada, a cadeia
        // de marcas recomeça num pico/ancora diferente -> fase diferente do
        // lote nas amostras cometidas. Então recuamos 'winStart' até o INÍCIO
        // da região vozeada que o contém (primeira amostra após um trecho
        // não-vozeado, ou o começo do sinal), garantindo a MESMA âncora do
        // lote. Como a síntese só COMETE [synthFront, alvo), o resto da região
        // (ainda não finalizado) será re-sintetizado na próxima janela com a
        // mesma âncora — overlap-save consistente.
        while (winStart > 0 && f0samp[winStart - 1] > 0.0f) --winStart;

        long long winEnd   = decis;
        long long len = winEnd - winStart;

        std::vector<float> xv(xAll.begin()+winStart,     xAll.begin()+winEnd);
        std::vector<float> f0(f0samp.begin()+winStart,   f0samp.begin()+winEnd);
        std::vector<float> fo(foutSamp.begin()+winStart, foutSamp.begin()+winEnd);
        std::vector<float> y = psolaSintetiza(xv, len, f0, fo, fs);

        // Compromete só o miolo novo [synthFront, alvo) (overlap-save).
        if ((long long)outBuf.size() < alvo) outBuf.resize(alvo, 0.0f);
        for (long long i = synthFront; i < alvo; ++i) outBuf[i] = y[i - winStart];
        synthFront = alvo;
    }

    // ---- estado configurado por prepare() ----
    int fs = 44100;        // taxa de amostragem (Hz)
    int latSamples = 0;    // latência algorítmica total (amostras)
    int psolaGuard = 0;    // folga (look-ahead) do PSOLA online = 2*fs/FMIN
    StreamParams p;        // cópia dos parâmetros recebidos em prepare()

    // ---- estado do buffer circular e do "relógio" de disparo de quadros ----
    std::vector<float> ringIn;   // buffer circular de entrada (cap amostras)
    std::vector<float> work;     // último quadro fatiado (nFrame amostras, contíguo)
    int cap = 0;                 // capacidade do ring (= nFrame + 8)
    int wpos = 0;                // próxima posição livre em ringIn (escrita circular)
    int buffered = 0;            // nº de amostras válidas já recebidas (até cap)
    int desdeUltimo = 0;         // amostras desde o último quadro disparado
    bool primeiroQuadro = true;  // true até o 1º quadro ser disparado
    long long qIdx = 0;          // índice sequencial do próximo quadro a disparar

    // ---- TAREFA 3: dimensões do HMM e limiares de Beta (calc. em prepare()) ----
    int K = 0;                   // nº de limiares de threshold (candidatos por quadro)
    int nBins = 0;                // nº de bins de pitch entre FMIN e FMAX
    int UV = 0;                   // índice do estado "não-vozeado" (== nBins)
    int nEst = 0;                 // nº total de estados do HMM (nBins + 1)
    int W = 0;                    // janela do CMNDF (nFrame/2)
    int tauMax = 0, tauMin = 0;   // faixa de períodos (amostras) para FMIN..FMAX
    std::vector<double> sLim, wLim;  // limiares de Beta e pesos normalizados

    // ---- TAREFA 3: estado do Viterbi causal (lag fixo) ----
    long long psiCount = 0;          // nº de colunas psi já empilhadas
    bool primeiroPasso = true;       // true até o 1º quadro ser processado
    std::vector<double> dPrev, delta, obs, dp;  // colunas do HMM + observação/CMNDF
    std::vector<std::vector<int>> psiRing;      // anel circular de colunas psi (look+1)
    std::vector<double> trilhaF0;               // F0 (Hz) emitido por quadro

    // ---- acumuladores que alimentam o PSOLA online (avancarPsola) ----
    std::vector<float> xAll;       // todas as amostras de entrada, em ordem
    std::vector<float> f0samp;     // F0 real por amostra (passo 3 do autotune_rt)
    std::vector<float> foutSamp;   // pitch-alvo por amostra, com glide (passo 3b)
    CorretorAltura corretor;       // malha de correcao (dsp.h) — glide + reset no ataque

    // ---- TAREFA 5: síntese incremental (re-síntese em janela + overlap-save) ----
    std::vector<float> outBuf;     // saída finalizada, indexada por amostra absoluta (C2 trocará por anel limitado)
    long long synthFront = 0;      // nº de amostras de saída já finalizadas
    long long lida = 0;            // nº de amostras já entregues ao host via process()
};

#endif // AUTOTUNE_STREAM_H
