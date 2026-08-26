# Execução do plano — diário das etapas

> Registro do que foi **efetivamente feito**, etapa por etapa, com a verificação de cada uma.
> O plano está em [plano-de-implementacao.md](plano-de-implementacao.md); este documento é o
> que aconteceu ao executá-lo.

| Etapa | Status | Data |
|---|---|---|
| **0 — malha de correção unificada** | ✅ **concluída** | 2026-08-26 |
| **1 — 24 tonalidades** | ✅ **concluída** (plugin não compilado localmente — ver ressalva) | 2026-08-26 |
| 2 — Mix / remoção da Forca | ⬜ não iniciada | |
| 3 — Retune Speed (funde o Glide) | ⬜ não iniciada | |
| 4 — Humanize | ⬜ não iniciada | |
| 5 — Create Vibrato | ⬜ não iniciada | |

---

## Ferramenta de verificação — `baseline.sh`

Criada antes da Etapa 0, porque **sem ela nenhuma etapa é verificável**.

```bash
./baseline.sh gravar     # grava a referência em baseline/
./baseline.sh conferir   # compara o estado atual contra a referência
```

Compila os três executáveis, roda **17 casos** sobre `exemplo-antes.wav` e resume tudo em
checksums SHA-256 do WAV de saída e do log (com tempos de execução filtrados, para que
variação de máquina não vire ruído). Os casos cobrem bypass, correção cheia, zona morta,
glide, look-ahead, invariância de tamanho de bloco e escalas.

Saída esperada quando nada mudou: **`IDENTICO — nada mudou.`**

> **Portabilidade.** O `compilar.bat` do repositório é só de Windows. O `baseline.sh` detecta o
> compilador disponível (`g++`, ou `clang++` no macOS) e usa as mesmas flags
> (`-std=c++17 -O2 -I external`). **O código compilou sem aviso nem alteração nos três
> arquivos com Apple clang 21** — o projeto é portável, o que não estava registrado.

---

## Etapa 0 — malha de correção unificada

**Concluída em 2026-08-26. Nenhuma mudança de comportamento.**

### O que foi feito

A malha de correção — nota-alvo com zona morta, filtro de glide de 1 polo e reset no ataque —
estava copiada literalmente em três arquivos. Foi extraída para `src/core/dsp.h` como
`ParamsCorrecao` + `CorretorAltura`, e os três caminhos passaram a chamá-la.

| Arquivo | Antes | Depois |
|---|---|---|
| `src/core/dsp.h` | — | `+45` linhas: a malha, uma vez |
| `src/offline_causal/main.cpp` | 13 linhas de malha | 5 linhas de chamada |
| `src/offline_causal/autotune_rt.cpp` | 13 linhas de malha | 5 linhas de chamada |
| `src/c1_streaming/autotune_stream.h` | 13 linhas + 2 membros de estado | 4 linhas + 1 membro |

No streaming, `glideEstado` e `tinhaNota` deixaram de ser membros soltos de `AutotuneStream` e
viraram estado interno de `CorretorAltura`, reinicializado em `reset()` junto com o resto.

### Verificação

```
./baseline.sh conferir
→ IDENTICO — nada mudou.
```

**17/17 casos byte a byte idênticos.** Nenhuma cópia da malha sobrou fora do `dsp.h`
(conferido por busca em `src/` e `plugin/`).

### Achados registrados durante a etapa

Nenhum dos dois foi causado por esta mudança — ambos são **anteriores**, e ficam registrados
porque o baseline os capturou.

#### 1. A invariância ao tamanho de bloco não vale para `block=512`

O README afirma: *"saída idêntica para blocos de 64, 128, 256 e 512"*. Medido:

| Bloco | Resultado |
|---|---|
| 64 | ✅ **idêntico** ao padrão (0 amostras diferentes) |
| 512 | ❌ difere em **457 de 220.500 amostras (0,21%)** |

As divergências se concentram em **exatamente dois trechos de 256 amostras** (um `hop` cada),
em t ≈ 0,372 s e t ≈ 4,516 s. A razão entre as saídas varia de 0,86 a 1,12 dentro do trecho,
com média ≈ 1,00 — ou seja, **não é diferença de ganho, é diferença de forma de onda**.

Isso descarta a hipótese mais óbvia (a normalização de pico por janela, Achado 3 da §8.3, que
produziria um ganho constante) e aponta para uma condição de contorno no disparo de quadros
quando o bloco do host é maior que o `hop`.

**Consequência:** a afirmação do README está forte demais e precisa ser corrigida ou
qualificada antes da defesa. **Não corrigido nesta etapa** — a Etapa 0 não pode mudar
comportamento. Vira item de backlog.

#### 2. `exp()` por amostra dentro do callback de áudio

