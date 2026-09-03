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
//  "Ao vivo" (sem realocar): mix, tol, retune, vibrato.
// ----------------------------------------------------------------------------
namespace ids {
    // Etapa 2 do plano: o id "forca" foi APOSENTADO e o parametro que ocupa o
    // lugar dele chama-se "mix". Id novo de proposito: o significado mudou (era
    // fracao do desvio corrigido, agora e' cruzamento seco/molhado), entao
    // reaproveitar o id faria o host restaurar um valor antigo com semantica
    // nova, calado. Com id novo, o projeto salvo simplesmente cai no padrao.
    static constexpr const char* mix    = "mix";
    static constexpr const char* tol    = "tol";
    // Etapa 3: o id "glide" foi APOSENTADO. O parametro nao mudou de unidade
    // (continua ms), mas mudou de SIGNIFICADO: o filtro deixou de agir sobre o
    // alvo e passou a agir sobre a correcao, e o padrao foi de 40 para 25 ms
    // (faixa recomendada pelo manual da Antares). Um valor salvo de 40 ms nao
    // soa mais igual, entao id novo -- mesma regra aplicada ao "mix".
    static constexpr const char* retune  = "retune";
    static constexpr const char* vibrato  = "vibrato";
    // Etapa 4/5
    static constexpr const char* humanize = "humanize";
    static constexpr const char* vibForma = "vibforma";
    static constexpr const char* vibTaxa  = "vibtaxa";
    static constexpr const char* vibProf  = "vibprof";
    static constexpr const char* vibAmp   = "vibamp";
    static constexpr const char* look   = "look";
    // Etapa 6: o botao Low Latency. Liga o motor de ponteiro E forca look = 0.
    // Estrutural: muda a latencia declarada ao host.
    static constexpr const char* lowlat = "lowlat";
    static constexpr const char* voz    = "voz";
    static constexpr const char* escala = "escala";
    // Etapa 1 do plano: a tonica virou parametro proprio. O id "escala" foi
    // MANTIDO (agora guarda so o modo) para nao inventar id novo a toa; quem
    // abrir um projeto salvo com a versao antiga cai no indice equivalente
    // quando ele existe, e em cromatico quando nao existe.
    static constexpr const char* tonica = "tonica";
}

