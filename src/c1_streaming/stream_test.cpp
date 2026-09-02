// ============================================================================
//  stream_test — driver HEADLESS para o núcleo de streaming (AutotuneStream)
//
//  CAMINHO C1, TAREFA 1: este programa simula um host de áudio chamando
//  process() em blocos de tamanho 'block' (parâmetro de linha de comando),
//  para qualquer tamanho de bloco — exatamente como faria um plugin VST/AU em
//  tempo real, em que o host decide o tamanho do buffer de callback.
//
//  Lê um WAV (mono ou multi-canal, convertido para mono por média), processa
//  em fatias de 'block' amostras chamando AutotuneStream::process(), e grava
//  o resultado em 16-bit PCM. Na Tarefa 1, process() é identidade pura, então
//  a saída deve ser BIT-A-BIT (a menos de quantização 16-bit) igual à entrada,
//  para qualquer 'block' — isso é o que valida o pipeline de build + I/O.
//
//  Uso: stream_test.exe <in.wav> [out.wav] [mix] [escala] [tol=] [retune=] [vibrato=] [humanize=] [vib*=] [look=]
//                       [frame=] [hop=] [voz=] [fmin=] [fmax=] [block=B] [dumpf0=arq] [dumpbeta=arq]
//                       [motor=psola|ponteiro] [lowlat=1]
// ============================================================================
#define DR_WAV_IMPLEMENTATION
#include "../core/dsp.h"
#include "autotune_stream.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("Uso: %s <in.wav> [out.wav] [mix] [escala] [flags...] [motor=psola|ponteiro] [lowlat=1] [block=B]\n", argv[0]); return 1; }

    // ------------------------------------------------------------------
    // 1. Leitura dos argumentos de linha de comando (mesmo formato do
    //    autotune_rt, + a flag block= que define o tamanho do bloco do
    //    "host" simulado). dumpf0= é aceita por compatibilidade de
    //    interface, mas ainda não é usada na Tarefa 1 (sem detecção de pitch).
    // ------------------------------------------------------------------
    const char* saida = (argc >= 3) ? argv[2] : "saida_stream.wav";
    StreamParams p;
    // ETAPA 2: 3o posicional era 'forca', agora e' 'mix' (seco/molhado).
    p.mix = (argc >= 4) ? std::atof(argv[3]) : 1.0;
    if (p.mix < 0) p.mix = 0; if (p.mix > 1) p.mix = 1;
    std::string a4 = (argc >= 5) ? argv[4] : "";
    const char* escalaTxt = (!a4.empty() && a4.find('=') == std::string::npos) ? argv[4] : "crom";
    definirEscala(escalaTxt);
    int block = 128; std::string vozNome, dumpF0Path, dumpFramesPath, dumpBetaPath; bool fminExpl=false, fmaxExpl=false;
    bool lowlat = false; // Etapa 6: guardado no laco e aplicado DEPOIS dele (ver abaixo), para
                          // vencer um look= que apareca antes OU depois na linha de comando.
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if      (lerFlagCorrecao(a, p.corr)) { /* flag da malha (dsp.h) */ }
        else if (a.rfind("look=",0)==0)  p.look     = std::atoi(a.c_str()+5);
        else if (a.rfind("frame=",0)==0) p.nFrame   = std::atoi(a.c_str()+6);
        else if (a.rfind("hop=",0)==0)   p.nHop     = std::atoi(a.c_str()+4);
        else if (a.rfind("voz=",0)==0)   vozNome    = a.c_str()+4;
        else if (a.rfind("fmin=",0)==0){ p.fmin=std::atof(a.c_str()+5); fminExpl=true; }
        else if (a.rfind("fmax=",0)==0){ p.fmax=std::atof(a.c_str()+5); fmaxExpl=true; }
        else if (a.rfind("block=",0)==0) block      = std::atoi(a.c_str()+6);
        else if (a.rfind("dumpf0=",0)==0)dumpF0Path = a.c_str()+7;
        else if (a.rfind("dumpframes=",0)==0) dumpFramesPath = a.c_str()+11;
        else if (a.rfind("dumpbeta=",0)==0)   dumpBetaPath   = a.c_str()+9;
        else if (a.rfind("motor=",0)==0) {
            std::string m = a.c_str()+6;
            p.motor = (m == "ponteiro" || m == "v3") ? MotorSintese::Ponteiro : MotorSintese::PSOLA;
        }
        // lowlat=1 reproduz o botao do plugin: motor de ponteiro E look = 0.
        else if (a.rfind("lowlat=",0)==0 && std::atoi(a.c_str()+7) != 0) {
            lowlat = true;
        }
    }
    // Preset de tessitura (igual ao autotune_rt): fmin=/fmax= explícitos vencem.
    if (!vozNome.empty()) { double pf,px; if (presetVoz(vozNome,pf,px)) { if(!fminExpl)p.fmin=pf; if(!fmaxExpl)p.fmax=px; } }
    // lowlat= tem de vencer um look= que venha antes OU depois dele na linha de
    // comando: por isso e' aplicado aqui, depois do laco, antes da sanitizacao.
    if (lowlat) { p.motor = MotorSintese::Ponteiro; p.look = 0; }
    // Sanitização básica dos parâmetros (mesmos limites do autotune_rt).
    sanearCorrecao(p.corr);
    if (p.look<0) p.look=0; if (p.nFrame<128) p.nFrame=128; if (p.nHop<1) p.nHop=1;
    if (p.nHop>p.nFrame) p.nHop=p.nFrame; if (block<1) block=1;

    // ------------------------------------------------------------------
    // 2. Leitura do WAV de entrada e conversão para mono (média dos canais).
    // ------------------------------------------------------------------
    unsigned canais=0, taxa=0; drwav_uint64 nn=0;
    float* dados = drwav_open_file_and_read_pcm_frames_f32(argv[1], &canais, &taxa, &nn, nullptr);
    if (!dados) { std::printf("ERRO: nao abriu '%s'\n", argv[1]); return 1; }
    long long N = (long long)nn;
    std::vector<float> x(N), out(N, 0.0f);
    for (long long i=0;i<N;++i){ float s=0; for(unsigned c=0;c<canais;++c) s+=dados[i*canais+c]; x[i]=s/(float)canais; }
    drwav_free(dados, nullptr);
    int fs = (int)taxa;

    // ------------------------------------------------------------------
    // 3. Loop "host": chama process() em fatias de 'block' amostras. O
    //    último bloco pode ser menor (resto de N/block) — process() recebe
    //    'nb' explicitamente, então isso é tratado normalmente.
    // ------------------------------------------------------------------
    AutotuneStream eng; eng.prepare(fs, p);
    for (long long off=0; off<N; off+=block) {
        int nb = (int)std::min((long long)block, N-off);
        eng.process(&x[off], &out[off], nb);
    }

    // ------------------------------------------------------------------
    // 3b. (TAREFA 2) Despeja os índices teóricos dos quadros disparados,
    //     um por linha, em 'dumpFramesPath' — usado por bench_frames.py
    //     para validar que o disparo de quadros (passos 1-2 do pipeline)
    //     bate com o fatiamento direto do sinal inteiro, para qualquer
    //     tamanho de bloco do host.
    // ------------------------------------------------------------------
    if (!dumpFramesPath.empty()) {
        FILE* fp = std::fopen(dumpFramesPath.c_str(), "w");
        for (long long idx : eng.framesDisparados) std::fprintf(fp, "%lld\n", idx);
        std::fclose(fp);
        std::printf("frames disparados: %zu\n", eng.framesDisparados.size());
    }

    // ------------------------------------------------------------------
    // 3c. (TAREFA 3) Despeja a trilha de F0 (Hz; 0=não-vozeado) emitida
    //     pelo Viterbi causal de lag fixo, um valor por quadro, em
    //     'dumpF0Path' — usado por bench_pitch.py para comparar com a
    //     trilha do autotune_rt (gold).
    // ------------------------------------------------------------------
    if (!dumpF0Path.empty()) {
        FILE* fp = std::fopen(dumpF0Path.c_str(), "w");
        for (double f : eng.getTrilhaF0()) std::fprintf(fp, "%.4f\n", f);
        std::fclose(fp);
        std::printf("F0 streaming: %zu quadros\n", eng.getTrilhaF0().size());
    }

    // ------------------------------------------------------------------
    // 3d. (ANALISE) Despeja o par (f0 detectado, pitch-alvo) por amostra em
    //     'dumpBetaPath'. A razao fout/f0 e' o fator de deslocamento beta
    //     que o PSOLA aplica — e e' exatamente o fator pelo qual um motor de
    //     reamostragem (ponteiro movel) deslocaria o espectro INTEIRO,
    //     formantes inclusive. Serve para medir a distribuicao de beta no
    //     material real, que e' o que decide a viabilidade do v3.
    //     Ver docs/pesquisa-latencia-antares.md §7, questao 2.
    //
    //     Decimado por 'hop' porque a trilha e' constante dentro do hop (ela
    //     e' preenchida por quadro): gravar por amostra so' multiplicaria o
    //     arquivo por 256 sem acrescentar informacao.
    // ------------------------------------------------------------------
    if (!dumpBetaPath.empty()) {
        const std::vector<float>& f0s = eng.getF0Samp();
        const std::vector<float>& fos = eng.getFoutSamp();
        FILE* fp = std::fopen(dumpBetaPath.c_str(), "w");
        if (fp) {
            std::fprintf(fp, "# f0_hz fout_hz  (0 = nao-vozeado; decimado por hop=%d)\n", p.nHop);
            size_t n = std::min(f0s.size(), fos.size());
            for (size_t i = 0; i < n; i += (size_t)p.nHop)
                std::fprintf(fp, "%.4f %.4f\n", f0s[i], fos[i]);
            std::fclose(fp);
            std::printf("beta (f0,fout) gravado em: %s (%zu linhas)\n",
                        dumpBetaPath.c_str(), (n + p.nHop - 1) / p.nHop);
        } else std::printf("AVISO: nao consegui escrever dumpbeta='%s'\n", dumpBetaPath.c_str());
    }

    // ------------------------------------------------------------------
    // 4. A síntese é INCREMENTAL: o buffer 'out' já foi preenchido bloco a
    //    bloco por process() (PSOLA "online" via re-síntese em janela
    //    deslizante + overlap-save), atrasado pela latência algorítmica.
    //
    // 5. Relatório e gravação do WAV de saída (16-bit PCM).
    // ------------------------------------------------------------------
    const bool ponteiro = (p.motor == MotorSintese::Ponteiro);
    std::printf("stream_test: %.2fs | fs=%u | block=%d | look=%d | frame=%d | FMIN=%.0f | motor=%s | lat=%d amostras\n",
                (double)N/fs, taxa, block, p.look, p.nFrame, FMIN, ponteiro ? "ponteiro" : "psola",
                eng.getLatencySamples());
    if (ponteiro)
        std::printf("ponteiro: defasagem da correcao=%d amostras | saltos=%lld | dist media=%.1f am (%.2f ms) | dist max=%.1f am (%.2f ms)\n",
                    eng.getDefasagemCorrecao(), eng.getSaltosPonteiro(),
                    eng.getDistMediaPonteiro(), 1000.0 * eng.getDistMediaPonteiro() / fs,
                    eng.getDistMaxPonteiro(),   1000.0 * eng.getDistMaxPonteiro() / fs);
    std::printf("ultimo f0=%.1f Hz | ultimo fout=%.1f Hz\n", eng.getF0Atual(), eng.getFoutAtual());
    if (!gravarWav16(saida, out, taxa)) { std::printf("ERRO ao gravar\n"); return 1; }
    std::printf("Gravado: %s\n", saida);
    return 0;
}