A malha calcula `alpha = exp(-1/(tau·fs))` **a cada amostra**, embora `tau` só mude quando o
usuário mexe no controle. No caminho de streaming isso acontece dentro do `processBlock()`.

Já era assim antes (`autotune_stream.h:432-433` calculava `exp()` dentro do laço de `nHop`), e
foi **preservado deliberadamente** para que a Etapa 0 não alterasse nada. Nos caminhos offline
a mudança é neutra: o `exp()` saiu de fora do laço para dentro dele, mas com os mesmos
argumentos, então o resultado é bit-idêntico.

**Oportunidade registrada:** cachear `alpha` e recalcular só quando `glideMs` mudar. Vale
medir antes — o xRT atual é 0,025, então provavelmente não é gargalo.

### Por que esta etapa vinha primeiro

Com três cópias, uma divergência entre elas quebraria **em silêncio** a comparação C1 × gold —
o teste passaria a comparar coisas diferentes sem acusar erro. Agora existe uma implementação
só, e as etapas seguintes mudam a matemática **em um lugar**.

---

## Etapa 1 — 24 tonalidades

**Concluída em 2026-08-26. Muda apenas a interface do plugin; o DSP e os CLIs não mudam.**

### O que foi feito

O combo único `Escala`, com 6 tonalidades fixas mais cromático, virou **dois** combos:
`Tonica` (12) × `Escala` (cromática / maior / menor natural) = **24 tonalidades**.

O motor nunca teve a limitação — `definirEscala()` (`dsp.h`) sempre calculou as classes
permitidas para qualquer tônica. A restrição era só a lista de strings do plugin.

| Arquivo | Mudança |
|---|---|
| `src/core/dsp.h` | `+montarEscala(tonica, modo)` — a tabela de nomes, num lugar só |
| `plugin/PluginProcessor.cpp/.h` | novo parâmetro `tonica`; `textoEscala()` delega a `montarEscala()` |
| `plugin/PluginEditor.cpp/.h` | combo novo; a faixa de controles foi de 6 para 7 colunas |
| `src/tests/test_escalas.cpp` | **novo** — 6 seções de verificação |

**Decisão de desenho:** a tabela de nomes foi para `dsp.h`, não para a GUI. Mesmo motivo da
Etapa 0 — se a GUI tivesse a própria cópia, o teste verificaria uma lógica e o plugin
executaria outra. Assim o teste exercita **exatamente** a função que o plugin chama.

### Verificação

```
./baseline.sh conferir
→ ok  test_escalas
→ IDENTICO — nada mudou.
```

O `test_escalas` (agora rodado pelo `baseline.sh`) cobre:

| Seção | O que prova |
|---|---|
| 1 · Regressão | as **6 escalas antigas** produzem exatamente o mesmo conjunto de notas |
| 2 · Cobertura | as **24 tonalidades** dão 7 notas cada, com os intervalos certos |
| 3 · Enarmonia | `Db`≡`C#`, `Eb`≡`D#`, `Gb`≡`F#`, `Ab`≡`G#`, `Bb`≡`A#` (e menores) |
| 4 · Entrada inválida | `""`, `"H"`, `"xyz"` caem em cromático sem travar |
| 5 · Os 36 combos da GUI | 12 tônicas × 3 modos, pela função que o plugin realmente chama |
| 6 · Índice fora da faixa | tônica −1/12/99 e modo −1/3/99 não quebram |

Os 17 casos de áudio continuam idênticos — como esperado, já que os CLIs recebem a escala como
string e **já aceitavam as 24 tonalidades desde sempre**.

### ⚠️ Ressalva — o plugin não foi compilado

**Esta máquina não tem CMake nem JUCE**, então o VST3 não foi construído. Foram verificados:

- ✅ a lógica de mapeamento (`montarEscala`), pelo teste, na função real;
- ✅ que os CLIs e o núcleo não regrediram (17/17 idênticos);
- ❌ **a compilação do plugin e o comportamento dos ComboBox na GUI.**

**Antes de fechar a etapa, rodar `compilar.bat` + o build do plugin em máquina Windows** e
conferir que os dois combos aparecem e mudam a escala.

### Quebra de compatibilidade, como decidido

Projetos de DAW salvos com a versão anterior **não recuperam a escala**: o parâmetro `escala`
mudou de 7 opções para 3, e a tônica passou a ser um parâmetro novo (`tonica`). Um projeto
antigo em "Sol maior" (índice 3) abrirá como "Menor natural" com tônica C.

Isso foi **decidido e aceito** (Decisão 4 em [historico-e-decisoes.md](historico-e-decisoes.md)):
o plugin não tem base instalada. Registrado aqui para constar do texto do TCC.
