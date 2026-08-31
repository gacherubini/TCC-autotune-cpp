// ============================================================================
//  PluginEditor.cpp — implementação da tela custom (CAMINHO C2).
// ============================================================================
#include "PluginEditor.h"

namespace {
    // Ponto na circunferencia, na convencao de angulo do JUCE (0 = meio-dia,
    // positivo = sentido horario) -- a mesma de Path::addCentredArc.
    juce::Point<float> polar(juce::Point<float> centro, float raio, float ang) {
        return { centro.x + std::sin(ang) * raio, centro.y - std::cos(ang) * raio };
    }

    juce::Path arcoEntre(juce::Point<float> centro, float raio, float a0, float a1) {
        juce::Path p;
        p.addCentredArc(centro.x, centro.y, raio, raio, 0.0f,
                        juce::jmin(a0, a1), juce::jmax(a0, a1), true);
        return p;
    }

    // Diferenca em cents entre duas frequencias. So faz sentido com as duas > 0.
    float centsEntre(float de, float para) {
        return (float) (1200.0 * std::log2((double) para / (double) de));
    }
}

// ----------------------------------------------------------------------------
//  TccLookAndFeel — paleta verde escura para sliders/combos/labels/popups.
// ----------------------------------------------------------------------------
TccLookAndFeel::TccLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, tccColours::fundoJanela);

    setColour(juce::Slider::backgroundColourId, tccColours::fundoFosso);
    setColour(juce::Slider::trackColourId,      tccColours::destaque);
    setColour(juce::Slider::thumbColourId,      tccColours::destaque);
    setColour(juce::Slider::textBoxTextColourId,       tccColours::textoPrincipal);
    setColour(juce::Slider::textBoxOutlineColourId,    tccColours::borda);
    setColour(juce::Slider::textBoxBackgroundColourId, tccColours::fundoFosso);

    setColour(juce::ComboBox::backgroundColourId, tccColours::fundoFosso);
    setColour(juce::ComboBox::outlineColourId,    tccColours::borda);
    setColour(juce::ComboBox::textColourId,       tccColours::textoPrincipal);
    setColour(juce::ComboBox::arrowColourId,      tccColours::destaque);

    setColour(juce::Label::textColourId, tccColours::textoSecundario);

    setColour(juce::PopupMenu::backgroundColourId,            tccColours::fundoPainel);
    setColour(juce::PopupMenu::textColourId,                  tccColours::textoPrincipal);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, tccColours::destaque);
    setColour(juce::PopupMenu::highlightedTextColourId,       tccColours::fundoJanela);
}

// ----------------------------------------------------------------------------
//  PainelAfinador
// ----------------------------------------------------------------------------
PainelAfinador::PainelAfinador(TccAutotuneProcessor& p) : proc(p) {
    histCents.fill(0.0f);
    histVoz.fill(false);
    startTimerHz(HZ_TIMER);
}

void PainelAfinador::timerCallback() {
    f0Atual   = proc.getUiF0();
    foutAtual = proc.getUiFout();
    temVoz    = (f0Atual > 0.0f && foutAtual > 0.0f);

    if (temVoz) {
        // A nota-alvo sai da SAIDA, nao da entrada. Se o cantor ataca um F#
        // numa escala de C maior, o alvo real do motor e' F ou G -- e quem
        // sabe disso e' o fout, que converge para ele. Tirar o alvo do f0
        // (como a versao anterior fazia) exibiria F#, uma nota que a escala
        // nem permite. Durante o glide de ataque os dois coincidem, e a nota
        // exibida "resolve" junto com o audio, o que e' honesto.
        alvoMidi  = notaMaisProximaMidi((double) foutAtual);
        freqAlvo  = (float) (440.0 * std::pow(2.0, (alvoMidi - 69) / 12.0));
        centsCorr = centsEntre(f0Atual, foutAtual);
    } else {
        freqAlvo = 0.0f;
        centsCorr = 0.0f;
    }

    histCents[escritaHist] = centsCorr;
    histVoz  [escritaHist] = temVoz;
    escritaHist = (escritaHist + 1) % N_HIST;

    repaint();
}

