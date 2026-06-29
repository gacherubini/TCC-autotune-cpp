// ============================================================================
//  PluginProcessor.cpp — implementação da casca JUCE (CAMINHO C2).
//
//  Liga o host de áudio ao núcleo de streaming `AutotuneStream` (C1):
//    - parâmetros do plugin  <->  StreamParams / updateLiveParams;
//    - callback de áudio do host (processBlock)  ->  core.process();
//    - latência algorítmica do núcleo  ->  setLatencySamples (barra do Ableton).
// ============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

// ----------------------------------------------------------------------------
//  IDs estáveis dos parâmetros (não mudar: o host salva por esse nome).
//  Estruturais (mudam dimensões/latência -> re-prepare): look, voz, escala.
//  "Ao vivo" (sem realocar): forca, tol, glide.
// ----------------------------------------------------------------------------
namespace ids {
    static constexpr const char* forca  = "forca";
    static constexpr const char* tol    = "tol";
    static constexpr const char* glide  = "glide";
    static constexpr const char* look   = "look";
    static constexpr const char* voz    = "voz";
    static constexpr const char* escala = "escala";
}

// Listas dos combos. A ORDEM define o índice salvo — manter em sincronia com
// nomeVoz()/textoEscala() abaixo.
const juce::StringArray kVozes  {
    "Baixo", "Baritono", "Tenor", "Contralto", "Mezzo", "Soprano", "Instrumento" };
const juce::StringArray kEscalas {
    "Cromatica", "Do maior (C)", "La menor (Am)", "Sol maior (G)",
    "Mi menor (Em)", "Fa maior (F)", "Re menor (Dm)" };

const char* TccAutotuneProcessor::nomeVoz(int idx) {
    switch (idx) {
        case 0: return "baixo";    case 1: return "baritono"; case 2: return "tenor";
        case 3: return "contralto";case 4: return "mezzo";    case 5: return "soprano";
        default: return "instrumento";
    }
}
const char* TccAutotuneProcessor::textoEscala(int idx) {
    switch (idx) {
        case 0: return "crom"; case 1: return "C";  case 2: return "Am"; case 3: return "G";
        case 4: return "Em";   case 5: return "F";  case 6: return "Dm"; default: return "crom";
    }
}

// ----------------------------------------------------------------------------
//  Layout de parâmetros expostos ao host. Os ranges espelham as flags do CLI.
// ----------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout
TccAutotuneProcessor::criarParametros() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // forca 0..1 — o quanto puxa em direção à nota da escala (1 = "duro").
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::forca, 1 }, "Forca", NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    // tol 0..50 cents — zona morta (preserva vibrato/microafinacao).
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::tol, 1 }, "Tolerancia (cents)", NormalisableRange<float>(0.0f, 50.0f), 15.0f));
    // glide 0..200 ms — portamento ate a nota-alvo (tira o "robo").
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::glide, 1 }, "Glide (ms)", NormalisableRange<float>(0.0f, 200.0f), 40.0f));
    // look 0..16 quadros — look-ahead do Viterbi (latencia x qualidade). ESTRUTURAL.
    layout.add(std::make_unique<AudioParameterInt>(
        ParameterID{ ids::look, 1 }, "Look-ahead (quadros)", 0, 16, 4));
    // voz — preset de tessitura (define fmin/fmax). ESTRUTURAL. Default Contralto.
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ ids::voz, 1 }, "Voz (tessitura)", kVozes, 3));
    // escala — cromatica ou tonica maior/menor. ESTRUTURAL (re-prepare por simplicidade).
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ ids::escala, 1 }, "Escala", kEscalas, 0));

    return layout;
}

// ----------------------------------------------------------------------------
//  Construtor: barramentos estéreo in/out, cria a APVTS, cacheia ponteiros dos
//  parâmetros e registra o listener nos parâmetros ESTRUTURAIS.
// ----------------------------------------------------------------------------
TccAutotuneProcessor::TccAutotuneProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", criarParametros())
{
    pForca  = apvts.getRawParameterValue(ids::forca);
    pTol    = apvts.getRawParameterValue(ids::tol);
    pGlide  = apvts.getRawParameterValue(ids::glide);
    pLook   = apvts.getRawParameterValue(ids::look);
    pVoz    = apvts.getRawParameterValue(ids::voz);
    pEscala = apvts.getRawParameterValue(ids::escala);

    // Só os estruturais disparam re-prepare (forca/tol/glide são "ao vivo").
    apvts.addParameterListener(ids::look,   this);
    apvts.addParameterListener(ids::voz,    this);
    apvts.addParameterListener(ids::escala, this);
}

TccAutotuneProcessor::~TccAutotuneProcessor() {
    apvts.removeParameterListener(ids::look,   this);
    apvts.removeParameterListener(ids::voz,    this);
    apvts.removeParameterListener(ids::escala, this);
}

void TccAutotuneProcessor::parameterChanged(const juce::String&, float) {
    // Chamado no thread de mensagens. Não realocamos aqui (poderia colidir com
    // o áudio); só marcamos a flag e o próximo processBlock re-prepara.
    precisaReprepare.store(true);
}

