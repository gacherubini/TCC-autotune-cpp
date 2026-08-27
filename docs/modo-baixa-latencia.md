# Modo de baixa latência — especificação

> **Data:** 2026-08-26
> **Status:** ⚠️ **ESPECIFICAÇÃO — NADA IMPLEMENTADO.**
> Este documento existe para responder *"o que exatamente o modo de baixa latência vai fazer?"*
> **antes** de qualquer linha de código ser escrita. Nenhuma decisão de implementação deve ser
> tomada sem que as questões em aberto da §8 estejam resolvidas.
> **Referências:** [comparacao-antares.md](comparacao-antares.md) ·
> [pesquisa-bibliografica.md](pesquisa-bibliografica.md) ·
> [documentacao-tecnica.md §8.1 e §9.1](documentacao-tecnica.md)
>
> ### ⚠️ Leia [pesquisa-latencia-antares.md](pesquisa-latencia-antares.md) antes deste documento
>
> A pesquisa de 2026-08-27 sobre como o Auto-Tune declara 37 amostras mudou duas coisas aqui:
>
> 1. **Os números de latência deste documento são do preset contralto** (FMIN 175 Hz). Com o
>    FMIN padrão de 80 Hz, a guarda do PSOLA dobra: **o v2 dá 12,5 ms, não 5,7 ms** — dentro da
>    faixa de 7 a 13 ms onde a coloração tímbrica começa, e portanto **na fronteira, não abaixo
>    dela**. A §4 e a §10 abaixo continuam corretas para contralto e otimistas para o resto.
> 2. **Existe um v3 que este documento não considerava.** Depois que `nFrame` e `look` saem, o
>    que sobra é a guarda do PSOLA, que é `fs/FMIN` **por construção**. Enquanto a síntese for
>    PSOLA, a latência é proporcional ao período da nota mais grave aceita — é um piso
>    arquitetural, não um parâmetro. Passar dele exige trocar o motor de síntese.

---

## 1. O que é, em uma frase

Um **preset de troca**: um único controle que reconfigura três parâmetros do motor de uma vez,
comprando latência ao preço de robustez de detecção. Não é um algoritmo novo — é um conjunto
nomeado de concessões, com o custo declarado.

O Auto-Tune tem exatamente isso (opção *Use Low Latency*, documentada nos manuais Antares) e
**declara os números**: 112 amostras (2,3 ms) em Modern Mode e 37 amostras (0,77 ms) em Classic
Mode, a 48 kHz, **apenas com Low Latency ativo**. Fora desse modo, a Antares não publica número.

---

## 2. De onde vêm os 57,9 ms hoje

`src/c1_streaming/autotune_stream.h:82`:

```cpp
latSamples = p.nFrame + p.look * p.nHop + psolaGuard;
```

Com os parâmetros de fábrica (frame 1024, hop 256, look 4, preset contralto FMIN = 175 Hz),
a 44,1 kHz:

| Termo | Fórmula | Amostras | ms | O que compra |
|---|---|---|---|---|
| Quadro de análise | `nFrame` | 1024 | 23,2 | sinal suficiente para o CMNDF medir o período |
| Look-ahead do Viterbi | `look · nHop` | 1024 | 23,2 | trajetória de pitch mais confiável |
| Guarda do PSOLA | `2 · fs/FMIN` | 504 | 11,4 | o grão da última marca não invade a região já emitida |
| **Total** | | **2552** | **57,9** | |

Os três termos são **independentes** e atacáveis separadamente.

---

## 3. As três mudanças do modo v1 (sem trocar de arquitetura)

### L1 · `look` = 0

**O que faz:** desliga o look-ahead do Viterbi de lag fixo. O decodificador passa a ser
**guloso** — decide o pitch do quadro atual usando só o presente e o passado.

**Ganho:** −1024 amostras = **−23,2 ms**. É o corte mais barato que existe, e o parâmetro já
está exposto no plugin.

**Custo medido:** a similaridade com o offline cai de ~0,65 para ~0,56
(ver [historico-e-decisoes.md](historico-e-decisoes.md), varredura de look-ahead).

> ⚠️ **Ressalva metodológica:** similaridade com o offline **não é qualidade percebida**. Ela
> mede o quanto a saída causal se parece com a saída que conhece o futuro. Para monitoração ao
> vivo, um erro de oitava ocasional pode ser preferível a 23 ms de atraso — mas isso é uma
> hipótese, não um resultado. **Precisa virar teste de escuta no TCC 2.**

