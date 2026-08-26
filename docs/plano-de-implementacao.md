# Plano de implementação — camada de correção e expressão

> **Data:** 2026-08-26
> **Status:** 📐 **PLANO — nada implementado.**
> **Decisões que o originam:** [historico-e-decisoes.md](historico-e-decisoes.md)
> **Fundamentação:** [pesquisa-retune-speed-e-cor.md](pesquisa-retune-speed-e-cor.md) ·
> [pesquisa-bibliografica.md](pesquisa-bibliografica.md)

---

## 1. O problema estrutural que o plano resolve primeiro

A malha de correção — zona morta, filtro, reset no ataque — está **duplicada literalmente em
três arquivos**:

| Arquivo | Linhas | Papel |
|---|---|---|
| `src/offline_causal/main.cpp` | 167–179 | caminho A (gold) |
| `src/offline_causal/autotune_rt.cpp` | 198–211 | caminho B (causal) |
| `src/c1_streaming/autotune_stream.h` | 426–438 | caminho C1 (streaming) |

O mesmo bloco, copiado. Hoje isso já é frágil; com Retune Speed, Natural Vibrato e Humanize
entrando, torna-se insustentável — **qualquer divergência entre as três cópias quebra
silenciosamente a comparação C1 × gold**, que é a base de toda a verificação do trabalho.

> **Etapa 0 do plano: extrair a malha para uma estrutura única em `dsp.h`.**
> Refatoração pura, sem mudança de comportamento, verificável pelos testes existentes.
> Tudo o mais depende disso.

---

## 2. A cadeia hoje

```
f0 detectado
   │
   ├─ midi      = 69 + 12·log2(f/440)
   ├─ alvo      = nota mais próxima permitida pela escala
   ├─ errCents  = (alvo − midi)·100
   ├─ mov       = |errCents| ≤ tol ? 0 : sign·(|errCents| − tol)      ← Tolerância
   ├─ corrMidi  = midi + forca·mov/100                                 ← Forca
   ├─ alvoCents = 1200·log2(alvoHz/FMIN)
   ├─ estado    = tinhaNota ? α·estado + (1−α)·alvoCents : alvoCents   ← Glide (no ALVO)
   └─ fout      = FMIN·2^(estado/1200)
```

Dois problemas, já diagnosticados:

1. O filtro age sobre `alvoCents`, que é **quase constante dentro de uma nota** — o filtro
   converge e deixa de agir. Não preserva vibrato.
2. O reset no ataque é para `alvoCents` — a nota **nasce exatamente afinada**, sem trajeto.

---

## 3. A cadeia proposta

```
f0 detectado
   │
   ├─ realCents = 1200·log2(f0/FMIN)
   ├─ alvoCents = 1200·log2(notaAlvo(f0, tol)/FMIN)         ← Tolerância (inalterada)
   │
   │  ── no ataque de nota:  lpAlvo = lpReal = realCents ──  ← nasce onde o cantor está
   │
   ├─ lpAlvo    = α·lpAlvo + (1−α)·alvoCents                ← Retune Speed
   ├─ lpReal    = α·lpReal + (1−α)·realCents                ← Retune Speed (mesmo α)
   │
   ├─ outCents  = lpAlvo + k·(realCents − lpReal)           ← k = Natural Vibrato
   └─ fout      = FMIN·2^(outCents/1200)
```

Dois estados de filtro em vez de um. `forca` desaparece.

---

## 4. Por que esta forma funde o Glide com o Retune Speed

Este é o ponto central do plano, e o que torna a mudança segura.

Escrevendo `LP(·)` para o filtro de 1 polo e `HP(x) = x − LP(x)` para o complementar:

```
outCents = LP(alvo) + k·HP(real)
```

### 4.1 `k = 0` reproduz o Glide atual — com uma ressalva

```
outCents = LP(alvo)
```

que é **literalmente** a linha de hoje (`estado = α·estado + (1−α)·alvoCents`). O comportamento
antigo não é perdido — vira um caso particular.

> ⚠️ **Ressalva, corrigida em 2026-08-26:** a equivalência vale para o **regime**, não para o
> **ataque**. O código atual inicializa o estado em `alvoCents`; a cadeia nova inicializa em
> `realCents`. Reproduzir o comportamento antigo exige `k = 0` **e** a flag `ataqueNoAlvo`.
> Ver §11.1.

### 4.2 `k = 1` é o Retune Speed da patente

```
outCents = LP(alvo) + real − LP(real) = real + LP(alvo − real) = real + LP(mov)
```

O filtro passa a agir sobre a **correção**, que é a formulação de Hildebrand US 5.973.252.

### 4.3 `k > 1` é o Natural Vibrato positivo

Com o alvo constante dentro da nota, `LP(alvo) → alvo` e a saída vira `alvo + k·vibrato`.

| `k` | Comportamento | Equivalente Antares |
|---:|---|---|
| 0 | vibrato removido; alvo suavizado | *(o Glide de hoje)* |
| 1 | vibrato preservado | Retune Speed puro |
| > 1 | vibrato exagerado | Natural Vibrato positivo |