// Listas dos combos. A ORDEM define o índice salvo — manter em sincronia com
// nomeVoz()/textoEscala() abaixo.
//
// Etapa "encaixe e estabilidade" (D4): a lista de tessituras ENCOLHEU de 7 para
// 4 e passou a ser a do Auto-Tune. Ela não é mais montada aqui: a tabela mora em
// dsp.h (vozDaInterface), junto da razão da ordem e do rótulo com a faixa em
// notas, para que a GUI e os testes leiam a MESMA fonte. Ver o comentário longo
// lá — em especial por que o índice 3 é `Instrument`.
//
// Os nove presets de presetVoz() continuam alcançáveis por `voz=` na linha de
// comando. Só a interface encolheu.
const juce::StringArray kVozes = [] {
    juce::StringArray a;
    for (int i = 0; i < N_VOZES_UI; ++i) a.add(vozDaInterface(i).rotulo);
    return a;
}();
// Etapa 1: 12 tonicas x 3 modos = 24 tonalidades + cromatico, no lugar dos
// 6 combos fixos que existiam antes (ver docs/execucao-do-plano.md).
const juce::StringArray kTonicas {
    "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };
const juce::StringArray kEscalas {
    "Cromatica", "Maior", "Menor natural" };
// Etapa 5: formas de onda do Create Vibrato. A ordem tem de bater com o enum
// FormaVibrato de dsp.h -- o indice do combo e' convertido direto.
const juce::StringArray kFormasVib { "Off", "Senoide", "Triangular", "Quadrada" };

// Era um switch sobre o índice, com `default: instrumento`. Isso é exatamente o
// que não sobrevive a uma lista que muda de tamanho: encolher o combo sem
// encolher o switch faria o menu mostrar uma tessitura e o DSP usar outra, sem
// erro de compilação. Agora as duas coisas saem da mesma tabela em dsp.h, e
// src/tests/test_vozes.cpp prende uma à outra.
const char* TccAutotuneProcessor::nomeVoz(int idx) {
    return vozDaInterface(idx).preset;
}
// A tabela mora em dsp.h (montarEscala), para que GUI e testes usem a mesma.
std::string TccAutotuneProcessor::textoEscala(int tonica, int escala) {
    return montarEscala(tonica, escala);
}

// ----------------------------------------------------------------------------
//  Layout de parâmetros expostos ao host. Os ranges espelham as flags do CLI.
// ----------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout
TccAutotuneProcessor::criarParametros() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // mix 0..1 — cruzamento seco/molhado. 0 = so o sinal original (bypass
    // exato, mas ainda atrasado da latencia do motor), 1 = so o corrigido.
    // Padrao 1.0 para que a instalacao nova soe como a versao anterior soava.
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::mix, 1 }, "Mix", NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    // tol 0..50 cents — zona morta (preserva vibrato/microafinacao).
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::tol, 1 }, "Tolerancia (cents)", NormalisableRange<float>(0.0f, 50.0f), 15.0f));
    // retune 0..200 ms — Retune Speed: quanto tempo a correcao leva para chegar
    // a nota. Padrao 25 ms; o manual da Antares diz que "a setting between 10
    // and 50 is typical for more natural sounding pitch correction". O ZERO
    // precisa continuar alcancavel: e' ele que da o efeito "duro" deliberado.
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::retune, 1 }, "Retune Speed (ms)", NormalisableRange<float>(0.0f, 200.0f), 25.0f));
    // vibrato 0..2 — quanto do vibrato do cantor sobrevive a correcao.
    //   0 = removido (o comportamento ate a Etapa 2), 1 = preservado, 2 = dobrado.
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::vibrato, 1 }, "Natural Vibrato", NormalisableRange<float>(0.0f, 2.0f), 1.0f));
    // humanize 0..1 (Etapa 4) — afrouxa o Retune Speed na sustentacao da nota.
    // Padrao 0 para que a instalacao nova soe exatamente como a Etapa 3.
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::humanize, 1 }, "Humanize", NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // Create Vibrato (Etapa 5) — GERA vibrato, ao contrario do Natural Vibrato,
    // que apenas preserva o do cantor. Desligado por padrao.
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ ids::vibForma, 1 }, "Create Vibrato",
        kFormasVib, 0));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::vibTaxa, 1 }, "Vibrato Rate (Hz)", NormalisableRange<float>(0.1f, 10.0f), 5.5f));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::vibProf, 1 }, "Vibrato Depth (ct)", NormalisableRange<float>(0.0f, 100.0f), 0.0f));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ ids::vibAmp, 1 }, "Amplitude Amount", NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // look 0..16 quadros — look-ahead do Viterbi (latencia x qualidade). ESTRUTURAL.
    layout.add(std::make_unique<AudioParameterInt>(
        ParameterID{ ids::look, 1 }, "Look-ahead (quadros)", 0, 16, 4));
    // Low Latency (Etapa 6): troca o TD-PSOLA pelo motor de ponteiro movel (v3)
    // e forca look = 0. Desligado por padrao: a instalacao nova soa como antes e
    // a linha de base mede o motor padrao. Ver docs/especificacao-v3-ponteiro.md.
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID{ ids::lowlat, 1 }, "Low Latency", false));
    // voz — preset de tessitura (define fmin/fmax). ESTRUTURAL.
    // Padrão de fábrica Alto-Tenor (índice 1): é o único dos quatro que cobre o
    // take medido inteiro. Ver VOZ_UI_PADRAO em dsp.h para a medição e o preço.
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ ids::voz, 1 }, "Voz (tessitura)", kVozes, VOZ_UI_PADRAO));
    // escala — cromatica ou tonica maior/menor. ESTRUTURAL (re-prepare por simplicidade).
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID{ ids::tonica, 1 }, "Tonica", kTonicas, 0));

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
    pMix    = apvts.getRawParameterValue(ids::mix);
    pTol    = apvts.getRawParameterValue(ids::tol);
    pRetune  = apvts.getRawParameterValue(ids::retune);
    pVibrato = apvts.getRawParameterValue(ids::vibrato);
    pHumanize = apvts.getRawParameterValue(ids::humanize);
    pVibForma = apvts.getRawParameterValue(ids::vibForma);
    pVibTaxa  = apvts.getRawParameterValue(ids::vibTaxa);
    pVibProf  = apvts.getRawParameterValue(ids::vibProf);
    pVibAmp   = apvts.getRawParameterValue(ids::vibAmp);
    pLook   = apvts.getRawParameterValue(ids::look);
    pLowLat = apvts.getRawParameterValue(ids::lowlat);
    pVoz    = apvts.getRawParameterValue(ids::voz);
    pEscala = apvts.getRawParameterValue(ids::escala);
    pTonica = apvts.getRawParameterValue(ids::tonica);

    // Só os estruturais disparam re-prepare (mix/tol/retune/vibrato são "ao vivo").
    apvts.addParameterListener(ids::look,   this);
    apvts.addParameterListener(ids::lowlat, this);
    apvts.addParameterListener(ids::voz,    this);
    apvts.addParameterListener(ids::escala, this);
    apvts.addParameterListener(ids::tonica, this);
}