void PainelAfinador::paint(juce::Graphics& g) {
    auto area = getLocalBounds().toFloat();

    // Dois paineis: o instrumento a esquerda, o historico a direita.
    auto esquerda = area.removeFromLeft(250.0f);
    area.removeFromLeft(9.0f);
    auto direita = area;

    for (auto r : { esquerda, direita }) {
        g.setColour(tccColours::fundoPainel);
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(tccColours::borda);
        g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.0f);
    }

    auto dentro = esquerda.reduced(13.0f, 11.0f);
    desenharCabecalho(g, dentro.removeFromTop(56.0f));
    desenharArco(g, dentro.withTrimmedTop(7.0f));
    desenharHistorico(g, direita.reduced(13.0f, 11.0f));
}

void PainelAfinador::desenharCabecalho(juce::Graphics& g, juce::Rectangle<float> r) {
    auto linha = r.removeFromBottom(1.0f);
    g.setColour(tccColours::borda);
    g.fillRect(linha);
    r.removeFromBottom(8.0f);

    g.setColour(tccColours::textoFraco);
    g.setFont(juce::Font(8.5f, juce::Font::bold));
    g.drawText("NOTA-ALVO", r.removeFromTop(11.0f).toNearestInt(), juce::Justification::topLeft);

    // Nome da nota: a peca de identidade da tela.
    juce::String nomeNota("--");
    if (temVoz) nomeNota = juce::String(hzParaNota((double) freqAlvo).c_str());
    g.setColour(tccColours::destaque);
    g.setFont(juce::Font(34.0f, juce::Font::bold));
    g.drawText(nomeNota, r.toNearestInt(), juce::Justification::bottomLeft);

    // Os dois Hz, um sob o outro, na cor do que cada um representa.
    auto colDir = r.removeFromRight(r.getWidth() * 0.55f);
    g.setFont(juce::Font(11.0f));
    g.setColour(tccColours::textoSecundario);
    g.drawText(temVoz ? juce::String::formatted("%.1f Hz", freqAlvo) : juce::String(),
               colDir.removeFromTop(14.0f).toNearestInt(), juce::Justification::bottomRight);
    g.setFont(juce::Font(9.5f));
    g.setColour(tccColours::cantado);
    g.drawText(temVoz ? juce::String::formatted("cantado %.1f", f0Atual) : juce::String(),
               colDir.removeFromTop(13.0f).toNearestInt(), juce::Justification::topRight);
}