> ### 🎯 A consequência que torna isto de baixo risco
> **Um único par de filtros, dois parâmetros (τ e k), e o comportamento antigo é
> recuperável exatamente com `k = 0`.** A fusão não é uma troca — é uma generalização.
> Isso dá um caminho de A/B honesto: com `k = 0` e o τ atual, a saída deve bater com a versão
> anterior **amostra a amostra**. Esse é o teste de não-regressão da etapa.

---

## 5. O reset no ataque

O detalhe que devolve o gesto de ataque, e a razão de os **dois** estados serem inicializados
com `realCents`:

```
outCents(ataque) = lpAlvo + k·(realCents − lpReal)
                 = realCents + k·(realCents − realCents)
                 = realCents
```

A nota **nasce na afinação que o cantor realmente produziu** e desliza dali até o alvo em ~τ.
É exatamente o gesto que a [§8.2 da documentação técnica](documentacao-tecnica.md) identificou
como apagado.

**Risco associado, que precisa ficar no texto:** um ataque errado fica audível por ~τ. Com
τ = 100 ms e o cantor entrando 200 cents fora, o erro é claramente perceptível. É o preço
direto da naturalidade — e é exatamente o que o **Humanize** existe para resolver (§7).

---

## 6. Parâmetros — antes e depois

| Parâmetro | Hoje | Depois | Nota |
|---|---|---|---|
| `Forca` | 0–1, pad. 1,0 | ❌ **removido** | substituído pelo `Mix` |
| `Tolerancia` | 0–50 ct, pad. 15 | ✅ **mantido, mesmo nome** | não é Flex-Tune — ver pesquisa §2 |
| `Glide` | 0–200 ms, pad. 40 | ➡️ vira `Retune Speed` | 0–200 ms, **padrão 25 ms** |
| — | — | ➕ `Natural Vibrato` (k) | 0–2,0, **padrão 1,0** |
| — | — | ➕ `Mix` | 0–100 %, **padrão 100 %** |
| — | — | ➕ `Key` | 12 tônicas |
| `Escala` | 7 combos | ➡️ `Scale` | cromática / maior / menor natural |
| `Look-ahead` | 0–16, pad. 4 | ✅ mantido | |
| `Voz` | 7 presets, pad. Contralto | ✅ mantido | |

Faixa e padrão do Retune Speed vêm do manual da Antares: *"A setting between 10 and 50 is
typical for more natural sounding pitch correction"*; **0 fica reservado ao efeito duro** e
precisa continuar alcançável.

---

## 7. Etapas, em ordem de execução

Cada etapa é verificável isoladamente. **Nenhuma etapa avança sem a anterior verde.**

| # | Etapa | Muda comportamento? | Como verificar |
|---|---|---|---|
| **0** | Extrair a malha de correção para `CorretorAltura` em `dsp.h`; os 3 caminhos passam a chamá-la | ❌ não | `bench_stream.py`, `bench_pitch.py`, `bench_frames.py` inalterados |
| **1** | `Key` × `Scale` (24 tonalidades) | só amplia opções | escalas antigas continuam produzindo o mesmo `g_permitida` |
| **2** | `Mix` seco/molhado + remoção da `Forca` | sim | identidade bit-perfect com `mix = 0` |
| **3** | Filtro sobre a correção + `k` (Retune Speed + Natural Vibrato) | sim | **`k = 0` deve bater amostra a amostra com a versão anterior** |
| **4** | K2 · Humanize (τ variável desde o ataque) | sim | `humanize = 0` → idêntico à etapa 3 |
| **5** | K3/K4 · Create Vibrato + Amplitude Amount | sim | `shape = none` → idêntico à etapa 4 |

**O padrão de verificação é o mesmo em todas:** cada etapa introduz um parâmetro cujo valor
neutro reproduz exatamente a etapa anterior. Isso dá um teste de não-regressão automático a
cada passo, e é o que permite mexer no núcleo de DSP sem perder o chão.

---

## 8. Mudanças por arquivo

| Arquivo | Etapas | O que muda |
|---|---|---|
| `src/core/dsp.h` | 0,1,2,3,4,5 | `CorretorAltura`; `notaAlvo()` perde `forca`; `definirEscala()` ganha montagem por tônica |
| `src/offline_causal/main.cpp` | 0,2 | substituir o bloco de glide pela chamada à struct; aplicar mix |
| `src/offline_causal/autotune_rt.cpp` | 0,2 | idem |
| `src/c1_streaming/autotune_stream.h` | 0,2,3 | idem; `StreamParams` ganha `retuneMs`, `vibrato`, `mix`, `humanize` e perde `forca` |
| `src/c1_streaming/stream_test.cpp` | 1,2 | novas flags de CLI |
| `plugin/PluginProcessor.cpp/.h` | 1,2,3,4,5 | APVTS: remover `forca`, adicionar `key`, `mix`, `vibrato`, `humanize` |
| `plugin/PluginEditor.cpp/.h` | 1,2,3,4,5 | dois combos no lugar de um; novos sliders |
| `python/bench_stream.py` | 2,3 | trocar o teste de identidade de `forca=0` para `mix=0`; **acrescentar** teste com `k=0` |