---

### L2 · `nFrame` derivado do preset de voz

**O que faz:** hoje `nFrame = 1024` é constante, independente da tessitura escolhida. Mas o
CMNDF só precisa de `2τ` do tom mais grave que se quer detectar — e `τ` é definido pelo preset.
A regra é:

```
nFrame ≥ 2 · fs / FMIN          (porque tauMax = W = nFrame/2)
```

Escolher `nFrame = 1024` para um soprano (FMIN = 262 Hz, que precisaria de apenas 337 amostras)
é **pagar latência por uma capacidade que não se usa**.

**Restrição de implementação:** o CMNDF é calculado diretamente (`dsp.h`, sem FFT), então
`nFrame` **não precisa ser potência de 2** — só precisa ser par, porque `W = nFrame/2`
(`autotune_stream.h:104`). Arredondar para múltiplo de 4 mantém `nHop = nFrame/4` inteiro.

**Custo:** janela de análise mais curta significa **menos amostras somadas no CMNDF**, e
portanto uma `d′(τ)` mais ruidosa. A detecção não perde alcance (ela é dimensionada pelo
preset), mas perde **robustez sob ruído**. Este custo **não foi medido** e precisa ser.

---

### L3 · Guarda do PSOLA de 2 períodos para 1

**O que faz:** hoje `psolaGuard = 2 · fs/FMIN` (`autotune_stream.h:78`). Os 2 períodos existem
porque o grão da **última** marca da janela tem largura estimada pela marca *anterior* — a
próxima ainda não chegou — e portanto é instável. Dois períodos garantem que esse grão nunca
alcance a região já finalizada.

**A mudança:** estimar essa largura a partir do `f0samp` já conhecido (que *é* conhecido até a
fronteira de decisão) em vez da marca vizinha. O grão deixa de ser instável e 1 período basta.

**Ganho:** −252 amostras = **−5,7 ms** no preset contralto.

**Fundamentação:** consistente com a patente do Auto-Tune, que reamostra repetindo ou
descartando **um ciclo inteiro** quando o ponteiro de saída ultrapassa o de entrada — o que
exige exatamente um período de folga, não dois.

**Custo:** risco de artefato de fase nas fronteiras de commit. **Há teste de regressão pronto**
(`python/bench_stream.py`, correlação C1 contra o gold) para validar.

---

## 4. O resultado do modo v1, preset a preset

Com `look = 0`, `nFrame = ceil(2·fs/FMIN)` arredondado para múltiplo de 4, e guarda de 1
período, a 44,1 kHz:

| Preset | FMIN | `nFrame` | Guarda 1T | Total (am.) | **Latência** | Hoje | Ganho |
|---|---:|---:|---:|---:|---:|---:|---:|
| Baixo | 82 | 1076 | 538 | 1614 | **36,6 ms** | 86,4 ms | −49,8 |
| Barítono | 98 | 900 | 450 | 1350 | **30,6 ms** | 76,4 ms | −45,8 |
| Tenor | 131 | 676 | 337 | 1013 | **23,0 ms** | 61,7 ms | −38,7 |
| **Contralto** | 175 | 504 | 252 | 756 | **17,1 ms** | 57,9 ms | −40,8 |
| Mezzo | 220 | 404 | 201 | 605 | **13,7 ms** | 53,3 ms | −39,6 |
| Soprano | 262 | 340 | 168 | 508 | **11,5 ms** | 50,5 ms | −39,0 |
| Instrumento | 50 | 1764 | 882 | 2646 | **60,0 ms** | 126,4 ms | −66,4 |

**A conclusão central do trabalho reaparece aqui:** a latência mínima é ditada pela **nota mais
grave que se quer corrigir**. O modo de baixa latência não escapa disso — ele apenas para de
pagar por notas que o cantor não vai cantar.

> ⚠️ **O preset "Instrumento" (FMIN = 50 Hz) continua inutilizável ao vivo mesmo no modo de
> baixa latência.** Isso é honesto e deve ficar documentado, não escondido.

---

## 5. O que o modo v1 **não** resolve

Esta seção existe para evitar expectativa errada.

