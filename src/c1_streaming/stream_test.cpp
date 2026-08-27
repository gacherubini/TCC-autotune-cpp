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
//  Uso: stream_test.exe <in.wav> [out.wav] [mix] [escala] [tol=] [retune=] [vibrato=] [look=]
//                       [frame=] [hop=] [voz=] [fmin=] [fmax=] [block=B] [dumpf0=arq]
// ============================================================================
#define DR_WAV_IMPLEMENTATION
#include "../core/dsp.h"
#include "autotune_stream.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("Uso: %s <in.wav> [out.wav] [mix] [escala] [flags...] [block=B]\n", argv[0]); return 1; }

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
    int block = 128; std::string vozNome, dumpF0Path, dumpFramesPath; bool fminExpl=false, fmaxExpl=false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if      (a.rfind("tol=",0)==0)   p.tolCents = std::atof(a.c_str()+4);
        // "glide=" e' o nome antigo de "retune="; apelido mantido (Etapa 3).
        else if (a.rfind("glide=",0)==0)   p.retuneMs = std::atof(a.c_str()+6);
        else if (a.rfind("retune=",0)==0)  p.retuneMs = std::atof(a.c_str()+7);
        else if (a.rfind("vibrato=",0)==0) p.vibrato  = std::atof(a.c_str()+8);
        else if (a.rfind("legado=",0)==0)  p.ataqueNoAlvo = (std::atoi(a.c_str()+7)!=0);
        else if (a.rfind("look=",0)==0)  p.look     = std::atoi(a.c_str()+5);
        else if (a.rfind("frame=",0)==0) p.nFrame   = std::atoi(a.c_str()+6);
        else if (a.rfind("hop=",0)==0)   p.nHop     = std::atoi(a.c_str()+4);
        else if (a.rfind("voz=",0)==0)   vozNome    = a.c_str()+4;
        else if (a.rfind("fmin=",0)==0){ p.fmin=std::atof(a.c_str()+5); fminExpl=true; }
        else if (a.rfind("fmax=",0)==0){ p.fmax=std::atof(a.c_str()+5); fmaxExpl=true; }
        else if (a.rfind("block=",0)==0) block      = std::atoi(a.c_str()+6);
        else if (a.rfind("dumpf0=",0)==0)dumpF0Path = a.c_str()+7;
        else if (a.rfind("dumpframes=",0)==0) dumpFramesPath = a.c_str()+11;
    }
    // Preset de tessitura (igual ao autotune_rt): fmin=/fmax= explícitos vencem.
    if (!vozNome.empty()) { double pf,px; if (presetVoz(vozNome,pf,px)) { if(!fminExpl)p.fmin=pf; if(!fmaxExpl)p.fmax=px; } }
    // Sanitização básica dos parâmetros (mesmos limites do autotune_rt).
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
    // 4. A síntese é INCREMENTAL: o buffer 'out' já foi preenchido bloco a
    //    bloco por process() (PSOLA "online" via re-síntese em janela
    //    deslizante + overlap-save), atrasado pela latência algorítmica.
    //
    // 5. Relatório e gravação do WAV de saída (16-bit PCM).
    // ------------------------------------------------------------------
    std::printf("stream_test: %.2fs | fs=%u | block=%d | look=%d | frame=%d | FMIN=%.0f | lat=%d amostras\n",
                (double)N/fs, taxa, block, p.look, p.nFrame, FMIN, eng.getLatencySamples());
    std::printf("ultimo f0=%.1f Hz | ultimo fout=%.1f Hz\n", eng.getF0Atual(), eng.getFoutAtual());
    if (!gravarWav16(saida, out, taxa)) { std::printf("ERRO ao gravar\n"); return 1; }
    std::printf("Gravado: %s\n", saida);
    return 0;
}