void PainelAfinador::desenharArco(juce::Graphics& g, juce::Rectangle<float> r) {
    const float esp   = 13.0f;
    const float raio  = juce::jmin(r.getWidth() * 0.5f, r.getHeight()) - esp * 0.5f - 3.0f;
    const juce::Point<float> centro { r.getCentreX(), r.getBottom() - 12.0f };
    const float meiaVolta = juce::MathConstants<float>::halfPi;

    auto ang = [meiaVolta](float cents) {
        return juce::jlimit(-1.0f, 1.0f, cents / CENTS_ESCALA) * meiaVolta;
    };
    const juce::PathStrokeType trilho(esp, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded);

    // Trilho.
    g.setColour(tccColours::fundoFosso);
    g.strokePath(arcoEntre(centro, raio, -meiaVolta, meiaVolta), trilho);

    // Zona morta: a Tolerancia desenhada. Dentro dela o motor nao corrige, e
    // ver isso na tela e' metade da explicacao do parametro.
    if (auto* pTol = proc.apvts.getRawParameterValue("tol")) {
        const float tol = pTol->load();
        if (tol > 0.0f) {
            g.setColour(tccColours::destaque.withAlpha(0.10f));
            g.strokePath(arcoEntre(centro, raio, ang(-tol), ang(tol)), trilho);
        }
    }

    if (temVoz) {
        const float desvCantado = centsEntre(freqAlvo, f0Atual);
        const float desvSaida   = centsEntre(freqAlvo, foutAtual);

        // Rastro: do que foi cantado ate o que saiu. O comprimento deste
        // trecho E' a correcao aplicada.
        g.setColour(tccColours::destaque.withAlpha(0.30f));
        g.strokePath(arcoEntre(centro, raio, ang(desvCantado), ang(desvSaida)), trilho);

        g.setColour(tccColours::cantado);
        g.drawLine({ centro, polar(centro, raio, ang(desvCantado)) }, 2.2f);
        g.setColour(tccColours::destaque);
        g.drawLine({ centro, polar(centro, raio, ang(desvSaida)) }, 2.6f);
    }

    // Marcas de escala.
    g.setColour(tccColours::textoFraco);
    for (float c : { -CENTS_ESCALA, 0.0f, CENTS_ESCALA }) {
        auto a = ang(c);
        g.drawLine({ polar(centro, raio - esp * 0.5f - 1.0f, a),
                     polar(centro, raio + esp * 0.5f + 3.0f, a) }, 1.4f);
    }
    g.setFont(juce::Font(8.0f));
    g.drawText("-50", juce::Rectangle<float>(r.getX(), centro.y + 2.0f, 26.0f, 11.0f).toNearestInt(),
               juce::Justification::centredLeft);
    g.drawText("+50", juce::Rectangle<float>(r.getRight() - 26.0f, centro.y + 2.0f, 26.0f, 11.0f).toNearestInt(),
               juce::Justification::centredRight);
    // O "0" vai DENTRO do arco, logo abaixo da marca do topo: acima dela ficaria
    // fora do retangulo do painel e nao seria desenhado.
    g.drawText("0", juce::Rectangle<float>(centro.x - 12.0f, centro.y - raio + esp * 0.5f + 3.0f,
                                           24.0f, 11.0f).toNearestInt(),
               juce::Justification::centred);

    // Eixo.
    g.setColour(tccColours::fundoPainel);
    g.fillEllipse(centro.x - 5.0f, centro.y - 5.0f, 10.0f, 10.0f);
    g.setColour(tccColours::destaque);
    g.drawEllipse(centro.x - 5.0f, centro.y - 5.0f, 10.0f, 10.0f, 1.6f);

    // Leitura central: quantos cents estao sendo somados ou tirados AGORA.
    juce::Rectangle<float> leitura(centro.x - 60.0f, centro.y - 56.0f, 120.0f, 40.0f);
    g.setColour(tccColours::destaque);
    g.setFont(juce::Font(26.0f, juce::Font::bold));
    g.drawText(temVoz ? juce::String::formatted("%+.0f", centsCorr) : juce::String("--"),
               leitura.removeFromTop(28.0f).toNearestInt(), juce::Justification::centred);
    g.setColour(tccColours::textoSecundario);
    g.setFont(juce::Font(8.5f, juce::Font::bold));
    juce::String seta = (! temVoz || std::abs(centsCorr) < 0.5f) ? "CENTS"
                      : (centsCorr > 0.0f ? "CENTS  ^" : "CENTS  v");
    g.drawText(seta, leitura.toNearestInt(), juce::Justification::centredTop);
}

