// ============================================================================
//  PluginProcessor.h — CAMINHO C2: o AudioProcessor do plugin (a "casca" JUCE)
//
//  Responsabilidade: adaptar o host de áudio (Ableton, ou o app Standalone) ao
//  núcleo de streaming `AutotuneStream` (C1). Toda a matemática do autotune mora
//  no núcleo; aqui só fazemos: (1) expor parâmetros ao host (APVTS), (2) ligar o
//  callback de áudio do host no `core.process()`, e (3) reportar a latência
//  algorítmica ao host via setLatencySamples (o número que aparece no Ableton).
//
//  Mono por dentro: a voz é mono. Somamos os canais de entrada num buffer mono,
//  processamos, e espalhamos a saída mono de volta em todos os canais.
// ============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <atomic>

#include "c1_streaming/autotune_stream.h"   // núcleo C1 (header-only); puxa core/dsp.h junto

// Listas dos combos "Voz"/"Escala" (definidas em PluginProcessor.cpp). A ORDEM
// define o índice salvo no parâmetro (ver nomeVoz()/textoEscala()). Usadas
// também pela GUI custom (PluginEditor.cpp) para popular os juce::ComboBox.
extern const juce::StringArray kVozes;
extern const juce::StringArray kTonicas;
extern const juce::StringArray kEscalas;

class TccAutotuneProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    TccAutotuneProcessor();
    ~TccAutotuneProcessor() override;

    // --- ciclo de vida do áudio ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    // --- GUI: usamos a GUI GENÉRICA do JUCE (sliders/combos automáticos a partir
    //     da APVTS). Suficiente p/ o TCC; uma GUI custom é refinamento posterior.
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // --- metadados exigidos pelo formato ---
    const juce::String getName() const override { return "TCC Autotune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // --- estado (salvar/restaurar os parâmetros no projeto do host) ---
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Árvore de parâmetros exposta ao host (mix/tol/retune/vibrato/look/voz/tonica/escala).
    juce::AudioProcessorValueTreeState apvts;

    // Snapshot "ao vivo" do pitch atual (Hz; 0 = sem voz), escrito no fim de
    // processBlock() e lido pela GUI (TunerDisplay) sem lock.
    float getUiF0()   const { return uiF0.load();   }
    float getUiFout() const { return uiFout.load(); }

private:
    // Monta os parâmetros do plugin (chamado no construtor da APVTS).
    static juce::AudioProcessorValueTreeState::ParameterLayout criarParametros();

    // (Re)configura o núcleo a partir dos valores ATUAIS dos parâmetros e
    // reporta a latência ao host. Aloca (prepare) → chamar fora do hot-path
    // de tempo real, ou no boot de um bloco quando um parâmetro ESTRUTURAL mudou.
    void aplicarParametros();

    // Listener da APVTS: marca 'precisaReprepare' quando um parâmetro ESTRUTURAL
    // (look/voz/escala) muda — esses exigem re-prepare (realocação + nova latência).
    void parameterChanged(const juce::String& id, float novoValor) override;

    // Mapeiam o índice do combo para o texto que dsp.h entende.
    static const char* nomeVoz(int idx);
    // Etapa 1: monta a string de definirEscala() a partir dos combos
    // Tonica (0=C..11=B) e Escala (0=cromatica, 1=maior, 2=menor natural).
    static std::string textoEscala(int tonica, int escala);

    AutotuneStream core;                 // o motor C1 (estado entre blocos)
    double          sampleRateAtual = 44100.0;

    // Buffers mono pré-alocados (no prepareToPlay) p/ não alocar no processBlock.
    std::vector<float> monoIn, monoOut;

    // Ponteiros "crus" (lock-free) para ler os parâmetros no áudio sem custo.
    std::atomic<float>* pMix  = nullptr;
    std::atomic<float>* pTol    = nullptr;
    std::atomic<float>* pRetune  = nullptr;
    std::atomic<float>* pVibrato = nullptr;
    std::atomic<float>* pLook   = nullptr;
    std::atomic<float>* pVoz    = nullptr;
    std::atomic<float>* pEscala = nullptr;
    std::atomic<float>* pTonica = nullptr;

    // Valores atuais do pitch (Hz; 0 = sem voz), escritos em processBlock()
    // e expostos via getUiF0()/getUiFout() acima.
    std::atomic<float> uiF0  { 0.0f };
    std::atomic<float> uiFout{ 0.0f };

    // Sinaliza (do thread de mensagens p/ o de áudio) que um parâmetro estrutural
    // mudou e o núcleo precisa ser re-preparado no próximo bloco.
    std::atomic<bool> precisaReprepare { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TccAutotuneProcessor)
};