---

## 9. Testes — o que precisa mudar

### 9.1 O teste de identidade

Hoje `bench_stream.py` verifica saída bit-idêntica com `forca = 0`. Com a `Forca` removida:

- **substituir** por `mix = 0` (não processa, devolve a entrada);
- **acrescentar** um teste que force `β = 1` por outro caminho.

Motivo: `forca = 0` e `mix = 0` produzem o mesmo áudio por razões diferentes. O primeiro fazia
o PSOLA **rodar** com β = 1 — e era isso que pegava drift de fase. O segundo faz *bypass*, e
não exercita o PSOLA. **Perder esse teste seria perder cobertura sem perceber.**

### 9.2 Re-baseline obrigatório

⚠️ Depois da etapa 3, **os números de "similaridade com o offline" já medidos deixam de ser
comparáveis** — o gold também muda. Antes de mexer, gravar o baseline atual; depois, remedir.
Sem isso, a tabela de resultados do TCC 2 fica sem referência.

### 9.3 O que só a escuta responde

Nenhum teste automático decide: o valor padrão de τ, se `k = 1` soa melhor que `k = 1,2`, e se
a fusão do Glide fez falta em legato. **Isso é teste de escuta, com o mesmo usuário do teste
anterior**, e deve ser planejado junto com a implementação — não depois.

---

## 10. Riscos

| Risco | Mitigação |
|---|---|
| Divergência entre gold, causal e streaming | etapa 0 resolve na raiz: uma única implementação |
| Perda de cobertura de teste ao remover `Forca` | §9.1 — dois testes no lugar de um |
| Ataque errado audível por ~τ | Humanize (etapa 4); τ padrão conservador (25 ms) |
| Quebra de estado salvo na DAW | aceita e documentada — o plugin não tem base instalada |
| Números do TCC 1 deixarem de valer | §9.2 — re-baseline explícito |
| A fusão do Glide remover algo audível | `k = 0` reproduz o comportamento antigo; A/B possível a qualquer momento |

---

## 11. Decisões de desenho tomadas em 2026-08-26

### 11.1 A equivalência com o comportamento antigo precisa de **dois** valores neutros

Registro de uma **correção a afirmação anterior deste próprio documento**: a §4.1 dizia que
`k = 0` reproduz o Glide atual "exatamente". **Não reproduz** — o reset de ataque difere.

| | Estado inicial no ataque |
|---|---|
| Código atual | `estado = alvoCents` — a nota **nasce afinada** |
| Cadeia nova | `lpAlvo = lpReal = realCents` — a nota **nasce onde o cantor cantou** |

A equivalência exata exige `vibrato = 0` **e** um segundo interruptor que restaure o reset
antigo. Sem isso, o teste de não-regressão da etapa 3 falharia por um motivo legítimo, e o
tempo seria gasto caçando um bug que não existe.

**Decisão:** `ataqueNoAlvo` entra como **flag interna de `ParamsCorrecao`**, usada apenas pelo
teste de comparação. **Não vira parâmetro de plugin** (ver §11.2).

### 11.2 O deslize de entrada é fixo, não é controle

**Decisão do autor:** a nota sempre nasce na afinação cantada e desliza até o alvo. Não haverá
botão para escolher entre os dois comportamentos.

**Motivos:** é um controle a menos na interface; o Auto-Tune também não expõe essa escolha; e
o deslize de entrada é justamente o gesto que o diagnóstico apontou como apagado — deixá-lo
opcional enfraqueceria a demonstração do resultado.

### 11.3 A escala global fica como está — limitação documentada

`g_permitida[12]` (`src/core/dsp.h:109`) é **estado global**, não estado de instância. Duas
instâncias do plugin na mesma sessão compartilham a escala: mudar numa muda na outra.

**Decisão do autor: fora do escopo, registrado como limitação conhecida.**

**Motivos:** já é assim hoje, então **não é regressão** introduzida por este plano; o uso
previsto no TCC é uma faixa por vez; e não tem relação com nenhum dos dois requisitos que o
teste de usuário reprovou.

> 📌 **Para o texto do TCC:** esta limitação deve ser declarada explicitamente na seção de
> limitações, junto com o caminho de correção (mover `g_permitida` para dentro do estado do
> `AutotuneStream`). Registrar uma limitação conhecida é mais forte do que a banca descobri-la.

---

## 12. O que este plano **não** cobre

- **Modo de baixa latência** — especificado em
  [modo-baixa-latencia.md](modo-baixa-latencia.md), com 6 questões em aberto. É trabalho de
  arquitetura de detecção, ortogonal a este plano.
- **K5 · Flex-Tune de verdade** e **K6 · Targeting Ignores Vibrato** — mecanismos novos, ainda
  sem decisão de desenho.
- **Detune / Transpose / Tracking** — decididos como ausentes, não priorizados.
- **Formante / Throat** — descartados com fundamentação; ver
  [pesquisa-retune-speed-e-cor.md §3.1](pesquisa-retune-speed-e-cor.md).