| Não resolve | Por quê |
|---|---|
| **O limiar de coloração** | 17,1 ms (contralto) ainda está **acima** dos 13 ms em que Marentakis et al. (2012) mediram coloração significativa para voz com in-ear |
| **Latência do driver** | o buffer ASIO/CoreAudio é do sistema, não do plugin |
| **Tamanho de bloco do host** | quem escolhe é a DAW (64–2048 amostras) |
| **Monitoração ao vivo via PDC** | `setLatencySamples()` alinha a faixa *gravada*; não remove o atraso que o cantor ouve no fone |
| **O piso físico da detecção por quadro** | enquanto o pitch for decidido em lotes de `nFrame`, o termo `nFrame` existe |

### Contexto perceptual — por que 17 ms ainda não basta

Da [pesquisa-bibliografica.md §1.2](pesquisa-bibliografica.md):

| Fonte | Medida | Valor para **voz** |
|---|---|---|
| Lester & Boley (2007), AES 123ª | limiar "Good", in-ear, confiança 85% | **1 ms** |
| Lester & Boley (2007) | limiar "Fair", in-ear | **6,5 ms** |
| Marentakis et al. (2012), DAGA | início da coloração audível, in-ear seco | **entre 7 e 13 ms** |

O teto de "20–30 ms" que aparece nos comentários do repo Python **não tem respaldo em
literatura revisada por pares** — ele rastreia até material de marketing da Antares.

**Portanto: o modo v1 melhora muito, mas não cruza a linha defensável.** Quem cruza é o v2.

---

## 6. O modo v2 — o que falta para chegar lá

O único termo que o v1 não toca é o `nFrame`, e ele não some com ajuste de parâmetro: some com
**mudança de arquitetura de detecção**.

### L6 · CMNDF recursivo em vez de CMNDF por quadro

**A correção conceitual:** a hipótese antiga era *"uma vez travado, basta um período novo em vez
de dois"*. **Isso erra o mecanismo.** Na formulação da patente do Auto-Tune, a janela **continua
sendo de 2 períodos** — mas esses 2 períodos são de sinal **passado**, e o estimador é atualizado
**recursivamente a cada amostra**:

```
E_i(L) = E_{i−1}(L) + x_i²        − x_{i−2L}²
H_i(L) = H_{i−1}(L) + x_i·x_{i−L} − x_{i−L}·x_{i−2L}
```

Custo: **4 multiplicações-acumulações por lag por amostra**. Com ~24 lags numa faixa estreita em
torno do período corrente e fs = 44,1 kHz, são ~4,2 M MAC/s — perfeitamente viável (o xRT atual
é 0,025, há ~40× de folga).

**O que o protótipo paga hoje não é o comprimento da janela — é acumular um quadro inteiro antes
de analisar.** É o **lote**, não a janela.

**Ganho:** elimina os termos `nFrame` e `look · nHop` do orçamento, deixando **apenas a guarda do
PSOLA**:

| Preset | FMIN | Latência v2 | Onde cai |
|---|---:|---:|---|
| Baixo | 82 | **12,2 ms** | acima da coloração |
| Barítono | 98 | **10,2 ms** | zona limítrofe |
| Tenor | 131 | **7,6 ms** | abaixo da coloração |
| **Contralto** | 175 | **5,7 ms** | abaixo da coloração, ~"Fair" in-ear |
| Mezzo | 220 | **4,6 ms** | confortável |
| Soprano | 262 | **3,8 ms** | confortável |
| Instrumento | 50 | **20,0 ms** | continua inviável |