void PainelAfinador::desenharHistorico(juce::Graphics& g, juce::Rectangle<float> r) {
    g.setColour(tccColours::textoFraco);
    g.setFont(juce::Font(8.5f, juce::Font::bold));
    auto titulo = r.removeFromTop(12.0f);
    g.drawText("CORRECAO AO LONGO DO TEMPO", titulo.toNearestInt(), juce::Justification::centredLeft);
    g.drawText("2,5 s", titulo.toNearestInt(), juce::Justification::centredRight);
    r.removeFromTop(5.0f);

    auto rodape = r.removeFromBottom(13.0f);
    g.setColour(tccColours::textoFraco);
    g.setFont(juce::Font(8.5f));
    g.drawText("acima da faixa clara = correcao maior que a tolerancia",
               rodape.toNearestInt(), juce::Justification::centredLeft);
    r.removeFromBottom(4.0f);

    g.setColour(tccColours::fundoFosso);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(tccColours::borda);
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);

    auto yDe = [&r](float cents) {
        return r.getCentreY() - juce::jlimit(-1.0f, 1.0f, cents / CENTS_ESCALA)
                              * (r.getHeight() * 0.5f - 6.0f);
    };
    const float yZero = yDe(0.0f);

    // Faixa da tolerancia: mesma leitura do arco, agora no tempo.
    if (auto* pTol = proc.apvts.getRawParameterValue("tol")) {
        const float tol = pTol->load();
        if (tol > 0.0f) {
            g.setColour(tccColours::destaque.withAlpha(0.06f));
            g.fillRect(juce::Rectangle<float>(r.getX() + 1.0f, yDe(tol),
                                              r.getWidth() - 2.0f, yDe(-tol) - yDe(tol)));
        }
    }

    g.setColour(tccColours::grade);
    g.drawLine(r.getX(), yDe(CENTS_ESCALA), r.getRight(), yDe(CENTS_ESCALA), 1.0f);
    g.drawLine(r.getX(), yDe(-CENTS_ESCALA), r.getRight(), yDe(-CENTS_ESCALA), 1.0f);
    g.setColour(tccColours::borda);
    g.drawLine(r.getX(), yZero, r.getRight(), yZero, 1.2f);

    g.setColour(tccColours::textoFraco);
    g.setFont(juce::Font(8.0f));
    g.drawText("+50", juce::Rectangle<float>(r.getX() + 3.0f, yDe(CENTS_ESCALA) - 1.0f, 24.0f, 10.0f).toNearestInt(),
               juce::Justification::centredLeft);
    g.drawText("0", juce::Rectangle<float>(r.getX() + 3.0f, yZero - 5.0f, 24.0f, 10.0f).toNearestInt(),
               juce::Justification::centredLeft);
    g.drawText("-50", juce::Rectangle<float>(r.getX() + 3.0f, yDe(-CENTS_ESCALA) - 9.0f, 24.0f, 10.0f).toNearestInt(),
               juce::Justification::centredLeft);

    // O traco, em trechos: cada regiao vozeada e' um trecho proprio, para que o
    // silencio vire buraco em vez de uma reta ligando duas frases.
    auto xDe = [&r](int i) {
        return r.getX() + r.getWidth() * (float) i / (float) (N_HIST - 1);
    };
    int i = 0;
    while (i < N_HIST) {
        while (i < N_HIST && ! histVoz[(escritaHist + i) % N_HIST]) ++i;
        const int ini = i;
        while (i < N_HIST && histVoz[(escritaHist + i) % N_HIST]) ++i;
        const int fim = i;
        if (fim - ini < 2) continue;

        juce::Path traco, area;
        for (int k = ini; k < fim; ++k) {
            const float x = xDe(k), y = yDe(histCents[(escritaHist + k) % N_HIST]);
            if (k == ini) { traco.startNewSubPath(x, y); area.startNewSubPath(x, yZero); area.lineTo(x, y); }
            else          { traco.lineTo(x, y);          area.lineTo(x, y); }
        }
        area.lineTo(xDe(fim - 1), yZero);
        area.closeSubPath();

        g.setColour(tccColours::destaque.withAlpha(0.22f));
        g.fillPath(area);
        g.setColour(tccColours::destaque);
        g.strokePath(traco, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    // "Agora" na borda direita.
    g.setColour(tccColours::destaque.withAlpha(0.55f));
    g.drawLine(r.getRight() - 1.0f, r.getY(), r.getRight() - 1.0f, r.getBottom(), 1.5f);
    if (temVoz) {
        g.setColour(tccColours::destaque);
        g.fillEllipse(r.getRight() - 4.0f, yDe(centsCorr) - 3.0f, 6.0f, 6.0f);
    }
}

// ----------------------------------------------------------------------------
//  TccAutotuneEditor
// ----------------------------------------------------------------------------
TccAutotuneEditor::TccAutotuneEditor(TccAutotuneProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), painel(p)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(painel);

    auto configurarSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& texto,
                                    bool vertical) {
        if (vertical) {
            s.setSliderStyle(juce::Slider::LinearVertical);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
            l.setJustificationType(juce::Justification::centred);
        } else {
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 16);
            l.setJustificationType(juce::Justification::centredLeft);
        }
        addAndMakeVisible(s);
        l.setText(texto, juce::dontSendNotification);
        l.setFont(juce::Font(10.0f));
        addAndMakeVisible(l);
    };
    // Grupo CORRECAO — verticais. Os rotulos vao no nome de catalogo completo:
    // "Retune" sozinho ja foi confundido com outra coisa.
    configurarSlider(tolSlider,      tolLabel,      "Tolerancia (ct)",   true);
    configurarSlider(retuneSlider,   retuneLabel,   "Retune Speed (ms)", true);
    configurarSlider(vibratoSlider,  vibratoLabel,  "Natural Vibrato",   true);
    configurarSlider(humanizeSlider, humanizeLabel, "Humanize",          true);
    // Grupo MOTOR — horizontais, com o rotulo ACIMA do trilho: ao lado, o
    // rotulo mais a caixa de valor comiam a largura toda e sobrava um trilho
    // curto demais para arrastar.
    configurarSlider(lookSlider, lookLabel, "Look-ahead", false);
    configurarSlider(mixSlider,  mixLabel,  "Mix",        false);

    for (auto* l : { &tolLabel, &retuneLabel, &vibratoLabel, &humanizeLabel })
        l->setFont(juce::Font(9.5f));

    auto configurarCombo = [this](juce::ComboBox& c, juce::Label& l, const juce::String& texto,
                                   const juce::StringArray& itens) {
        c.addItemList(itens, 1);
        addAndMakeVisible(c);
        l.setText(texto, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setFont(juce::Font(10.0f));
        addAndMakeVisible(l);
    };
    configurarCombo(vozCombo,    vozLabel,    "Voz",    kVozes);
    configurarCombo(tonicaCombo, tonicaLabel, "Tonica", kTonicas);
    configurarCombo(escalaCombo, escalaLabel, "Escala", kEscalas);

    auto& apvts = processorRef.apvts;
    mixAttach      = std::make_unique<SliderAttachment>(apvts, "mix",      mixSlider);
    tolAttach      = std::make_unique<SliderAttachment>(apvts, "tol",      tolSlider);
    retuneAttach   = std::make_unique<SliderAttachment>(apvts, "retune",   retuneSlider);
    vibratoAttach  = std::make_unique<SliderAttachment>(apvts, "vibrato",  vibratoSlider);
    humanizeAttach = std::make_unique<SliderAttachment>(apvts, "humanize", humanizeSlider);
    lookAttach     = std::make_unique<SliderAttachment>(apvts, "look",     lookSlider);
    vozAttach      = std::make_unique<ComboAttachment>(apvts, "voz",    vozCombo);
    tonicaAttach   = std::make_unique<ComboAttachment>(apvts, "tonica", tonicaCombo);
    escalaAttach   = std::make_unique<ComboAttachment>(apvts, "escala", escalaCombo);

    // Formatacao das caixas de valor — TEM de vir DEPOIS dos attachments.
    // O SliderAttachment instala, no proprio construtor, um
    // 'textFromValueFunction' derivado de param.getText(), que imprime a
    // precisao inteira do float ("15.0000..." reticenciado) e ignora o
    // setNumDecimalPlacesToDisplay do slider. Sobrescrever aqui e' so
    // apresentacao: o valor do parametro nao e' tocado, e digitar na caixa
    // continua passando pelo 'valueFromTextFunction' do parametro.
    auto formatar = [](juce::Slider& s, int casas, double fator = 1.0,
                        const juce::String& sufixo = {}) {
        s.textFromValueFunction = [casas, fator, sufixo](double v) {
            return juce::String(v * fator, casas) + sufixo;
        };
        // Se a exibicao muda de unidade, o caminho de VOLTA tem de mudar
        // junto: sem isto, digitar "50" numa caixa que mostra "100 %" cairia
        // no parser do parametro (que espera 0..1) e saturaria em 1,0.
        if (fator != 1.0)
            s.valueFromTextFunction = [fator](const juce::String& t) {
                return t.getDoubleValue() / fator;
            };
        s.updateText();
    };
    formatar(tolSlider,      1);
    formatar(retuneSlider,   0);
    formatar(vibratoSlider,  2);
    formatar(humanizeSlider, 2);
    formatar(lookSlider,     0);
    // Mix em porcentagem, como a Antares mostra. Item cosmetico do backlog
    // (docs/execucao-do-plano.md, "Backlog tecnico"): o parametro segue 0..1.
    formatar(mixSlider, 0, 100.0, " %");

    setSize(640, 430);
}

