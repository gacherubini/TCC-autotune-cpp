// ============================================================================
//  PluginEditor.cpp — implementação da tela custom (CAMINHO C2).
// ============================================================================
#include "PluginEditor.h"

// ----------------------------------------------------------------------------
//  TccLookAndFeel — paleta âmbar escura para sliders/combos/labels/popups.
// ----------------------------------------------------------------------------
TccLookAndFeel::TccLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, tccColours::fundoJanela);

    setColour(juce::Slider::backgroundColourId, tccColours::fundoPainel);
    setColour(juce::Slider::trackColourId,      tccColours::destaque);
    setColour(juce::Slider::thumbColourId,      tccColours::destaque);
    setColour(juce::Slider::textBoxTextColourId,       tccColours::textoPrincipal);
    setColour(juce::Slider::textBoxOutlineColourId,    tccColours::borda);
    setColour(juce::Slider::textBoxBackgroundColourId, tccColours::fundoPainel);

    setColour(juce::ComboBox::backgroundColourId, tccColours::fundoPainel);
    setColour(juce::ComboBox::outlineColourId,    tccColours::borda);
    setColour(juce::ComboBox::textColourId,       tccColours::textoPrincipal);
    setColour(juce::ComboBox::arrowColourId,      tccColours::destaque);

    setColour(juce::Label::textColourId, tccColours::textoSecundario);

    setColour(juce::PopupMenu::backgroundColourId,            tccColours::fundoPainel);
    setColour(juce::PopupMenu::textColourId,                  tccColours::textoPrincipal);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, tccColours::destaque);
}

// ----------------------------------------------------------------------------
//  TunerDisplay — afinador: nota-alvo + texto Hz + medidor antes/depois.
// ----------------------------------------------------------------------------
TunerDisplay::TunerDisplay(TccAutotuneProcessor& p) : proc(p) {
    startTimerHz(30);
}

void TunerDisplay::timerCallback() {
    f0Atual   = proc.getUiF0();
    foutAtual = proc.getUiFout();
    repaint();
}

void TunerDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    g.setColour(tccColours::fundoPainel);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(tccColours::borda);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const bool semVoz = (f0Atual <= 0.0f || foutAtual <= 0.0f);

    // -- nome da nota-alvo (área superior) --------------------------------
    auto areaNota = bounds.removeFromTop(bounds.getHeight() * 0.45f);
    juce::String nomeNota = "--";
    double freqAlvo = 0.0;
    int alvoMidi = 0;
    if (!semVoz) {
        alvoMidi = notaMaisProximaMidi((double) f0Atual);
        freqAlvo = 440.0 * std::pow(2.0, (alvoMidi - 69) / 12.0);
        nomeNota = hzParaNota(freqAlvo);
    }
    g.setColour(tccColours::destaque);
    g.setFont(juce::Font(48.0f, juce::Font::bold));
    g.drawText(nomeNota, areaNota.toNearestInt(), juce::Justification::centred);

    // -- texto "detectado X Hz -> alvo Y Hz" -------------------------------
    auto areaTexto = bounds.removeFromTop(24.0f);
    if (!semVoz) {
        g.setColour(tccColours::textoSecundario);
        g.setFont(juce::Font(13.0f));
        juce::String texto = juce::String::formatted("detectado %.1f Hz -> alvo %.1f Hz",
                                                       f0Atual, freqAlvo);
        g.drawText(texto, areaTexto.toNearestInt(), juce::Justification::centred);
    }

    // -- medidor de desvio (cents), faixa fixa de -50 a +50 ----------------
    auto areaMedidor = bounds.reduced(20.0f, 10.0f);
    float meterY    = areaMedidor.getCentreY();
    float centerX   = areaMedidor.getCentreX();
    float pxPorCent = (areaMedidor.getWidth() * 0.5f) / 50.0f;

    g.setColour(tccColours::borda);
    g.fillRoundedRectangle(areaMedidor.getX(), meterY - 4.0f, areaMedidor.getWidth(), 8.0f, 4.0f);

    // marca central (0 cents)
    g.setColour(tccColours::agulhaAntes);
    g.fillRect(centerX - 1.0f, meterY - 7.0f, 2.0f, 14.0f);

    if (!semVoz) {
        double midiF0   = 69.0 + 12.0 * std::log2((double) f0Atual   / 440.0);
        double midiFout = 69.0 + 12.0 * std::log2((double) foutAtual / 440.0);
        double centsAntes  = juce::jlimit(-50.0, 50.0, (midiF0   - alvoMidi) * 100.0);
        double centsDepois = juce::jlimit(-50.0, 50.0, (midiFout - alvoMidi) * 100.0);

        // agulha cinza: voz original (antes da correção)
        g.setColour(tccColours::agulhaAntes);
        float xAntes = centerX + (float) centsAntes * pxPorCent;
        g.fillRect(xAntes - 1.5f, meterY - 9.0f, 3.0f, 18.0f);

        // agulha âmbar: sinal corrigido (depois)
        g.setColour(tccColours::destaque);
        float xDepois = centerX + (float) centsDepois * pxPorCent;
        g.fillRect(xDepois - 1.5f, meterY - 9.0f, 3.0f, 18.0f);
    }
}

