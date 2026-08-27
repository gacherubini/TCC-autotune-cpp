// ============================================================================
//  PluginEditor.h — tela custom do plugin (CAMINHO C2).
//
//  Substitui a GenericAudioProcessorEditor por: um "afinador" (TunerDisplay)
//  no topo, mostrando a nota-alvo e o desvio em cents antes/depois da
//  correção, e uma faixa com os 6 controles existentes embaixo (Task 5).
//  Tema "âmbar escuro" via TccLookAndFeel. Sem DSP nova: só lê
//  proc.getUiF0()/getUiFout() (Task 3) e usa notaMaisProximaMidi/hzParaNota
//  (src/core/dsp.h) para nomear a nota-alvo.
// ============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "core/dsp.h"

// Paleta do tema "âmbar escuro" (ver docs/superpowers/specs/2026-06-12-gui-plugin-design.md).
namespace tccColours {
    const juce::Colour fundoJanela    { 0xff15110d };
    const juce::Colour fundoPainel    { 0xff1f1a14 };
    const juce::Colour borda          { 0xff3a2f24 };
    const juce::Colour textoPrincipal { 0xffd8cfc0 };
    const juce::Colour textoSecundario{ 0xff9a8c78 };
    const juce::Colour destaque       { 0xffe0a458 };
    const juce::Colour agulhaAntes    { 0xff6b5a48 };
}

// LookAndFeel custom: aplica a paleta âmbar aos sliders/combos/labels da
// faixa de controles (Task 5).
class TccLookAndFeel : public juce::LookAndFeel_V4 {
public:
    TccLookAndFeel();
};

// Área do afinador: nome da nota-alvo, texto "detectado X Hz -> alvo Y Hz" e
// medidor de desvio em cents com duas marcas (cinza = antes da correção,
// âmbar = depois). Lê proc.getUiF0()/getUiFout() ~30x/segundo via Timer.
class TunerDisplay : public juce::Component, private juce::Timer {
public:
    explicit TunerDisplay(TccAutotuneProcessor& p);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    TccAutotuneProcessor& proc;
    float f0Atual = 0.0f;
    float foutAtual = 0.0f;
};

// Editor custom: por enquanto só o TunerDisplay, ocupando a janela toda.
// A Task 5 adiciona a faixa de controles embaixo.
class TccAutotuneEditor : public juce::AudioProcessorEditor {
public:
    explicit TccAutotuneEditor(TccAutotuneProcessor&);
    ~TccAutotuneEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    TccAutotuneProcessor& processorRef;
    TccLookAndFeel lookAndFeel;
    TunerDisplay tuner;

    juce::ComboBox vozCombo, tonicaCombo, escalaCombo;
    juce::Slider   mixSlider, tolSlider, retuneSlider, vibratoSlider, lookSlider;
    juce::Label    vozLabel, tonicaLabel, escalaLabel, mixLabel, tolLabel, retuneLabel,
                   vibratoLabel, lookLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> mixAttach, tolAttach, retuneAttach, vibratoAttach, lookAttach;
    std::unique_ptr<ComboAttachment>  vozAttach, tonicaAttach, escalaAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TccAutotuneEditor)
};