> 📌 **Verificação de consistência:** estes números são **exatamente** a coluna "lat. PSOLA" da
> varredura de tessitura já medida e registrada em
> [historico-e-decisoes.md](historico-e-decisoes.md#experimento-fmin--presets-de-tessitura-2026-06-09).
> Isso não é coincidência: com L6, a latência **passa a ser** a guarda do PSOLA e nada mais.

### A ressalva que precisa ficar no texto do TCC

O rastreio recursivo resolve a latência da **detecção**. Ele **não** resolve, sozinho, a decisão
de vozeamento nem a robustez contra erro de oitava — que hoje são responsabilidade do
HMM/Viterbi. A patente resolve isso com um teste de limiar (`E − 2H ≤ eps·E`) mais verificação
de sub-harmônico: **muito mais simples e menos robusto que o pYIN**.

**Trocar Viterbi por limiar é um retrocesso em robustez que precisa ser medido, não assumido.**

E é exatamente aí que está a contribuição original do trabalho: **quantificar o trade-off entre
latência de detecção e robustez de trajetória** num mesmo pipeline. Essa curva não foi
encontrada publicada.

---

## 7. Como o controle deve aparecer na interface

Três opções, ainda **não decididas**:

| Opção | Como funciona | Prós | Contras |
|---|---|---|---|
| **A — Checkbox** | "Low Latency" liga/desliga; sobrescreve `look` e `nFrame` | igual ao Auto-Tune; simples | esconde o que mudou |
| **B — Checkbox + campos derivados** | idem, mas `look` e `nFrame` ficam visíveis em modo somente-leitura mostrando os valores derivados | transparente, didático | mais trabalho de GUI |
| **C — Três níveis** | Off / Balanceado / Mínimo | granularidade | mais combinações para testar |

**Recomendação preliminar: opção B.** Para um TCC, mostrar *o que* o modo mudou vale mais do
que esconder — a interface vira parte da demonstração do trade-off.

Independente da escolha, o valor de `setLatencySamples()` precisa ser atualizado no mesmo
`processBlock` que aplica a mudança (o caminho de `precisaReprepare` já faz isso).

---

## 8. Questões em aberto — resolver ANTES de implementar

Nenhuma linha de código deve ser escrita enquanto estas perguntas não tiverem resposta:

1. **O custo de `look = 0` é aceitável ao vivo?** A métrica atual (similaridade com o offline)
   não responde. Precisa de teste de escuta cego, com o mesmo usuário do teste anterior.
2. **Quanto de robustez se perde com `nFrame` curto?** Não medido. Precisa de varredura de
   `nFrame` **por preset**, com SNR controlado, medindo GPE e estabilidade — não similaridade.
3. **A guarda de 1 período realmente não gera artefato?** O teste de regressão existe, mas foi
   escrito para a guarda de 2. Precisa ser re-validado com sinal de pico > 1.
4. **O modo v1 sozinho justifica o esforço**, se ele não cruza o limiar de coloração? Ou o
   trabalho deve ir direto ao v2?
5. **Qual é a latência real do Auto-Tune fora do Low Latency Mode?** A Antares não publica.
   Medir por loopback (clique → gravar entrada e saída → correlacionar) dá um baseline próprio,
   que vale mais na banca do que citação de manual.
6. **O preset "Instrumento" deve continuar existindo?** Ele nunca será utilizável ao vivo.
   Manter e documentar a limitação, ou remover do modo de baixa latência?

---

## 9. Como validar quando for implementado

| O que | Como | Critério |
|---|---|---|
| Latência declarada bate com a real | loopback no próprio plugin | erro < 1 amostra |
| Não regrediu qualidade estrutural | `python/bench_stream.py` | correlação por região > 0,99 |
| Sem cliques | contador de "pipoco" | zero |
| Sem alocação em RT | contador de `capacity()` antes/depois | zero realocações |
| Qualidade percebida | teste de escuta cego A/B | a definir com o usuário de teste |

---

## 10. Resumo executivo

| | v1 (parâmetros) | v2 (detecção) | v3 (síntese) |
|---|---|---|---|
| Mudanças | L1 + L2 + L3 | v1 + L6 | v2 + ponteiro móvel no lugar do PSOLA |
| Latência (contralto, FMIN 175) | 17,1 ms | **5,7 ms** | ~0,8 ms |
| Latência (padrão, FMIN 80) | ~23 ms | **12,5 ms** | ~0,8 ms |
| Cruza o limiar de coloração (7–13 ms)? | ❌ não | ⚠️ só no contralto | ✅ sempre |
| Esforço | baixo | alto | alto |
| Risco | baixo (testes cobrem) | alto (vozeamento a medir) | alto (perde formantes; invalida a linha de base) |
| Muda o DSP? | não | **sim** (detecção) | **sim** (síntese — o núcleo) |
| Contribuição original? | não | **sim** — a curva latência × robustez | **sim** — duas arquiteturas de síntese comparadas na mesma base |

O v3 está fundamentado em [pesquisa-latencia-antares.md §6](pesquisa-latencia-antares.md), com
os custos declarados e a medição barata que decide se ele é viável (deslocamento de formante da
reamostragem na faixa de correção real, mensurável hoje com `formantes.py`).