// ----------------------------------------------------------------------------
//  TccAutotuneEditor
// ----------------------------------------------------------------------------
TccAutotuneEditor::TccAutotuneEditor(TccAutotuneProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), tuner(p)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(tuner);

    auto configurarSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& texto) {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(s);
        l.setText(texto, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(11.0f));
        addAndMakeVisible(l);
    };
    configurarSlider(mixSlider, mixLabel, "Mix");
    configurarSlider(tolSlider,   tolLabel,   "Tolerancia");
    configurarSlider(retuneSlider, retuneLabel, "Retune Speed");
    configurarSlider(vibratoSlider, vibratoLabel, "Natural Vibrato");
    configurarSlider(humanizeSlider, humanizeLabel, "Humanize");
    configurarSlider(vibTaxaSlider,  vibTaxaLabel,  "Vib Rate");
    configurarSlider(vibProfSlider,  vibProfLabel,  "Vib Depth");
    configurarSlider(vibAmpSlider,   vibAmpLabel,   "Vib Amp");
    configurarSlider(lookSlider,  lookLabel,  "Look-ahead");

    auto configurarCombo = [this](juce::ComboBox& c, juce::Label& l, const juce::String& texto,
                                   const juce::StringArray& itens) {
        c.addItemList(itens, 1);
        addAndMakeVisible(c);
        l.setText(texto, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(11.0f));
        addAndMakeVisible(l);
    };
    configurarCombo(vozCombo,    vozLabel,    "Voz",    kVozes);
    configurarCombo(tonicaCombo, tonicaLabel, "Tonica", kTonicas);
    configurarCombo(escalaCombo, escalaLabel, "Escala", kEscalas);
    configurarCombo(vibFormaCombo, vibFormaLabel, "Create Vib", kFormasVib);

    auto& apvts = processorRef.apvts;
    mixAttach = std::make_unique<SliderAttachment>(apvts, "mix",   mixSlider);
    tolAttach   = std::make_unique<SliderAttachment>(apvts, "tol",   tolSlider);
    retuneAttach  = std::make_unique<SliderAttachment>(apvts, "retune",  retuneSlider);
    vibratoAttach  = std::make_unique<SliderAttachment>(apvts, "vibrato",  vibratoSlider);
    humanizeAttach = std::make_unique<SliderAttachment>(apvts, "humanize", humanizeSlider);
    vibTaxaAttach  = std::make_unique<SliderAttachment>(apvts, "vibtaxa",  vibTaxaSlider);
    vibProfAttach  = std::make_unique<SliderAttachment>(apvts, "vibprof",  vibProfSlider);
    vibAmpAttach   = std::make_unique<SliderAttachment>(apvts, "vibamp",   vibAmpSlider);
    vibFormaAttach = std::make_unique<ComboAttachment>(apvts, "vibforma", vibFormaCombo);
    lookAttach  = std::make_unique<SliderAttachment>(apvts, "look",  lookSlider);
    vozAttach    = std::make_unique<ComboAttachment>(apvts, "voz",    vozCombo);
    tonicaAttach = std::make_unique<ComboAttachment>(apvts, "tonica", tonicaCombo);
    escalaAttach = std::make_unique<ComboAttachment>(apvts, "escala", escalaCombo);

    setSize(480, 320);
}

TccAutotuneEditor::~TccAutotuneEditor() {
    setLookAndFeel(nullptr);
}

void TccAutotuneEditor::paint(juce::Graphics& g) {
    g.fillAll(tccColours::fundoJanela);
}

void TccAutotuneEditor::resized() {
    auto area = getLocalBounds().reduced(10);

    auto tunerArea = area.removeFromTop((int) (area.getHeight() * 0.65f));
    tuner.setBounds(tunerArea);

    area.removeFromTop(10); // espaco entre o afinador e a faixa de controles

    // Etapa 1: 7 colunas — a Tonica entrou ao lado da Escala.
    // Etapa 3: 8 — o Natural Vibrato entrou ao lado do Retune Speed.
    // Etapas 4/5: 13 — Humanize e os quatro do Create Vibrato.
    //
    // Treze colunas numa faixa e' MUITO, e fica anotado como divida de
    // interface: a GUI generica em faixa unica nao escala mais. A organizacao
    // certa e' em grupos (Escala | Correcao | Expressao), e isso e' trabalho de
    // desenho, nao de DSP -- nao entra numa etapa cujo criterio de aceite e'
    // "nao mudou o audio". Ver docs/execucao-do-plano.md, Etapa 5.
    const int n = 13;
    const int largura = area.getWidth() / n;

    auto montarCombo = [&](juce::ComboBox& ctrl, juce::Label& label) {
        auto col = area.removeFromLeft(largura);
        label.setBounds(col.removeFromTop(16));
        ctrl.setBounds(col.removeFromTop(24));
    };
    auto montarSlider = [&](juce::Slider& ctrl, juce::Label& label) {
        auto col = area.removeFromLeft(largura);
        label.setBounds(col.removeFromTop(16));
        ctrl.setBounds(col);
    };

    montarCombo(vozCombo,    vozLabel);
    montarCombo(tonicaCombo, tonicaLabel);
    montarCombo(escalaCombo, escalaLabel);
    montarSlider(mixSlider, mixLabel);
    montarSlider(tolSlider,   tolLabel);
    montarSlider(retuneSlider, retuneLabel);
    montarSlider(vibratoSlider, vibratoLabel);
    montarSlider(humanizeSlider, humanizeLabel);
    montarCombo(vibFormaCombo, vibFormaLabel);
    montarSlider(vibTaxaSlider, vibTaxaLabel);
    montarSlider(vibProfSlider, vibProfLabel);
    montarSlider(vibAmpSlider,  vibAmpLabel);
    montarSlider(lookSlider,  lookLabel);
}