TccAutotuneEditor::~TccAutotuneEditor() {
    setLookAndFeel(nullptr);
}

void TccAutotuneEditor::desenharGrupo(juce::Graphics& g, juce::Rectangle<int> caixa,
                                       const juce::String& titulo) {
    auto r = caixa.toFloat();
    g.setColour(tccColours::fundoPainel);
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(tccColours::borda);
    g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.0f);

    // O titulo "morde" a borda de cima: apaga o trecho com a cor da janela e
    // escreve por cima.
    g.setFont(juce::Font(8.5f, juce::Font::bold));
    const float larg = g.getCurrentFont().getStringWidthFloat(titulo) + 12.0f;
    juce::Rectangle<float> selo(r.getX() + 11.0f, r.getY() - 5.0f, larg, 10.0f);
    g.setColour(tccColours::fundoJanela);
    g.fillRect(selo);
    g.setColour(tccColours::destaque);
    g.drawText(titulo, selo.toNearestInt(), juce::Justification::centred);
}

void TccAutotuneEditor::paint(juce::Graphics& g) {
    g.fillAll(tccColours::fundoJanela);

    // Barra de titulo.
    g.setColour(tccColours::destaque);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText("TCC AUTOTUNE", faixaTitulo, juce::Justification::centredLeft);
    g.setColour(tccColours::textoFraco);
    g.setFont(juce::Font(8.5f));
    g.drawText("pYIN  ->  TD-PSOLA", faixaTitulo, juce::Justification::centredRight);

    desenharGrupo(g, caixaEscala,   "ESCALA");
    desenharGrupo(g, caixaCorrecao, "CORRECAO");
    desenharGrupo(g, caixaMotor,    "MOTOR");

    // Latencia real, lida do processor. Nao pode ser numero fixo: a guarda do
    // PSOLA e' proporcional a fs/FMIN, e o FMIN vem do preset de Voz -- trocar
    // "soprano" por "baixo" muda a latencia.
    const double fs  = processorRef.getSampleRate();
    const int    lat = processorRef.getLatencySamples();
    g.setColour(tccColours::textoFraco);
    g.setFont(juce::Font(8.5f));
    g.drawText(fs > 0.0 ? juce::String::formatted("LATENCIA  %.1f ms", 1000.0 * lat / fs)
                        : juce::String("LATENCIA  --"),
               caixaMotor.reduced(11, 0).removeFromBottom(16),
               juce::Justification::centredLeft);
}