TccAutotuneProcessor::~TccAutotuneProcessor() {
    apvts.removeParameterListener(ids::look,   this);
    apvts.removeParameterListener(ids::lowlat, this);
    apvts.removeParameterListener(ids::voz,    this);
    apvts.removeParameterListener(ids::escala, this);
    apvts.removeParameterListener(ids::tonica, this);
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
// ----------------------------------------------------------------------------
//  Monta a malha de correcao a partir do APVTS. Existe como funcao porque e'
//  chamada de DOIS lugares -- aplicarParametros() (no re-prepare) e a cada
//  bloco em processBlock() -- e uma divergencia entre as duas copias produziria
//  um plugin que soa diferente logo depois de mexer num controle estrutural.
// ----------------------------------------------------------------------------
ParamsCorrecao TccAutotuneProcessor::lerCorrecao() const {
    ParamsCorrecao c;
    c.tolCents = pTol      ? pTol->load()      : 0.0;
    c.retuneMs = pRetune   ? pRetune->load()   : 0.0;
    c.vibrato  = pVibrato  ? pVibrato->load()  : 1.0;
    c.humanize = pHumanize ? pHumanize->load() : 0.0;
    c.vibForma = (FormaVibrato)(pVibForma ? (int)pVibForma->load() : 0);
    c.vibTaxa  = pVibTaxa  ? pVibTaxa->load()  : 5.5;
    c.vibProf  = pVibProf  ? pVibProf->load()  : 0.0;
    c.vibAmp   = pVibAmp   ? pVibAmp->load()   : 0.0;
    sanearCorrecao(c);
    return c;
}

void TccAutotuneProcessor::aplicarParametros() {
    StreamParams p;
    p.mix      = pMix    ? pMix->load()    : 1.0;
    p.corr = lerCorrecao();
    // Low Latency (Etapa 6): liga o motor de ponteiro e forca look = 0. O
    // valor salvo em 'pLook' NAO e' alterado -- so' o que chega ao nucleo.
    // Desligar o botao devolve o look-ahead configurado sem o usuario mexer.
    const bool lowlat = pLowLat && pLowLat->load() > 0.5f;
    p.look  = lowlat ? 0 : (pLook ? (int) pLook->load() : 4);
    p.motor = lowlat ? MotorSintese::Ponteiro : MotorSintese::PSOLA;

    // Preset de tessitura -> fmin/fmax (a grade de pitch do núcleo).
    double fmin = FMIN, fmax = FMAX;
    presetVoz(nomeVoz(pVoz ? (int) pVoz->load() : VOZ_UI_PADRAO), fmin, fmax);
    p.fmin = fmin; p.fmax = fmax;

    // Escala é global (g_permitida via definirEscala), lida por notaAlvo.
    definirEscala(textoEscala(pTonica ? (int) pTonica->load() : 0,
                              pEscala ? (int) pEscala->load() : 0).c_str());

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

    // (2) Parâmetros "ao vivo" (sem realocar): toda a malha de correcao + mix.
    core.updateLiveParams(lerCorrecao(), pMix->load());

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
