// ============================================================================
//  PluginEditor.h — tela custom do plugin (CAMINHO C2).
//
//  Duas metades. Em cima, o PainelAfinador: um cabecalho com a nota-alvo, um
//  arco com o desvio em cents, e uma faixa com o HISTORICO da correcao nos
//  ultimos 2,5 s. Embaixo, os 9 controles em tres grupos (Escala | Correcao |
//  Motor). Tema "verde escuro" via TccLookAndFeel.
//
//  Sem DSP nova: le proc.getUiF0()/getUiFout() e usa notaMaisProximaMidi/
//  hzParaNota (src/core/dsp.h) para nomear a nota-alvo.
//
//  ⚠️ O Create Vibrato NAO aparece aqui de proposito. Os parametros seguem no
//  APVTS (automatizaveis pelo host) e nos CLIs; so os widgets sairam. Ver a
//  Decisao 8 em docs/historico-e-decisoes.md -- nao "conserte" isto.
// ============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PluginProcessor.h"
#include "core/dsp.h"

// Paleta do tema "verde escuro". Duas cores carregam significado e nao podem
// ser trocadas por gosto: 'destaque' e' SEMPRE o sinal corrigido (a saida) e
// 'cantado' e' SEMPRE a altura crua do cantor. A leitura da tela inteira -- as
// duas agulhas do arco, as duas legendas de Hz -- depende desse par ser
// distinguivel, e por isso 'cantado' e' de outra familia de matiz (azul-aco
// frio), nao um verde mais escuro.
namespace tccColours {
    const juce::Colour fundoJanela    { 0xff070c0a };
    const juce::Colour fundoPainel    { 0xff0d1512 };
    const juce::Colour fundoFosso     { 0xff050807 };  // interior de graficos e trilhos
    const juce::Colour borda          { 0xff1b2f27 };
    const juce::Colour grade          { 0xff13231e };
    const juce::Colour textoPrincipal { 0xffcfe6da };
    const juce::Colour textoSecundario{ 0xff6b8a7c };
    const juce::Colour textoFraco     { 0xff3a5349 };
    const juce::Colour destaque       { 0xff2ee6a0 };  // SAIDA (sinal corrigido)
    const juce::Colour cantado        { 0xff7d94a8 };  // ENTRADA (altura crua)
}

// LookAndFeel custom: aplica a paleta verde aos sliders/combos/labels.
class TccLookAndFeel : public juce::LookAndFeel_V4 {
public:
    TccLookAndFeel();
};

// Painel do afinador: cabecalho (nota-alvo + Hz), arco de desvio e historico da
// correcao. Le proc.getUiF0()/getUiFout() 60x/segundo via Timer.
//
//  POR QUE O HISTORICO E' ACUMULADO AQUI, e nao no processor: o que a faixa
//  plota e' uma ENVOLTORIA lenta (quantos cents de correcao ao longo do tempo),
//  nao forma de onda. O vibrato vive em ~5,5 Hz, entao 60 Hz de amostragem
//  sobra. Acumular no timer da UI evita um ring buffer lock-free no callback de
//  audio -- que seria codigo novo em tempo real para desenhar um grafico.
class PainelAfinador : public juce::Component, private juce::Timer {
public:
    explicit PainelAfinador(TccAutotuneProcessor& p);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    // As tres pecas de desenho, cada uma dona do seu retangulo.
    void desenharCabecalho(juce::Graphics&, juce::Rectangle<float>);
    void desenharArco     (juce::Graphics&, juce::Rectangle<float>);
    void desenharHistorico(juce::Graphics&, juce::Rectangle<float>);

    TccAutotuneProcessor& proc;

    // Estado do instante, atualizado pelo Timer.
    float f0Atual    = 0.0f;   // Hz cantados (0 = sem voz)
    float foutAtual  = 0.0f;   // Hz de saida (0 = sem voz)
    int   alvoMidi   = 0;
    float freqAlvo   = 0.0f;
    float centsCorr  = 0.0f;   // saida - cantado, em cents: o numero grande
    bool  temVoz     = false;

    // Escala fixa dos dois medidores. Meio semitom para cada lado: passou
    // disso, o alvo teria sido outra nota.
    static constexpr float CENTS_ESCALA = 50.0f;

    // Historico circular da correcao. 60 Hz x 2,5 s.
    static constexpr int HZ_TIMER = 60;
    static constexpr int N_HIST   = 150;
    std::array<float, N_HIST> histCents { };
    std::array<bool,  N_HIST> histVoz   { };   // false = silencio (buraco no traco)
    int escritaHist = 0;                       // proxima posicao a escrever

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PainelAfinador)
};

// Editor custom: o PainelAfinador em cima, os 9 controles em tres grupos
// embaixo. As caixas dos grupos sao decoracao, desenhada em paint() a partir
// dos retangulos que resized() calcula.
class TccAutotuneEditor : public juce::AudioProcessorEditor {
public:
    explicit TccAutotuneEditor(TccAutotuneProcessor&);
    ~TccAutotuneEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Desenha uma caixa de grupo com o titulo "mordendo" a borda de cima.
    void desenharGrupo(juce::Graphics&, juce::Rectangle<int>, const juce::String& titulo);

    TccAutotuneProcessor& processorRef;
    TccLookAndFeel lookAndFeel;
    PainelAfinador painel;

    // Retangulos dos tres grupos, preenchidos por resized() e lidos por paint().
    juce::Rectangle<int> caixaEscala, caixaCorrecao, caixaMotor, faixaTitulo;

    juce::ComboBox vozCombo, tonicaCombo, escalaCombo;
    juce::Slider   mixSlider, tolSlider, retuneSlider, vibratoSlider, lookSlider, humanizeSlider;
    juce::Label    vozLabel, tonicaLabel, escalaLabel, mixLabel, tolLabel, retuneLabel,
                   vibratoLabel, lookLabel, humanizeLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> mixAttach, tolAttach, retuneAttach, vibratoAttach,
                                      lookAttach, humanizeAttach;
    std::unique_ptr<ComboAttachment>  vozAttach, tonicaAttach, escalaAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TccAutotuneEditor)
};