void TccAutotuneEditor::resized() {
    auto area = getLocalBounds().reduced(12);

    faixaTitulo = area.removeFromTop(14);
    area.removeFromTop(8);
    painel.setBounds(area.removeFromTop(196));
    area.removeFromTop(20);

    // Tres grupos numa linha. Cabe porque sao 9 controles, nao 13 -- ver a
    // Decisao 8 em docs/historico-e-decisoes.md.
    auto faixa = area;
    caixaEscala = faixa.removeFromLeft((int) ((float) faixa.getWidth() * 0.27f));
    faixa.removeFromLeft(9);
    caixaMotor = faixa.removeFromRight((int) ((float) area.getWidth() * 0.27f));
    faixa.removeFromRight(9);
    caixaCorrecao = faixa;

    // ESCALA: tres linhas de "rotulo + combo".
    {
        auto dentro = caixaEscala.reduced(11, 0).withTrimmedTop(16).withTrimmedBottom(10);
        const int alturaLinha = dentro.getHeight() / 3;
        auto linha = [&](juce::Label& l, juce::ComboBox& c) {
            auto row = dentro.removeFromTop(alturaLinha).reduced(0, 3);
            l.setBounds(row.removeFromLeft(46));
            c.setBounds(row);
        };
        linha(vozLabel,    vozCombo);
        linha(tonicaLabel, tonicaCombo);
        linha(escalaLabel, escalaCombo);
    }

    // CORRECAO: quatro colunas de slider vertical.
    {
        auto dentro = caixaCorrecao.reduced(8, 0).withTrimmedTop(16).withTrimmedBottom(8);
        const int largura = dentro.getWidth() / 4;
        auto coluna = [&](juce::Slider& s, juce::Label& l) {
            auto col = dentro.removeFromLeft(largura);
            l.setBounds(col.removeFromTop(24));
            s.setBounds(col);
        };
        coluna(tolSlider,      tolLabel);
        coluna(retuneSlider,   retuneLabel);
        coluna(vibratoSlider,  vibratoLabel);
        coluna(humanizeSlider, humanizeLabel);
    }

    // MOTOR: dois sliders horizontais, rotulo em cima; o rodape com a latencia
    // e' pintado, nao e' componente.
    {
        auto dentro = caixaMotor.reduced(11, 0).withTrimmedTop(16).withTrimmedBottom(20);
        auto linha = [&](juce::Label& l, juce::Slider& s) {
            l.setBounds(dentro.removeFromTop(14));
            s.setBounds(dentro.removeFromTop(22));
            dentro.removeFromTop(10);
        };
        linha(lookLabel, lookSlider);
        linha(mixLabel,  mixSlider);
    }
}