// ----------------------------------------------------------------------------
//  (Re)configura o núcleo a partir dos parâmetros atuais + reporta latência.
//  Aloca (core.prepare) — chamar no prepareToPlay ou no boot de um bloco quando
//  um parâmetro estrutural mudou (não a cada bloco).
// ----------------------------------------------------------------------------
void TccAutotuneProcessor::aplicarParametros() {
    StreamParams p;
    p.forca    = pForca  ? pForca->load()  : 1.0;
    p.tolCents = pTol    ? pTol->load()    : 0.0;
    p.glideMs  = pGlide  ? pGlide->load()  : 0.0;
    p.look     = pLook   ? (int) pLook->load() : 4;

    // Preset de tessitura -> fmin/fmax (a grade de pitch do núcleo).
    double fmin = FMIN, fmax = FMAX;
    presetVoz(nomeVoz(pVoz ? (int) pVoz->load() : 3), fmin, fmax);
    p.fmin = fmin; p.fmax = fmax;

    // Escala é global (g_permitida via definirEscala), lida por notaAlvo.
    definirEscala(textoEscala(pEscala ? (int) pEscala->load() : 0));

    core.prepare(sampleRateAtual, p);
    setLatencySamples(core.getLatencySamples());   // aparece na barra do Ableton
}

// ----------------------------------------------------------------------------
//  prepareToPlay: o host informa taxa e bloco máximo. Pré-alocamos os buffers
//  mono e configuramos o núcleo (única vez por sessão, fora do tempo real).
// ----------------------------------------------------------------------------
void TccAutotuneProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sampleRateAtual = sampleRate;
    monoIn.assign((size_t) juce::jmax(1, samplesPerBlock), 0.0f);
    monoOut.assign((size_t) juce::jmax(1, samplesPerBlock), 0.0f);
    precisaReprepare.store(false);
    aplicarParametros();
}

// ----------------------------------------------------------------------------
//  isBusesLayoutSupported: aceitamos mono->mono ou estéreo->estéreo (entrada e
//  saída com o mesmo conjunto de canais). Internamente é sempre mono.
// ----------------------------------------------------------------------------
bool TccAutotuneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

// ----------------------------------------------------------------------------
//  processBlock: o coração em tempo real. (1) re-prepara se um estrutural mudou;
//  (2) empurra os parâmetros "ao vivo"; (3) dobra os canais em mono, processa
//  pelo núcleo, e espalha a saída mono de volta nos canais.
// ----------------------------------------------------------------------------
void TccAutotuneProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();

    // (1) Parâmetro estrutural mudou? Re-prepara (realoca + nova latência).
    //     Acontece raramente (usuário girou look/voz/escala). NOTA: prepare()
    //     aloca — num plugin de produção isso iria p/ o thread de mensagens com
    //     suspendProcessing; aqui (protótipo de TCC) fazemos no boot do bloco.
    if (precisaReprepare.exchange(false)) {
        aplicarParametros();
        updateHostDisplay();   // pede ao host p/ reler a latência
    }

    // (2) Parâmetros "ao vivo" (sem realocar): forca/tol/glide.
    core.updateLiveParams(pForca->load(), pTol->load(), pGlide->load());

    // Segurança: se o host mandar um bloco maior que o previsto, cresce os
    // scratch buffers (não deveria ocorrer; JUCE garante n <= samplesPerBlock).
    if ((int) monoIn.size() < n)  { monoIn.resize((size_t) n);  monoOut.resize((size_t) n); }

    // (3a) entrada -> mono (média dos canais).
    for (int i = 0; i < n; ++i) {
        float s = 0.0f;
        for (int c = 0; c < numCh; ++c) s += buffer.getReadPointer(c)[i];
        monoIn[(size_t) i] = (numCh > 0) ? s / (float) numCh : 0.0f;
    }

    // (3b) processa um bloco pelo núcleo de streaming (saída atrasada da latência).
    core.process(monoIn.data(), monoOut.data(), n);
    uiF0.store(core.getF0Atual());
    uiFout.store(core.getFoutAtual());

    // (3c) saída mono -> todos os canais.
    for (int c = 0; c < numCh; ++c) {
        float* w = buffer.getWritePointer(c);
        for (int i = 0; i < n; ++i) w[i] = monoOut[(size_t) i];
    }
}

// ----------------------------------------------------------------------------
//  GUI genérica: o JUCE gera sliders/combos a partir da APVTS. (Custom depois.)
// ----------------------------------------------------------------------------
juce::AudioProcessorEditor* TccAutotuneProcessor::createEditor() {
    return new TccAutotuneEditor(*this);
}

// ----------------------------------------------------------------------------
//  Estado: serializa/desserializa a APVTS (host salva os parâmetros no projeto).
// ----------------------------------------------------------------------------
void TccAutotuneProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}
void TccAutotuneProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    precisaReprepare.store(true);   // reaplica nos próximos blocos
}

// ----------------------------------------------------------------------------
//  Fábrica exigida pelo wrapper de plugin do JUCE.
// ----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new TccAutotuneProcessor();
}
