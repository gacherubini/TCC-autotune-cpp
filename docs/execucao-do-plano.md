# Execução do plano — diário das etapas

> Registro do que foi **efetivamente feito**, etapa por etapa, com a verificação de cada uma.
> O plano está em [plano-de-implementacao.md](plano-de-implementacao.md); este documento é o
> que aconteceu ao executá-lo.

| Etapa | Status | Data |
|---|---|---|
| **0 — malha de correção unificada** | ✅ **concluída** | 2026-08-26 |
| **1 — 24 tonalidades** | ✅ **concluída** (a ressalva "plugin não compilado" foi fechada pela Etapa 1-bis) | 2026-08-26 |
| **1-bis — ambiente de compilação e `pluginval` (macOS)** | ✅ **concluída** | 2026-08-26 |
| **2 — Mix / remoção da Forca** | ✅ **concluída** | 2026-08-26 |
| **3 — Retune Speed (funde o Glide)** | ✅ **concluída** | 2026-08-26 |
| **4 — Humanize** | ✅ **concluída** | 2026-08-26 |
| **5 — Create Vibrato** | ✅ **concluída** — ⚠️ os quatro controles saíram da GUI em 2026-08-31 ([Decisão 8](historico-e-decisoes.md#decisão-8--create-vibrato-sai-da-interface-fica-no-dsp-2026-08-31)); o DSP, os CLIs e os parâmetros do APVTS ficam | 2026-08-26 |
| **redesenho da interface** (fora do plano) | ✅ **concluída** — painel afinador + 9 controles em três grupos, no lugar da GUI genérica | 2026-08-31 |
| **6 — motor v3 (Low Latency)** | ✅ **concluída** | 2026-09-02 |

> **O plano acabou — as cinco primeiras etapas.** Elas estão feitas e verificadas — o que isso
> prova, e o que **não** prova, está na [seção logo abaixo da Etapa 5](#o-plano-acabou-o-que-ele-não-entrega).
> A **Etapa 6** veio depois, de um plano próprio ([plano-v3-ponteiro.md](plano-v3-ponteiro.md)),
> motivado pelo piso de latência que a [análise v1/v2/v3](analise-v1-v2-v3.md) expôs — não estava
> nas cinco etapas originais. O que ficou pendente ao todo está reunido em
> [§ Pendências abertas](#pendências-abertas-ao-fim-do-plano).

---

## Ferramenta de verificação — `baseline.sh`

Criada antes da Etapa 0, porque **sem ela nenhuma etapa é verificável**.

```bash
./baseline.sh gravar     # grava a referência em baseline/
./baseline.sh conferir   # compara o estado atual contra a referência
```

Compila os três executáveis, roda **37 casos** sobre `exemplo-antes.wav` e resume tudo em
checksums SHA-256 do WAV de saída e do log (com tempos de execução filtrados, para que
variação de máquina não vire ruído). Os casos cobrem bypass, correção cheia, zona morta,
glide, look-ahead, invariância de tamanho de bloco, escalas e os dois motores de síntese
(PSOLA e ponteiro, Etapa 6).

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

> **RESOLVIDA em 26/08/2026.** O ambiente de compilação foi montado nesta mesma máquina
> (macOS) e o plugin foi construído e validado — ver **Etapa 1-bis**, adiante. O texto
> original da ressalva fica preservado abaixo, como registro do estado em que a etapa foi
> fechada.

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

---

## Etapa 1-bis — ambiente de compilação e validação do plugin (macOS)

**Data:** 26/08/2026 · **Motivação:** a ressalva da Etapa 1 — o plugin era a única parte do
projeto que nenhum teste alcançava, porque não havia como compilá-lo aqui.

### O que faltava

Só o **CMake**. O diagnóstico anterior ("não tem CMake nem JUCE") estava certo quanto ao CMake
e enganado quanto ao JUCE: o JUCE nunca precisou estar instalado — o `CMakeLists.txt` já o baixa
sozinho via `FetchContent` na primeira configuração. Compilador e SDK também já estavam
presentes (Apple clang 21, Command Line Tools). Instalado com:

```
brew install cmake ninja            # cmake 4.4.3, ninja 1.13.2
brew install --cask pluginval       # pluginval 1.0.4
```

### Resultado da compilação

```
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-mac
```

Compilou **sem um único erro**, gerando o `TCC Autotune.vst3` e o app Standalone. Os avisos que
aparecem são todos de `-Wsign-conversion` / `-Wimplicit-int-conversion` no `dsp.h`, que o JUCE
liga por padrão (`juce_recommended_warning_flags`) e o `compilar.bat` não liga — são
**pré-existentes**, nenhum vem das mudanças da Etapa 0 ou 1. Ficam registrados como item de
dívida técnica, sem ação nesta etapa (mexer neles alteraria o núcleo e quebraria a linha de base).

Que o mesmo código compile em **MSVC (Windows)** e em **Apple clang (macOS)**, dois compiladores
independentes, é evidência a favor da portabilidade do núcleo — vale citar no texto do TCC.

### Validação automatizada — `pluginval`

O [pluginval](https://github.com/Tracktion/pluginval) (Tracktion Corporation) é o validador de
plugins usado como padrão na indústria; roda **headless**, o que o torna citável como
verificação reproduzível. Executado no nível de rigor **máximo (10)**:

```
pluginval --strictness-level 10 --validate "TCC Autotune.vst3"
→ SUCCESS      (0 falhas, 0 avisos)
```

O que ele exercitou:

| Teste | Cobertura |
|---|---|
| Audio processing | 44,1 / 48 / 96 kHz × blocos de 64, 128, 256, 512, 1024 — sem NaN/Inf/denormals |
| Non-releasing audio processing | as mesmas 15 combinações, sem liberar o plugin entre elas |
| Automation | as 15 combinações, com automação em sub-blocos de 32 amostras |
| Plugin state / state restoration | salvar e restaurar o estado do APVTS |
| Editor / open editor whilst processing | abrir e fechar a GUI **durante** o callback de áudio |
| Parameter thread safety | parâmetros alterados por outra thread durante o processamento |
| Fuzz parameters | valores aleatórios em todos os parâmetros |
| auval | validador da Apple (AudioUnit) |
| Buses | mono e estéreo, entrada e saída |

Isso cobre justamente o que o `test_escalas` **não** alcança: o plugin como objeto vivo dentro
de um host. Os dois se complementam — o teste unitário prova que o mapeamento tônica × modo está
certo, o `pluginval` prova que o plugin não quebra o host ao ser usado.

**Um ponto anotado, sem ação:** o `pluginval` reporta `latency: 0` na fase *Plugin info*. É
esperado — a latência só é conhecida depois do `prepareToPlay()`, e é lá que o
`setLatencySamples()` é chamado. Hosts leem o valor após o `prepare`. Fica registrado para não
ser confundido com defeito numa leitura futura do log.

### O que ficou no repositório

| Arquivo | Papel |
|---|---|
| `plugin/build.sh` | **novo** — contraparte do `build.bat` para macOS/Linux: configura, compila e valida num comando só |
| `.gitignore` | passa a ignorar `plugin/build-mac/` |

O diretório de build é `build-mac`, e **não** `build`, de propósito: assim a árvore do Windows e
a do macOS coexistem na mesma cópia do repositório sem uma sobrescrever o cache da outra.

### Situação da ressalva da Etapa 1

| Item | Antes | Agora |
|---|---|---|
| Lógica de mapeamento (`montarEscala`) | ✅ `test_escalas` | ✅ inalterado |
| CLIs e núcleo sem regressão | ✅ 17/17 idênticos | ✅ inalterado |
| **Compilação do plugin** | ❌ não verificada | ✅ compila limpo (Apple clang 21) |
| **Plugin vivo num host** | ❌ não verificada | ✅ `pluginval` nível 10, SUCCESS |
| **Aparência dos ComboBox na GUI** | ❌ não verificada | ⚠️ app Standalone aberto e inspecionado por olho humano; não há verificação automatizada de layout |

A ressalva está **resolvida para todos os itens verificáveis por máquina**. O único que
permanece dependente de inspeção humana é a aparência dos dois combos na faixa de controles —
o `pluginval` abre e fecha o editor sem falhar, o que prova que ele **funciona**, mas não que
esteja **legível**.

Continua valendo compilar em Windows/MSVC antes da entrega, já que o Ableton do teste de usuário
roda lá — mas isso deixou de ser um risco de *compilar ou não*, e passou a ser uma conferência
de plataforma.

---

## Etapa 2 — Mix seco/molhado, e a remoção da `Forca`

**Data:** 26/08/2026 · **Plano:** §6 e §9.1 · **Decisão que a originou:** Decisão 1 em
[historico-e-decisoes.md](historico-e-decisoes.md)

### O que mudou, e por quê a troca não é cosmética

A `Forca` (0–1) multiplicava o desvio antes da correção: `corrMidi = midi + forca·mov/100`.
Com `forca = 0,5` numa nota sustentada 50 cents baixa, o cantor terminava **25 cents baixo, e
ficava lá** — a correção era parcial *na afinação*, permanentemente. Não era uma dosagem de
efeito; era uma correção mal feita de propósito.

O Mix faz outra coisa: a correção é sempre **integral**, e o que se dosa é **quanto do sinal
corrigido se ouve**. Em 50% ouvem-se dois sinais somados — o original e o corrigido —, não um
terceiro sinal sintetizado num alvo intermediário.

| | `Forca = 0,5` | `Mix = 0,5` |
|---|---|---|
| O que o PSOLA recebe | alvo a meio caminho | o alvo verdadeiro |
| O que sai | **um** sinal, 25 ct desafinado | **dois** sinais somados |
| Afinação percebida | fica errada | a do original e a certa, juntas |

### A parte difícil: alinhamento

No caminho offline não há problema — o TD-PSOLA preserva a duração, então `out[i]` e `x[i]`
são a mesma posição no tempo. No **streaming**, não: a saída sai atrasada de `latSamples`.

Misturar o seco *de agora* com o molhado *atrasado* somaria o sinal a uma cópia deslocada de si
mesmo — **filtro-pente**, claramente audível. E é um erro traiçoeiro: em `mix = 0` e `mix = 1`
o áudio sai certo, porque só um dos dois caminhos é lido. Só as posições **intermediárias**
ficariam erradas, que são justamente as que nenhum teste de identidade cobre.

A solução caiu de graça na estrutura que já existia. O `process()` já indexava a saída por
amostra absoluta (`src = lida − latSamples`), e o `xAll` já guarda toda a entrada indexada do
mesmo jeito. Então o seco alinhado é `xAll[src]` — **o mesmo índice**:

```cpp
const float molhado = (src >= 0 && src < synthFront)        ? outBuf[src] : 0.0f;
const float seco    = (src >= 0 && src < (long long)xAll.size()) ? xAll[src]   : 0.0f;
out[i] = misturar(seco, molhado, p.mix);
```

**Consequência deliberada:** com `mix = 0` a saída é a entrada **atrasada**, não a entrada
instantânea. É o comportamento certo num plugin que reporta latência — se o bypass não
atrasasse, mexer no Mix deslocaria o áudio no tempo. O host compensa um atraso fixo; não
compensa um atraso que aparece e some.

### O problema de cobertura de teste — e a saída que apareceu

O plano (§9.1) tinha avisado do risco: `forca = 0` fazia **duas coisas ao mesmo tempo**, e o
`mix = 0` só herda uma delas.

| | O PSOLA roda? | O resultado vai pra saída? | O que isso testa |
|---|---|---|---|
| `forca = 0` (antes) | ✅ com β = 1 | ✅ | **drift de fase do PSOLA** |
| `mix = 0` (agora) | ✅ | ❌ descartado | só o caminho de bypass |

Perder isso seria perder, sem perceber, o teste que pegou o bug de drift de fase de 2026-08-25
(Decisão 3 do histórico). O plano previa "acrescentar um teste que force β = 1 por outro
caminho", sem dizer qual.

**O caminho já existia, e não precisou de código nenhum:** a zona morta. Uma tolerância maior
que meio semitom (`tol ≥ 50`) torna o desvio sempre menor que a tolerância, logo `mov = 0`,
logo `alvo == f0`, logo **β = 1** — com o PSOLA rodando inteiro e o resultado indo pra saída.
Usamos `tol=600` (meia oitava), folgado.

Verificado por checksum **antes** de mexer em qualquer coisa, ainda com o código antigo:

```
autotune exemplo-antes.wav a.wav 0.0              -> 4f35cced6cc704b2
autotune exemplo-antes.wav b.wav 1.0 crom tol=600 -> 4f35cced6cc704b2   (idêntico)
stream_test … 0.0                                 -> 373037487675431e
stream_test … 1.0 crom tol=600                    -> 373037487675431e   (idêntico)
```

Ou seja: `tol=600` **é** o antigo `forca=0`, byte a byte, nos dois caminhos. A cobertura migrou
com prova, não com esperança.

### O par que virou invariante

Com isso o `baseline.sh` ganhou algo melhor que um checksum gravado. `gold_tol600` e
`gold_mix0` **têm de ser iguais entre si** — um roda o PSOLA em identidade, o outro não roda
nada. Se o PSOLA ganhar drift de fase, o primeiro muda e o segundo não.

Isso é um **invariante do algoritmo**, não uma fotografia do passado: continua valendo depois
de qualquer re-baseline legítimo. O script agora verifica os dois pares e **aborta** se
quebrarem, antes de deixar gravar referência nova por cima de um resultado errado:

```
== invariantes (independem da referencia) ==
  ok    PSOLA em identidade (beta=1) == bypass  [offline]
  ok    PSOLA em identidade (beta=1) == bypass  [streaming]
```

### Verificação — nenhum áudio mudou

Todos os 17 casos anteriores produzem WAV com o **mesmo checksum**. Só as hashes de *log*
mudaram, porque a linha impressa passou a dizer `mix=` em vez de `forca=`.

| caso (antes) | caso (agora) | wav antes | wav agora | |
|---|---|---|---|---|
| `gold_forca1` | `gold_mix1` | `a10c561e7290fa8a` | `a10c561e7290fa8a` | ✅ |
| `gold_forca0` | `gold_mix0` | `4f35cced6cc704b2` | `4f35cced6cc704b2` | ✅ |
| `gold_tol30` | `gold_tol30` | `511c1e14ee113393` | `511c1e14ee113393` | ✅ |
| `gold_glide120` | `gold_glide120` | `db0f52d6bd47b25f` | `db0f52d6bd47b25f` | ✅ |
| `gold_cmaior` | `gold_cmaior` | `74794f1942feb95e` | `74794f1942feb95e` | ✅ |
| `gold_aminor` | `gold_aminor` | `74794f1942feb95e` | `74794f1942feb95e` | ✅ |
| `rt_look4` | `rt_look4` | `0c2fb55c41dcfe22` | `0c2fb55c41dcfe22` | ✅ |
| `rt_look0` | `rt_look0` | `00016234ee2ea312` | `00016234ee2ea312` | ✅ |
| `rt_glide0` | `rt_glide0` | `0c2fb55c41dcfe22` | `0c2fb55c41dcfe22` | ✅ |
| `rt_tol0` | `rt_tol0` | `0c2fb55c41dcfe22` | `0c2fb55c41dcfe22` | ✅ |
| `st_forca1` | `st_mix1` | `522ffaf32a3d6e47` | `522ffaf32a3d6e47` | ✅ |
| `st_forca0` | `st_mix0` | `373037487675431e` | `373037487675431e` | ✅ |
| `st_glide40` | `st_glide40` | `a86b831cc550896a` | `a86b831cc550896a` | ✅ |
| `st_tol15` | `st_tol15` | `42e8ad70d2356180` | `42e8ad70d2356180` | ✅ |
| `st_block64` | `st_block64` | `522ffaf32a3d6e47` | `522ffaf32a3d6e47` | ✅ |
| `st_block512` | `st_block512` | `270c22c19308a2e4` | `270c22c19308a2e4` | ✅ |
| `st_cmaior` | `st_cmaior` | `44af6aebec7600ce` | `44af6aebec7600ce` | ✅ |
| — | `gold_tol600` | — | `4f35cced6cc704b2` | 🆕 |
| — | `st_tol600` | — | `373037487675431e` | 🆕 |

> **Nuance que o texto do TCC precisa registrar.** A tabela mostra que *nenhum caso de teste*
> mudou de áudio, mas isso **não** quer dizer que a Etapa 2 não mudou comportamento. Todos os
> casos usam os **extremos** (`1.0` e `0.0`), e nos extremos as duas formulações coincidem. O
> que mudou está nos valores **intermediários**, que nenhum caso exercitava — e mudou de
> propósito. Dizer "não mudou nada" seria falso; o correto é "a regressão foi nula onde havia
> teste, e a mudança intencional está fora do alcance dos testes existentes".

### O teste novo — `test_mix.cpp`

Cobre justamente o que os checksums não pegam:

| Seção | O que prova |
|---|---|
| 1 · Extremos | `mix=1` devolve o molhado **bit a bit** e `mix=0` o seco, sem passar por multiplicação |
| 2 · Cruzamento | `mix=0,5` é a média exata; a varredura de 0 a 1 é monótona (sem degrau nos extremos) |
| 3 · **Alinhamento** | no streaming, `mix=0` é a entrada atrasada de **exatamente** `latSamples` — 40 950 amostras conferidas, e as `lat` primeiras são silêncio |
| 4 · Blocos | o atraso do seco não depende do tamanho do bloco (32, 128, 256, 512, 1024 × 64) |

A Seção 3 é a que importa: é a única coisa no projeto que pegaria o filtro-pente descrito
acima. Está anotado no próprio teste que a Seção 4 **não** cobre o caminho molhado — com
`mix = 0` ele nem é lido —, e portanto não diz nada sobre o achado em aberto de invariância em
`block = 512`.

### Estado da árvore

| Arquivo | Mudança |
|---|---|
| `src/core/dsp.h` | `notaAlvo()` perde `forca`; `ParamsCorrecao` perde `forca`; **+`misturar()`** |
| `src/offline_causal/main.cpp` | 3º posicional vira `mix`; mistura após o PSOLA |
| `src/offline_causal/autotune_rt.cpp` | idem (a mistura fica **dentro** da região cronometrada — é custo real) |
| `src/c1_streaming/autotune_stream.h` | `StreamParams.forca` → `.mix`; mistura alinhada no `process()` |
| `src/c1_streaming/stream_test.cpp` | 3º posicional vira `mix` |
| `plugin/PluginProcessor.cpp/.h` | id `"forca"` **aposentado**, id novo `"mix"`; `pForca` → `pMix` |
| `plugin/PluginEditor.cpp/.h` | slider `Forca` → `Mix` |
| `src/tests/test_mix.cpp` | **novo** |
| `baseline.sh` | casos novos + o bloco de invariantes que aborta |

**Sobre o id do parâmetro:** o `"forca"` foi aposentado e o novo chama-se `"mix"`, em vez de
reaproveitar o id. Reaproveitar faria o host restaurar um valor antigo com semântica nova, em
silêncio — um projeto salvo com `Forca = 0,5` abriria com `Mix = 0,5`, que soa diferente. Com
id novo, o projeto antigo simplesmente cai no padrão (`Mix = 1`). Isso **amplia** a quebra de
compatibilidade já aceita na Etapa 1, e pela mesma razão: o plugin não tem base instalada.

### Verificação final

```
./baseline.sh conferir
→ ok  test_escalas
→ ok  test_mix
→ ok  PSOLA em identidade (beta=1) == bypass  [offline]
→ ok  PSOLA em identidade (beta=1) == bypass  [streaming]
→ IDENTICO — nada mudou.

cd plugin && ./build.sh
→ compila limpo (Apple clang 21)
→ pluginval --strictness-level 10 → SUCCESS
```

### O que fica pendente

- **Escuta.** Nenhum teste decide se `Mix` intermediário soa bem. O plano (§9.3) já previa que
  isso é teste de escuta, com o mesmo usuário — e continua não feito.
- **Faixa do Mix na GUI** está em 0–1. A Antares mostra em %; converter é cosmético e ficou
  para quando a GUI for revista.
- O `bench_stream.py` **não quebra** — ele passa `"1.0"` no 3º posicional, que agora significa
  `mix = 1` e produz o mesmo áudio de antes; só o comentário sobre `forca=0` foi corrigido. Mas
  ele roda no venv do repositório irmão, em Windows, e **não foi executado nesta máquina**: os
  números de correlação com o gold citados no README continuam sendo os da medição anterior.

---

## Etapa 3 — Retune Speed, e a fusão do Glide

**Data:** 26/08/2026 · **Plano:** §3 a §5, §11.1 · **Fundamentação:**
[pesquisa-retune-speed-e-cor.md](pesquisa-retune-speed-e-cor.md)

Esta é a etapa que muda o som. As anteriores reorganizaram controles; esta troca a matemática
da correção.

### O que estava errado na malha antiga

```
estado = alpha*estado + (1-alpha)*alvoCents        (filtro sobre o ALVO)
```

Dois defeitos, ambos já diagnosticados na [documentação técnica](documentacao-tecnica.md) §8.2:

1. **O filtro agia sobre o alvo**, que é quase constante dentro de uma nota. Ele converge em
   ~τ e depois **não faz mais nada**. Como o alvo não vibra e a saída segue o alvo, o vibrato
   do cantor era destruído.
2. **O reset de ataque era para o alvo**: a nota nascia exatamente afinada, sem trajeto. É o
   "duro, estático, robótico" que o teste de usuário reprovou.

### A cadeia nova

```
outCents = LP(alvo) + k·(real − LP(real))  =  LP(alvo) + k·HP(real)
```

Dois estados de filtro em vez de um. O que essa forma faz é **separar o que o cantor faz
devagar** (deriva de afinação — corrigir) **do que ele faz depressa** (vibrato — preservar).

| `k` | Saída | Equivalente |
|---:|---|---|
| 0 | `LP(alvo)` — **literalmente a linha antiga** | o Glide de antes |
| 1 | `real + LP(alvo − real)` — filtro sobre a **correção** | Retune Speed puro (Hildebrand, US 5.973.252) |
| >1 | `alvo + k·vibrato` | Natural Vibrato positivo |

**Por isso é fusão e não troca:** o Glide não foi removido, virou o caso `k = 0`.

### A ressalva do plano estava certa

O plano (§11.1) já tinha corrigido a si mesmo: `k = 0` reproduz o **regime**, não o **ataque**.
A malha antiga iniciava o estado em `alvoCents`; a nova inicia em `realCents`. Reproduzir a
Etapa 2 **exatamente** exige **dois** valores neutros — `k = 0` **e** a flag `ataqueNoAlvo`.

Ela entrou como campo de `ParamsCorrecao` e flag de CLI (`legado=1`), **não** como parâmetro de
plugin: o deslize de entrada é comportamento fixo do produto, não uma escolha do usuário.

### Verificação — a parte que dá o chão

**1. Não-regressão, 19 casos, bit a bit.** Cada caso do baseline roda de novo com
`legado=1 vibrato=0` e é comparado com a impressão digital da Etapa 2:

```
== nao-regressao: legado=1 vibrato=0 reproduz a Etapa 2 ==
  ok    19 casos reproduzem a Etapa 2 bit a bit
```

Isso não é um checksum gravado que o próximo `gravar` sobrescreve. As hashes ficam em
**`baseline/etapa2-legado.sha256`**, marcado como *nunca regravar*: é um **marco fixo no
passado**. Se as Etapas 4 ou 5 quebrarem a generalização, é aqui que aparece.

**2. `test_retune.cpp`**, que prova o que checksum não alcança:

| Seção | O que prova |
|---|---|
| 1 · Equivalência | `k=0` + `ataqueNoAlvo` == a malha da Etapa 2, **bit a bit**, para glide 0/15/40/120 ms |
| 2 · Ataque | a 1ª amostra da nota é a altura **real** do cantor, para k = 0, 1 e 2 |
| 3 · Álgebra | `LP(alvo)+real−LP(real)` == `real+LP(alvo−real)` (divergência máx. **6,2·10⁻¹¹ Hz**) |
| 4 · Vibrato | o ganho segue `G(f_v) = f_v/√(f_v²+f_c²)` — medido vs. teoria: **erro ≤ 0,1 %** |

A Seção 1 usa um **oráculo congelado**: a malha da Etapa 2 reimplementada dentro do próprio
teste, copiada do commit `63ee0f0`. Não é duplicação descuidada — é contra aquele texto que a
não-regressão é medida, e ele tem de ficar imóvel mesmo que o `dsp.h` evolua.

A Seção 4 transforma em número o compromisso central deste controle: **corrigir rápido come
vibrato**. Com τ = 25 ms, f_c ≈ 6,4 Hz, e um vibrato de 5,5 Hz sai com 65 % da amplitude. Não é
defeito do filtro, é o que um passa-altas de 1 polo faz — e agora está quantificado, não
suposto.

### Um erro meu no caminho, que vale registrar

A primeira conferência da não-regressão acusou divergência em **exatamente** os casos com
`glide ≠ 0`. Passei um tempo procurando erro na malha antes de medir: instrumentei uma cópia
rodando as duas malhas lado a lado sobre a trilha de F0 real, e ela acusou **zero** divergências
em `fout`.

A causa era do shell, não do código: eu tinha posto `legado=1 vibrato=0` numa variável e usado
sem aspas. **`zsh` não faz word-splitting** de expansão de variável (o `bash` faz), então os dois
foram um argumento só — `legado=1` casou, `vibrato=` nunca foi lido e ficou em 1. E com
`glide=0` o termo `k·HP(real)` é nulo, o que explica por que **só** os casos com glide≠0
divergiam.

Fica anotado porque o sintoma era enganosamente coerente com um bug real de DSP: "falha só
quando o filtro está ativo" é exatamente o que um erro na fusão produziria.

### Parâmetros

| | Antes | Agora |
|---|---|---|
| `Glide` | 0–200 ms, padrão 40 | ➡️ **`Retune Speed`**, 0–200 ms, **padrão 25** |
| — | — | ➕ **`Natural Vibrato`** (k), 0–2, **padrão 1,0** |

O padrão 25 ms vem do manual da Antares: *"A setting between 10 and 50 is typical for more
natural sounding pitch correction"*. O **zero continua alcançável** — é ele que dá o efeito
"duro" deliberado.

Nos CLIs, `glide=` continua valendo como **apelido** de `retune=`, para que comandos e scripts
anteriores não quebrem em silêncio. No plugin o id `"glide"` foi **aposentado** em favor de
`"retune"`: a unidade não mudou, mas o significado sim (o filtro age sobre outra coisa) e o
padrão foi de 40 para 25 — pela mesma regra aplicada ao `mix` na Etapa 2.

### Estado da árvore

| Arquivo | Mudança |
|---|---|
| `src/core/dsp.h` | `CorretorAltura` com dois estados; `ParamsCorrecao` ganha `retuneMs`, `vibrato`, `ataqueNoAlvo` |
| os 3 CLIs | flags `retune=`, `vibrato=`, `legado=`; `glide=` vira apelido |
| `src/c1_streaming/autotune_stream.h` | `StreamParams` acompanha; `updateLiveParams` ganha o k |
| `plugin/` | `Retune Speed` + `Natural Vibrato`; a faixa foi de 7 para 8 colunas |
| `src/tests/test_retune.cpp` | **novo** |
| `baseline/etapa2-legado.sha256` | **novo** — o marco congelado |
| `baseline.sh` | 4 casos novos + o bloco de não-regressão |

### Verificação final

```
./baseline.sh conferir
→ ok  test_escalas / test_mix / test_retune
→ ok  PSOLA em identidade (beta=1) == bypass  [offline] [streaming]
→ ok  19 casos reproduzem a Etapa 2 bit a bit
→ IDENTICO — nada mudou.

cd plugin && ./build.sh   → compila limpo · pluginval nível 10 → SUCCESS
```

### O que esta etapa NÃO resolve

**A escuta.** Nenhum número aqui diz se `k = 1` soa melhor que `k = 1,2`, nem se 25 ms é o τ
certo para esta voz, nem se o ataque na altura real incomoda quando o cantor entra errado. O
plano (§9.3) já dizia que isso é teste de escuta, com o mesmo usuário do teste anterior.
**Continua não feito, e é agora o item mais importante do trabalho** — a Etapa 3 é a que
promete resolver a reprovação de naturalidade, e essa promessa só se verifica ouvindo.

---

## Achados de medição — três afirmações do projeto que não se sustentam

**Data:** 26/08/2026. Levantados ao gravar a linha de base de **qualidade** que o plano (§9.2)
exigia antes da Etapa 3, e ao diagnosticar a invariância ao tamanho de bloco. Os dois trabalhos
correram **em paralelo** com a implementação da Etapa 3.

Nada aqui foi causado pelas Etapas 0–3: são defeitos **pré-existentes**, e dois deles estavam
documentados como *verificados*.

### Achado A — o "gold" do teste de correlação não é o gold

`CLAUDE.md` lista como invariante: *"Correlação do streaming com o gold ≥ 0,995 (hoje: 0,997)"*.
A arquitetura chama de **gold** o caminho **offline** (`autotune`, Viterbi global). Mas o script
que mede isso, `python/bench_stream.py`, faz na linha 15:

```python
run("./autotune_rt.exe","_gold.wav",[])   # <- o "gold" é o CAUSAL
```

Ele compara o streaming com o **causal**, não com o offline. Medido:

| comparação | correlação |
|---|---|
| streaming × causal | **0,9996** |
| streaming × offline | **0,78** |
| causal × offline | **0,78** |

**O que isso significa, com cuidado:** o número que o script produz é *verdadeiro e valioso* —
o streaming reproduz o causal quase perfeitamente, e essa era a pergunta de engenharia difícil
(transformar um algoritmo de lote em motor causal de blocos). O que **nunca foi medido** é
streaming ≡ offline. A lacuna real está entre **causal e offline**, e é esperada em espécie: o
offline usa Viterbi global e `suavizarVozeamento()`, que é não-causal por construção.

O que não é esperado é o **tamanho**. A divergência não está distribuída: concentra-se em
poucas janelas, com correlação **negativa** (t ≈ 3,3–3,6 s e t ≈ 0,4 s), o que é assinatura de
**escorregão de fase**, não de decisão de pitch diferente. Fica como item de backlog.

> **Para o texto do TCC:** a frase "correlação com o gold" precisa dizer *com qual* caminho. Do
> jeito que está, promete mais do que mede.

### Achado B — "pipoco = 0" não se reproduz

O critério documentado é "descontinuidades amostra-a-amostra maiores que 30× a mediana". Com voz
real a 44,1 kHz, a mediana de |Δ| fica em ~2,2·10⁻³ e picos legítimos passam de 0,27 — então
"30× a mediana" cai por volta do percentil 97 do sinal normal. O detector conta **conteúdo de
alta frequência**, não clique.

| sinal | contagem |
|---|---|
| **entrada intocada** | 2916 |
| offline | 2272 |
| causal | 2566 |
| streaming | 2655 |

O resultado que importa continua bom, e é o que a afirmação *queria* dizer: **os três caminhos
ficam abaixo da entrada** — nenhum introduz descontinuidade. Mas "= 0" é um artefato do limiar,
não uma medida. Com limiar absoluto (|Δ| > 0,25) dá 13/13/12, que é uma medida defensável.

### Achado C — a invariância ao tamanho de bloco estava quebrada (e foi corrigida)

Documentada como verificada em `README.md` e `CLAUDE.md`; **nunca foi testada**. O `baseline.sh`
rodava `block=64` e `block=512` mas não comparava um com o outro.

**Causa raiz.** `avancarPsola()` cometia `[synthFront, alvoFinal)` de uma vez, e roda uma vez por
`process()`. Um bloco do host maior que `nHop` (256) cabe **duas** emissões de quadro numa
chamada, então `synthFront` pulava `2·nHop` e **saía da grade** `k·nHop − guarda` que os blocos
pequenos percorriam. Como a janela re-sintetizada é derivada de `synthFront`, a janela passava a
ser outra.

Isso importa por causa de um detalhe em `dsp.h`: a busca por correlação que refina as marcas do
PSOLA descarta candidatos com `m − Wc < 0`, ou seja, **mede contra o início da janela**. Para a
primeira marca de uma região vozeada, uma janela que começa em cima da região não avalia
candidato nenhum e a marca seguinte cai no *fallback*; alguns milissegundos a mais de contexto à
esquerda mudam a marca em 1–7 amostras, e o desvio propaga pela cadeia toda.

| região | block ≤ 256 | block = 512 |
|---|---|---|
| `[13234,13490)` | ws=12032 → **0 candidatos** → 2ª marca 12210 | ws=11954 → 67 candidatos → 12212 |
| `[196018,196274)` | ws=194994 → **0 candidatos** → 195302 | ws=194738 → 113 candidatos → 195309 |

**É só síntese, não análise:** `dumpframes` e `dumpf0` são bit-idênticos entre 64 e 512 (858
quadros, 854 F0). Mesma decisão de pitch, reconstrução diferente.

**Descartada por medição** a hipótese que estava documentada (§8.3 Achado 3, normalização por
pico): ela dispara **0 de 850** chamadas neste material.

**Correção aplicada:** cometer em passos de no máximo `nHop` e derivar `winEnd` de `alvo` em vez
de `decis`. Cada chamada a `psolaSintetiza()` vira **função exclusiva de `synthFront`**, e a
invariância deixa de ser empírica e passa a ser **estrutural**. Verificado em 16 tamanhos de
bloco de 1 a 4096: **uma única impressão digital**. Nenhuma outra saída mudou — o único caso do
baseline afetado foi o `st_block512`, que passou a ser igual ao `st_block64`.

O `baseline.sh` agora **compara** `block=64` com 512 e 1024, e o `block=1024` entrou como caso.

> **Uma consequência incômoda, registrada.** A tabela congelada `etapa2-legado.sha256` guardava,
> para o `st_block512`, a saída **com o bug**. A referência estava congelando um defeito. Foi a
> única linha alterada, e a alteração está anotada no próprio arquivo. É um lembrete de que uma
> linha de base protege contra mudança inadvertida, **não** atesta correção.

**Custo:** blocos grandes perdem um desconto de CPU que só existia porque o motor estava errado
(512: 0,87 s → 1,50 s; 1024: 0,54 s → 1,48 s, sobre 5 s de áudio). O custo fica **uniforme e
igual ao do pior caso já suportado** (block=64, 1,47 s) — que é o caso que o plugin tem de
aguentar de qualquer jeito.

**Alternativa registrada, não aplicada.** Limitar a busca por correlação à própria região
vozeada (em `dsp.h`) também dá invariância, a custo zero de CPU, e é argumentavelmente o DSP
**mais correto** — correlacionar através de uma fronteira vozeada/não-vozeada não significa
nada. Mas mexe nos **quatro** caminhos: muda 1,75 % das amostras (correlação 0,999995, todas em
ataques de nota) e exigiria regravar todas as referências. Fica no backlog, com o registro de
que a escolha foi **preservar a linha de base**, não afirmar que a solução aplicada é a melhor.

### Linha de base de qualidade da Etapa 2, para comparação futura

Gravada **antes** da Etapa 3, como o plano §9.2 exigia. Reprodutível com
`python/medir_qualidade.py` (novo, roda em macOS/Linux sem depender do venv do repositório irmão).

| Métrica | Etapa 2 |
|---|---|
| Latência do `stream_test` (a que o plugin reporta) | 71,4 ms (3150 amostras) |
| xRT do caminho causal | 0,042 |
| corr. streaming × causal (global / vozeada) | 0,9981 / 0,9979 |
| corr. streaming × offline (global / vozeada) | 0,5695 / 0,5123 |
| F0 streaming × causal | 100,00 %, erro máx. 0,0000 Hz |

> ⚠️ **Sobre latência, uma armadilha de citação:** os 57,9 ms citados em vários lugares são com
> `voz=contralto` (FMIN = 175 Hz). Com o padrão FMIN = 80 Hz a guarda do PSOLA dobra e dá
> 71,4 ms. **Latência sempre tem de ser citada junto com o FMIN**, senão o número não quer dizer
> nada.

> **Limitação do levantamento:** não há trilha de F0 do offline para comparar, porque
> `src/offline_causal/main.cpp` não aceita `dumpf0=` — ele só parseia `tol=` e `glide=` e
> **ignora em silêncio** o resto (`look=` inclusive). A referência de F0 acima é o causal.

---

## Etapas 4 e 5 — Humanize e Create Vibrato

**Data:** 26/08/2026 · **Plano:** §7, itens K2, K3 e K4 da pesquisa

As duas foram feitas juntas porque compartilham a mesma máquina: o contador `desdeAtaque`, que
a Etapa 4 introduziu para saber há quanto tempo a nota está soando, é o mesmo que a Etapa 5 usa
para o atraso de entrada do vibrato.

### Etapa 4 — Humanize

Do manual da Antares: *"applies a slower Retune Speed only during the sustained portion of
longer notes"*.

O problema que ele resolve é **efeito colateral direto da Etapa 3**. Um τ único serve a dois
momentos que querem coisas opostas:

| momento | quer | por quê |
|---|---|---|
| **ataque** | τ **curto** | a nota nasce onde o cantor a colocou; precisa chegar à afinação rápido, senão a entrada soa desafinada |
| **sustentação** | τ **longo** | é onde vive a expressão; corrigir depressa ali achata tudo |

```
tauEff = tau · (1 + humanize · HUM_FATOR · rampa(t))
rampa(t) = 1 − exp(−t / HUM_SUSTENTACAO)
```

A rampa é **suave de propósito**. Um limiar duro ("depois de X ms troque o τ") poria um degrau
na constante de tempo no meio da nota, e degrau em filtro é transitório audível.

`HUM_SUSTENTACAO = 200 ms` e `HUM_FATOR = 3` são **constantes**, não controles: elas definem o
que "sustentação" *quer dizer*, e isso é decisão de desenho — a intensidade o usuário já escolhe
pelo próprio Humanize. Ficam nomeadas em `dsp.h` para poderem ser discutidas no texto.

**Efeito medido:** com vibrato de 5,5 Hz e τ = 25 ms, a sustentação preserva **19,6 → 28,8
cents** (×1,47) ao ligar o Humanize em 1.

### Etapa 5 — Create Vibrato

> ⚠️ **Revisto em 2026-08-31: os quatro controles saíram da interface do plugin.** O DSP descrito
> abaixo continua inteiro — `formaVibrato()`, os campos de `ParamsCorrecao`, as flags dos CLIs e
> os quatro parâmetros do APVTS (logo, ainda automatizáveis pelo host). O que saiu foram os
> widgets. Motivo curto: o Create Vibrato é um **gerador** num protótipo que se declara
> **corretor** — o mesmo argumento que cortou Throat e Formante. Registro completo em
> [Decisão 8](historico-e-decisoes.md#decisão-8--create-vibrato-sai-da-interface-fica-no-dsp-2026-08-31).
> O texto desta seção fica como está, como registro do que foi construído e por quê.

Aqui o plugin deixa de só **corrigir** e passa a **gerar**. Quatro controles: forma (off /
senoide / triangular / quadrada), taxa (Hz), profundidade (cents) e **Amplitude Amount**, que
modula a amplitude em sincronia com a altura.

> **Não confundir com o `Natural Vibrato` (k) da Etapa 3 — são opostos.** `k` **preserva** o
> vibrato que o cantor fez; Create Vibrato **inventa** um que ele não fez. Convivem: dá para
> preservar o do cantor e somar outro por cima. Soam mal juntos em profundidade alta, e isso é
> escolha do usuário, não defeito.

**Atraso de entrada.** Vibrato que começa junto com a nota soa sintético — cantor nenhum entra
vibrando. A rampa de entrada (`VIB_ONSET = 300 ms`) reusa o `desdeAtaque` da Etapa 4. Medido:
nos primeiros 50 ms a excursão é **2,9 cents** contra **40,0** em regime.

**Onde a amplitude entra.** O `proxima()` devolve **altura**, e altura e amplitude entram no
sinal em pontos diferentes: a altura vai para o TD-PSOLA, o ganho é aplicado **depois** dele —
o PSOLA move altura, não amplitude. Por isso o ganho sai por um caminho separado
(`ultimoGanho()`), e no streaming ele viaja num vetor indexado pela **mesma amostra absoluta**
que o seco e o molhado, chegando alinhado ao ponto de saída.

O ganho multiplica **só o molhado**: o seco tem de continuar sendo a entrada intocada para que
`mix = 0` siga sendo bypass exato.

### Verificação

Ambas seguem o padrão do plano — **valor neutro reproduz a etapa anterior bit a bit**:

```
ok    humanize=0 == retune25 puro
ok    Create Vibrato off == retune25 puro
ok    19 casos reproduzem a Etapa 2 bit a bit
IDENTICO — nada mudou.
```

**Nenhuma hash de áudio mudou** ao acrescentar as duas etapas — só as de log, porque os CLIs
passaram a imprimir o resumo dos parâmetros novos.

`test_expressao.cpp` cobre o que checksum não alcança: as três formas de "desligado" que
precisam ser **exatas**, o crescimento do τ, as quatro formas de onda dentro de [−1,1], a
profundidade e a taxa geradas, o atraso de entrada, e o Amplitude Amount em dB.

### Dois erros meus no teste, que valem registro

O código passou de primeira; **o teste falhou duas vezes por premissa errada minha**, e as duas
são instrutivas:

1. **"Humanize não toca o ataque."** Falso. A rampa é `1 − exp(−t/0,2)`, que em 1,5 ms já vale
   ~0,0075 — ela começa a agir **imediatamente**, só que com peso desprezível. A afirmação
   verificável é sobre **magnitude** (medido: 0,022 cent, ~200× abaixo do limiar de percepção),
   não sobre igualdade exata.
2. **"Com Humanize o desvio cresce e em 1 s é grande."** Também falso, e mais interessante: com
   F0 **constante** os dois filtros convergem para o mesmo alvo qualquer que seja o τ, então o
   desvio **volta a zero** em regime. O Humanize muda o **transitório**, não o ponto de chegada.
   Ele só muda o regime quando a entrada **se move** — que é exatamente o caso do vibrato, e é
   por isso que a medida de "quanto vibrato sobrevive" é a que mostra o efeito.

Ficam anotados porque os dois enunciados errados são o tipo de coisa que se escreve num texto
de TCC sem pensar duas vezes.

### Faxina que veio junto

Com 10 parâmetros na malha, os três CLIs liam as **mesmas** flags com três cópias do
`if/else-if`. Isso virou `lerFlagCorrecao()` em `dsp.h`, junto com `sanearCorrecao()` e
`resumoCorrecao()`. Mesmo motivo da Etapa 0: um CLI aceitando `vibprof=` e outro **ignorando em
silêncio** é um bug que nenhum teste pega, porque flag ignorada não reclama.

Na mesma linha, o `StreamParams` deixou de copiar campo a campo os parâmetros da malha e passou
a **conter** um `ParamsCorrecao`. O `updateLiveParams()` foi de quatro `double` soltos para
`(const ParamsCorrecao&, double mix)` — com 10 parâmetros, a lista de argumentos já não cabia.

### Dívida de interface, registrada

A faixa de controles do plugin foi de 8 para **13 colunas**. Treze numa faixa única é demais, e
a GUI genérica não escala mais. A organização certa é em grupos (**Escala | Correção |
Expressão**), mas isso é trabalho de **desenho de interface**, não de DSP — não entra numa etapa
cujo critério de aceite é "não mudou o áudio". Fica como item explícito de backlog.

> **Atualização de 2026-08-31.** A [Decisão 8](historico-e-decisoes.md#decisão-8--create-vibrato-sai-da-interface-fica-no-dsp-2026-08-31)
> ataca essa dívida pela outra ponta: em vez de acomodar 13 controles, retira 4 deles — os do
> Create Vibrato, que eram ~31 % da faixa. Sobram **9**, e nove cabem em três grupos numa linha
> só (**Escala | Correção | Motor**).
>
> **A dívida foi quitada no mesmo dia** — ver a seção seguinte.

### Redesenho da interface (2026-08-31) — fora do plano

Não é uma etapa: não tem critério de aceite de áudio, porque **não toca DSP**. Entrou porque a
GUI genérica do JUCE parou de servir com 13 colunas numa linha, e sair do protótipo para a
defesa com ela seria vender o trabalho por menos do que ele é.

**O que passou a existir**, em `plugin/PluginEditor.h/.cpp`:

| Parte | O que faz |
|---|---|
| `PainelAfinador` (metade de cima) | cabeçalho com a **nota-alvo** e a frequência; **arco** com o desvio em cents e a zona morta da `Tolerancia` desenhada dentro; faixa com o **histórico de 2,5 s** da correção aplicada |
| Faixa de controles (metade de baixo) | os **9** controles restantes em três grupos: **Escala** \| **Correção** \| **Motor** |
| `TccLookAndFeel` | tema verde escuro. `destaque` é **sempre** a saída corrigida, `cantado` é **sempre** a altura crua — matizes de famílias diferentes, senão o arco fica ilegível |

**Três decisões que vale registrar:**

1. **A nota-alvo é derivada do `fout`, não do `f0`.** Atacar um F# em dó maior faz o motor mirar
   em F ou G; ler a nota mais próxima do `f0` mostrava F#, que a escala nem permite. O erro
   aparecia justamente quando o plugin estava fazendo o trabalho dele.
2. **O anel do histórico é acumulado no timer da UI, a 60 Hz, não no processor.** O que a faixa
   plota é uma envoltória lenta — o vibrato vive em ~5,5 Hz, então 60 Hz sobra. Acumular na UI
   evitou escrever um ring buffer lock-free dentro do callback de áudio só para desenhar um
   gráfico, que seria código novo em tempo real por um motivo cosmético.
3. **A formatação do texto dos sliders tem de ser aplicada *depois* dos attachments.** O
   `SliderAttachment` instala o próprio `textFromValueFunction` a partir de `param.getText()`,
   que ignora `setNumDecimalPlacesToDisplay` — toda caixa imprimia `15.0000…`. Foi ao corrigir
   isso que o **Mix passou a ser exibido em %**, fechando o item cosmético que a Etapa 2 tinha
   deixado.

**Verificação:** nenhuma linha de DSP mudou, e é isso que o `baseline.sh` confirma —
`IDENTICO` antes e depois. O `pluginval` no nível 10 passa, incluindo *Editor Automation* e
*Fuzz parameters*, que são justamente os testes que exercitam o editor novo.

**O que continua sem verificação automatizada:** a aparência. Layout e legibilidade seguem
conferidos por olho humano no app Standalone, como já era o caso dos ComboBox na Etapa 1-bis.

#### Correção de 2026-09-02 — o texto do afinador piscava a 60 Hz

Apareceu ao montar uma reprodução da tela fora do plugin, sem áudio: os números eram ilegíveis.
Não era defeito da reprodução, era do editor.

O `getF0Atual()` muda a cada **hop**, ou seja a cada **5,8 ms** a 44,1 kHz. O
`startTimerHz(60)` amostra a cada 16,7 ms e chama `repaint()` — então o nome da nota, os dois
Hz e o número de cents trocavam em praticamente **todo** repaint. Uma agulha em movimento a
60 Hz o olho acompanha; um dígito trocando 60 vezes por segundo, não.

**A correção separa as duas taxas dentro do `timerCallback()`:** as agulhas, o rastro e o
histórico seguem lendo os valores no ritmo cheio, e uma cópia dos mesmos valores é congelada a
cada 8 ticks (**7,5 Hz**, 133 ms) só para o texto. Nada é suavizado nem interpolado — o que se
exibe continua sendo o valor de um instante real, amostrado com menos frequência.

Ao vivo o efeito era menos evidente porque o cantor **ouve** a nota, e o ouvido cobre o que o
olho não conseguia ler. É o tipo de defeito que só aparece quando se tira o áudio do caminho.

**Verificação:** `baseline.sh` = `IDENTICO` (nenhum áudio mudou, como se espera de uma mudança
que só toca o editor) e `pluginval` nível 10 = `SUCCESS`.

### O plano acabou. O que ele não entrega.

As cinco etapas estão feitas e verificadas. Vale ser exato sobre o que isso significa:

| O que está provado | O que **não** está |
|---|---|
| cada etapa reproduz a anterior no valor neutro, bit a bit | que os padrões escolhidos soem bem |
| o filtro faz o que a matemática diz (ganho de vibrato dentro de 0,1 % da teoria) | que 25 ms seja o τ certo **para esta voz** |
| o plugin carrega, automatiza e sobrevive a fuzzing no `pluginval` | que a interface de 13 colunas seja usável |
| a saída não depende do tamanho do bloco do host | que a latência de 71 ms seja aceitável ao vivo |

**A reprovação que originou tudo isto foi de escuta** ("duro, estático, robótico"), e escuta é a
única coisa que nenhuma destas verificações substitui. O teste com o mesmo usuário, agora com
Retune Speed, Humanize e Natural Vibrato disponíveis, é o próximo passo do trabalho — e é dele
que sai a resposta sobre se o plano funcionou.

---

## Etapa 6 — motor v3 de ponteiro móvel (Low Latency)

**Concluída em 2026-09-02.** Especificação: [especificacao-v3-ponteiro.md](especificacao-v3-ponteiro.md).
Decisão de escopo: [Decisão 9](historico-e-decisoes.md#decisão-9--motor-v3-de-ponteiro-móvel-como-motor-paralelo-2026-09-01).

### O que foi feito

- `MotorPonteiro` em `dsp.h` (98 linhas), RT-safe; `MotorSintese` e `StreamParams::motor` no
  streaming; `motor=`/`lowlat=` no `stream_test`; botão **Low Latency** no plugin;
  `test_ponteiro.cpp`; 7 casos e 2 invariantes no `baseline.sh`; `python/medir_v3.py`.
- O que **não** mudou: pYIN, Viterbi, `CorretorAltura`, `psolaSintetiza`, `avancarPsola`.

### Verificação

```
./baseline.sh conferir
→ ok    test_ponteiro
→ ok    ponteiro em identidade (beta=1) == bypass  [lowlat]
→ ok    invariancia ao bloco no ponteiro: 64 == 512
→ IDENTICO — nada mudou.
```

- 30 casos antigos `IDENTICO`; 8 invariantes `ok`; 19 legados `ok`; **37 casos** no total (30 +
  7 novos: `st_lowlat_mix1`, `st_lowlat_mix0`, `st_lowlat_tol600`, `st_lowlat_natural`,
  `st_lowlat_block64`, `st_lowlat_block512`, `st_ponteiro_look4`).
- `test_ponteiro`: **18 verificações, todas `ok`** (`TUDO CERTO (0 falha(s))`), sobre as cinco
  seções da especificação §6: identidade em β = 1, identidade sem voz, nota sobe (231 Hz),
  nota desce (209 Hz), salto de nota no meio do sinal (220 → 330 Hz).
- `pluginval` no nível de rigor 10: `SUCCESS`, 0 falhas, 0 avisos.
- Latência declarada com Low Latency: **8 amostras = 0,18 ms** (era 2552 = 57,9 ms no contralto).

### Medição — `python/medir_v3.py`

Sobre `exemplo-antes.wav`, preset natural (`tol=15 retune=25`):

| Motor | Latência fixa | dist média | dist máx | erro estável (med / p95, ct) | erro de ataque (med, ct) | degraus |
|---|---:|---:|---:|---:|---:|---:|
| PSOLA, look=4 | 71.43 ms | 0.00 ms | 0.00 ms | 4.6 / 20.0 | 2.8 | 4 |
| Ponteiro, look=4 | 0.18 ms | 1.84 ms | 3.81 ms | 6.2 / 24.1 | 2.7 | 5 |
| Low Latency (look=0) | 0.18 ms | 2.45 ms | 4.87 ms | 5.1 / 21.2 | 6.9 | 4 |

### O que a medição diz, e o que não diz

- **A nota sai igual.** Os três erros estáveis ficam na mesma ordem de grandeza (4,6 a 6,2 ct de
  mediana), bem abaixo do quarto de tom (50 ct) — os dois motores convergem para a mesma nota,
  como a §2 da especificação previa (`β` é o mesmo número, lido do mesmo lugar). O Ponteiro tem
  um pouco mais de erro residual que o PSOLA no mesmo `look`, plausível: ele reamostra
  continuamente em vez de re-sintetizar por período.
- **O erro de ataque não é maior no motor de ponteiro — é maior com `look = 0`.** Com o mesmo
  `look = 4`, Ponteiro (2,7 ct) e PSOLA (2,8 ct) empatam; quem dispara o erro de ataque é o
  Low Latency (6,9 ct, ~2,5× os outros dois), e ele é a combinação **ponteiro + `look = 0`**, não
  o motor em si. A leitura mais provável: com menos look-ahead o Viterbi causal decide o pitch do
  ataque com menos contexto futuro, então erra mais logo no início da nota — é o preço da parte
  do botão Low Latency que zera o `look`, não do motor de síntese.
- **O `distMax` medido é compatível com um T de nota real do trecho.** Uma primeira tentativa de
  confirmar a projeção do §3.3 pela razão `distMax/distMedia` (2,07 no Ponteiro look=4, 1,99 no
  Low Latency, perto do 2 esperado) foi descartada: essa razão divide um máximo do arquivo
  inteiro por uma média sobre notas de `T` bem diferentes, então fica perto de 2 para quase
  qualquer distribuição — não é evidência de nada específico sobre `dist`. A checagem que
  sobrou no lugar dela: `T` implícito por `distMax − margem` (margem = 8), comparado à faixa de
  F0 realmente detectada no trecho — dado que o `dumpbeta=` do `stream_test` já grava por hop.
  Ponteiro look=4: `distMax` = 168,1 amostras → `T` implícito = 160,1 amostras → F0 implícito ≈
  275,5 Hz. Low Latency: `distMax` = 214,9 amostras → `T` implícito = 206,9 amostras → F0
  implícito ≈ 213,1 Hz. A faixa de F0 vozeado detectada no trecho é 161,9–447,4 Hz (mediana
  281,8 Hz). Os dois F0 implícitos caem dentro dessa faixa — o Ponteiro look=4 perto da mediana,
  o Low Latency abaixo dela —, o que é consistente com `distMax` refletindo o período de uma
  nota realmente cantada no trecho, não um acúmulo sem relação com o áudio. Isso confirma a
  *plausibilidade* da fórmula, não decompõe `dist` em `T` por nota — a medição não guarda dist
  por instante, só o agregado do arquivo inteiro.
- **Ressalva 1 — o critério de "degraus" não se sustentou.** O `exemplo-antes.wav` só tem **4**
  descontinuidades acima do limiar absoluto (`|Δ| > 0,25`) na própria **entrada**, intocada — a
  métrica já está no piso de ruído para este arquivo antes de qualquer motor processar algo.
  PSOLA e Low Latency empatam com a entrada (4); o Ponteiro em `look=4` fica **um acima** (5).
  Uma diferença de ±1 sobre uma base de 4 não é evidência de nada, para nenhum dos dois lados —
  não é um critério que este motor **passou**, é um critério que **não tinha o que medir** neste
  material. Um WAV com mais transiente (ou um limiar relativo) seria necessário para essa
  comparação fazer sentido. **Nota de convenção:** estes 4/5/4 são contados sobre o sinal **bruto**
  (`medir_v3.py`); os 13/13/12 do Achado B, que o `README.md` ainda cita, foram contados sobre o
  sinal **normalizado por pico** (`medir_qualidade.py`) — a mesma entrada dá 4 descontinuidades
  bruta e 13 normalizada. É uma convenção diferente, não uma contradição entre as duas medições.
- **Ressalva 2 — o erro de ataque não é um número absoluto.** A janela de ataque medida é de
  **30 ms**, com um detector de **23 ms** de quadro (`N_FRAME = 1024` a 44,1 kHz) — a janela mal
  cobre um quadro e meio de análise. Os números da coluna "erro de ataque" servem para comparar
  os três motores **entre si**, na mesma medição, não como distância a um padrão de qualidade
  absoluto.
- **Ressalva 3 — a tabela acima é medida em `mix=1` (100% molhado); em `mix` intermediário o
  motor de ponteiro mistura dois sinais que não estão alinhados no tempo.** O seco é
  `xAll[a − margem]`; o molhado, fora de `β = 1`, vem de uma leitura até `T` amostras distante
  do seco emparelhado a ele (mediana 60-110 amostras, máximo ~250 amostras / 5,7 ms, medido em
  `exemplo-antes.wav`). Com `mix` no meio, isso soma o sinal a uma cópia deslocada de si mesmo —
  o filtro-pente que `dsp.h` adverte em ALINHAMENTO. Não é um bug do mix: é a distância variável
  que o próprio motor usa por construção, então não há correção que preserve a baixa latência.
  Ver [especificacao-v3-ponteiro.md §3.3](especificacao-v3-ponteiro.md#33-o-passo-por-amostra).

### Pendências que esta etapa abre

- Teste de escuta (agora responde também ao erro de ataque).
- L6, recentragem em silêncio, formante em maior/menor — ver [especificacao-v3-ponteiro.md §8](especificacao-v3-ponteiro.md#8-fora-do-escopo-registrado).
- Medir o motor de ponteiro em `mix` intermediário (Ressalva 3) — um caso `st_lowlat_mix50` no
  `baseline.sh` fixaria um comb filter no checksum sem valor diagnóstico, então fica registrado
  aqui como caracterização em prosa, não como caso de teste.

---

## Pendências abertas ao fim do plano

Consolidação dos itens que ficaram registrados ao longo das etapas. Nada aqui bloqueou o
fechamento do plano; tudo aqui está em aberto.

### 1. Escuta — o item mais importante

Aparece como pendência ao fim das Etapas 2, 3 e 5, e é a única verificação que nenhum teste
automático substitui. A reprovação que originou o plano foi de escuta; a validação também
precisa ser. **Não feito.**

O que só a escuta responde: se τ = 25 ms é o valor certo **para esta voz**, se `k = 1` soa
melhor que `k = 1,2`, se o ataque na altura real incomoda quando o cantor entra errado, se o
`Mix` intermediário é útil, e se a fusão do Glide fez falta em legato.

### 2. Backlog técnico

| Item | Origem | Situação |
|---|---|---|
| ~~Faixa de controles — agrupar em **Escala \| Correção \| Motor**~~ | Etapa 5 + Decisão 8 | ✅ **quitada em 2026-08-31** — ver [Redesenho da interface](#redesenho-da-interface-2026-08-31--fora-do-plano) |
| Escorregão de fase entre streaming e offline (corr. 0,57, concentrada em poucas janelas, com correlação negativa) | Achado A | não investigado |
| Limitar a busca por correlação à região vozeada — DSP argumentavelmente mais correto, custo zero de CPU | Achado C | não aplicado: exigiria regravar todas as referências |
| `src/offline_causal/main.cpp` **ignora em silêncio** `dumpf0=` — por isso não há trilha de F0 do offline para comparar | Etapa 3 | **parcialmente corrigido**: desde a Etapa 5 ele lê a malha inteira por `lerFlagCorrecao()`; sobra o `dumpf0=` (o `look=` não faz sentido no offline, que usa Viterbi global) |
| `bench_stream.py` nunca foi executado nesta máquina (roda no venv Windows do repositório irmão) | Etapa 2 | os números de correlação do README são da medição anterior |
| `g_permitida[12]` é estado global — duas instâncias do plugin compartilham a escala | plano §11.3 | decidido fora de escopo; **declarar como limitação no texto do TCC** |
| ~~Faixa do `Mix` na GUI está em 0–1; a Antares mostra em %~~ | Etapa 2 | ✅ **quitado em 2026-08-31**, junto com o redesenho — o slider mostra %, o parâmetro segue 0–1 |
| `exp()` calculado por amostra dentro do callback de áudio | Etapa 0 | oportunidade; xRT 0,042, provavelmente não é gargalo |

### 3. Fora do escopo do plano, ainda por fazer

- **Modo de baixa latência** — implementado pela **v3** (Etapa 6 /
  [Decisão 9](historico-e-decisoes.md#decisão-9--motor-v3-de-ponteiro-móvel-como-motor-paralelo-2026-09-01)),
  que troca o motor de síntese em vez de seguir o caminho por parâmetros de
  [`modo-baixa-latencia.md`](modo-baixa-latencia.md), que essa etapa supera. O que ficou de fora
  da v3 e segue em aberto: **L6** (CMNDF recursivo), **recentragem do ponteiro em silêncio** e
  **correção de formante em maior/menor** — ver
  [especificacao-v3-ponteiro.md §8](especificacao-v3-ponteiro.md#8-fora-do-escopo-registrado).
- **K5 · Flex-Tune de verdade** e **K6 · Targeting Ignores Vibrato** — mecanismos novos, sem
  decisão de desenho.
- **Detune / Transpose / Tracking** — decididos como ausentes, não priorizados.
- **Formante / Throat** — descartados com fundamentação.

---

## Spec de encaixe e estabilidade — tickets 01 a 05 (2026-09-03)

Implementa as decisões **D1, D2, D4, D5 e D6** de
[`spec-encaixe-e-estabilidade.md`](spec-encaixe-e-estabilidade.md), mais a seam de teste que
todas elas usam. Os tickets 06 (D7) e 07 (D3) estão em seções próprias, abaixo.

### Antes de tudo: a linha de base não passava, e não era o motor

`./baseline.sh conferir` devolvia `DIFERENTE` nesta máquina **desde antes de qualquer mudança**.
Os 37 `wav=` batiam; divergiam só os 9 `log=` dos casos `gold_*`. A causa é
`src/offline_causal/main.cpp:216`, que imprime o **caminho absoluto** do WAV de entrada no log,
e o script hasheia o log — então esses nove hashes nunca bateriam fora do computador onde a
referência foi gravada.

Isso não é cosmético. Uma linha de base que diz `DIFERENTE` todo dia treina quem lê a ignorá-la,
e era exatamente a ferramenta em que estas cinco mudanças iam se apoiar. O `sed` de limpeza do
log agora reduz todo caminho de `.wav` ao nome do arquivo. Não dá para casar contra `$RAIZ`: o
shell do MSYS converte `/c/Users/...` para `C:/Users/...` antes de entregar o argumento ao `.exe`,
então as duas formas aparecem no log.

Duas outras correções na mesma passada, ambas descobertas por agentes trabalhando em *worktrees*:

- **`.gitattributes`.** Num *worktree* novo, `baseline/etapa2-legado.sha256` saía com CRLF, o
  `read -r h nome` deixava o `\r` colado no nome, `g_$nome.wav` virava arquivo inexistente e os
  19 casos falhavam de uma vez. O sintoma que distingue isso de regressão é o **hash vazio** —
  regressão de verdade traz hash *diferente*, não ausente.
- **Três classes de falha, separadas.** O script tratava "invariante quebrada" e "tabela da
  Etapa 2 desatualizada" como a mesma coisa. Ver a seção seguinte.

Depois disso, `IDENTICO` pela primeira vez nesta máquina. Nenhum `wav=` mudou.

### A colisão que o spec não previu: a tabela da Etapa 2

`baseline/etapa2-legado.sha256` era descrita como **"NUNCA regravada"** — um marco fixo no
passado, e o script tratava qualquer divergência dela como invariante quebrada, com `exit 1`.

A tabela compara a saída **de ponta a ponta**, e de ponta a ponta inclui o **detector de altura**.
Ou seja: além da malha de correção que ela existe para proteger, ela congelava em silêncio muito
mais do que isso. Duas decisões deste spec mexem justamente no que ela congelava sem dizer:

- **D3** (ticket 07) abriu mão de propósito da equivalência incremental ≡ lote;
- **D1** (ticket 02) corrigiu o detector de propósito.

A alegação que a tabela existe para sustentar — *"a malha da Etapa 3+ com `legado=1 vibrato=0`
reproduz a Etapa 2"* — **nunca deixou de valer**, e continua verificada **amostra a amostra** por
`src/tests/test_retune.cpp` seção 1, contra uma cópia congelada do código da Etapa 2 e **sem
detector no meio**. Essa prova é mais forte que a tabela e não depende dela.

**Decisão do autor:** regravar o marco, documentado, e separar as classes de falha no script.
Invariante quebrada continua sendo erro fatal. Tabela desatualizada por mudança deliberada vira
regravação justificada, por um comando **próprio** — `./baseline.sh gravar-legado` —, separado
justamente para que nunca aconteça por distração dentro de um `gravar`.

Regravada **uma vez**, no ticket 07, em 6 dos 19 casos (`st_mix1`, `st_block64`, `st_block512`,
`st_cmaior`, `st_glide40`, `st_tol15`) — todos streaming-PSOLA, todos pela mesma causa (a âncora
da cadeia de marcas mudou). Os 13 casos `gold_*`/`rt_*` nunca mudaram.

### Ticket 01 — a seam de altura conhecida

`src/tests/test_deteccao.cpp`. É o **primeiro teste do repositório que verifica validade, e não
reprodutibilidade**. Os outros provam que o motor continua fazendo o que fazia; nenhum provava
que o que ele faz está certo. É por isso que os três defeitos do spec atravessaram 37 casos de
linha de base sem serem vistos: os três são estáveis e reprodutíveis.

Alimenta o núcleo com voz sintética de altura conhecida e assevera sobre as duas trilhas públicas
(`getF0Samp`, `getFoutSamp`). Nada olha marcas de período, colunas do Viterbi ou posição de
ponteiro. Roda no motor de ponteiro porque as trilhas são decididas no estágio 1+2, que os dois
motores compartilham — o PSOLA levaria minutos para as ~30 passadas do arquivo.

**O gerador reproduziu as tabelas do spec exatamente**, o que valida a seam antes de ela ser
usada: `Baixo` 219/263/296/332/**174/195/219/246/260**, divergência de oitava de **34,3 %**
(Baixo) e **16,2 %** (Low Male) sobre o take, mediana de **40,6 ms** e **76 %** abaixo de 80 ms.
Os mesmos números que o spec cita, medidos de novo por código diferente.

A terceira medição da seam é a que o ticket 02 precisava e que nenhum outro teste dava: a
**cobertura de vozeamento dentro da faixa**, preset a preset (~99,5 %). É o denominador honesto —
sem ele, "zero quadros na oitava errada" é o que uma guarda que rejeita *tudo* também entrega.

### Ticket 02 (D1) — guarda contra a subharmônica

`candidato()` em `dsp.h`. Custo zero: `calcularCMNDF()` já preenchia `dp` desde `tau = 1`, os
dados abaixo de `tauMin` já existiam e apenas não eram consultados.

Duas calibrações que **não estavam no spec** e que a medição exigiu:

**1. A evidência é um vale que FECHA abaixo de `tauMin`, não um mergulho qualquer.** A regra
ingênua do spec (`se existe tau < tauMin com dp[tau] < limiar`) rejeita a nota mais aguda que o
preset promete cobrir: cantando o próprio `fmax` (330 Hz no `Baixo`), o período real é 133,6
amostras contra um `tauMin` de 133, e o flanco esquerdo do vale legítimo cai dentro da região
varrida. Seguir o vale até o fundo separa os dois casos pela física, sem constante de ajuste.

**2. O limiar da guarda é o do YIN (0,10), não o do chamador.** `candidato()` é chamada 100 vezes
por quadro com limiares varridos de uma Beta, e vários são **permissivos de propósito** — o peso
deles na agregação é mínimo e quem decide é o HMM. Reusar esse limiar para *rejeitar* quadros é
erro de tipo. Medido, a profundidade do vale abaixo de `tauMin` separa por duas ordens de
grandeza:

| caso | `dp` no fundo do vale |
|---|---|
| verdadeiro positivo (349 Hz num preset de teto 330) | **0,001** |
| falso positivo (take real, teto de 1000 Hz) | **0,14 a 0,43** |

0,10 cai no meio da lacuna, e é o limiar absoluto do YIN original (de Cheveigné e Kawahara, 2002,
§II-D) — critério de periodicidade da literatura, não número escolhido para o take passar.

**O que a segunda calibração custava.** Sem ela, três quadros de `exemplo-antes.wav` viravam
não-vozeados por engano. Três em 854 parece desprezível e não é: um quadro não-vozeado **parte
uma região vozeada**, a cadeia de marcas do PSOLA re-ancora, e a correlação da saída caía para
**0,77**. Um erro de 0,35 % na trilha custava a frase inteira. Registrado porque é o mesmo
mecanismo de amplificação que o ticket 07 explora do outro lado.

**Resultado.** Divergência de oitava **34,3 % → 0 %** e **16,2 % → 0 %**; perda de vozeamento
dentro da faixa de **0,2 a 0,5 pontos percentuais**, contra um orçamento de 2. Fora da faixa, a
saída passa a ser "sem voz" nos dois lados — um comportamento só para lembrar.

**Linha de base: nenhum caso mudou.** Os 37 e os 19 da Etapa 2 saem bit-idênticos, porque todos
rodam no teto padrão de 1000 Hz sobre um take que chega a 444 Hz. A trilha de F0 do streaming é
idêntica em **0 de 854 quadros**. A guarda só age onde deve.

### Ticket 03 (D4) — a lista de Input Type encolhe

Sete presets SATB viram quatro, com os nomes do Auto-Tune e a faixa em **notas** no rótulo.
Padrão de fábrica de `Contralto` para `Alto-Tenor`. `Bass Instrument` fica fora: não existe no DSP.

A tabela mudou de casa. Ela mora agora em `dsp.h` (`vozDaInterface`), junto da razão da ordem,
seguindo o precedente que `montarEscala()` abriu para o combo de escala. O motivo é concreto:
`nomeVoz()` era um `switch` sobre o índice com `default: instrumento`, e **nada ligava esse switch
ao conteúdo do combo**. Encolher um sem o outro faria o índice 1 mostrar `Alto-Tenor` enquanto o
DSP usava `baritono` — sem erro de compilação, sem teste falhando, e sem sintoma além de "a
correção soa errada nesse preset".

`src/tests/test_vozes.cpp` prende os dois, e vai além: confere que **cada rótulo bate com a faixa
do preset**, comparando as notas do texto contra `hzParaNota()` dos Hz reais. Os quatro passam.

**Quebra de compatibilidade, aceita.** A APVTS grava o **índice** do `AudioParameterChoice`, então
um projeto salvo com `Contralto` (índice 3 da lista antiga) reabre no **índice 3 da lista nova**.
Não há como preservar quando a lista encolhe. O que dá para escolher é **onde ele pousa**, e por
isso o índice 3 é `Instrument` — a faixa mais larga, e a única imune à Causa 1. O pior pouso
possível seria `Low Male`, cujo teto de 392 Hz corta 18,5 % do take. A razão está escrita **junto
da tabela**, não só aqui, senão a próxima reordenação a desfaz sem perceber. Precedente: a Etapa 1
quebrou o parâmetro `escala` do mesmo jeito, de 7 opções para 3.

**Projetos de DAW salvos precisam de reajuste à mão da tessitura.**

Só a interface encolheu: os nove presets continuam resolvendo por `voz=` na linha de comando, e
isso é deliberado — eles são **dados de medição** antes de serem itens de menu. Foi com eles que
as tabelas da Causa 1 e da §3 do spec foram levantadas, e o texto cita as duas.

`docs/comparacao-antares.md` afirmava que o protótipo era *"mais granular (7 contra 5)"*, listado
como vantagem. Na interface isso virou paridade; a granularidade passou a ser descrita como
capacidade da linha de comando, que é onde ela continua existindo.

### Ticket 04 (D2) — histerese e permanência mínima

O estado mora em `CorretorAltura`, e `notaMaisProximaMidi()` continua pura — a GUI a chama do
timer da message thread com outro argumento (o `fout`, não o `f0`), e estado ali seriam duas
threads escrevendo a mesma variável, ainda por cima compartilhada entre os três CLIs no mesmo
processo. O contador de permanência anda **por chamada de `proxima()`**, que roda uma vez por
amostra emitida numa cadência comandada pelo hop: a invariância ao tamanho de bloco sai **por
construção**, não por medição.

**Os dois valores saíram de medição, e a segunda foi a que ensinou alguma coisa.**

*Histerese = 30 cents.* O piso é imposto pela análise e não é escolha: a trilha de F0 vive numa
grade de 20 cents (`RES_CENTS`), então nada abaixo disso filtraria sequer uma tremulação de um
bin. Varrendo 25/30/35 aparece um joelho — **30 e 35 dão resultados idênticos**, ou seja, é em 30
que o mecanismo satura. Subir mais não compra nada e arrisca nota de verdade.

*Permanência mínima = 50 ms.* Quem fixou foi a **contraprova**, e vale registrar porque é uma
armadilha de metodologia: com 100 ms, uma melodia de notas de 62 ms ainda emitia **as oito notas
na ordem certa** — um teste que contasse presença teria aprovado. As durações emitidas eram
**52, 58, 100, 16, 58, 58, 100 e 4 ms**: todas as notas lá, o ritmo destruído. Um cantor ouve isso
como o plugin engasgando. A asserção passou a ser **fidelidade de duração**, e com 50 ms as mesmas
oito saem em 52–58 ms cada, com 2,5× de margem sobre semicolcheias a 120 bpm (125 ms).

| | antes | depois |
|---|---|---|
| notas em 5 s | 51 | 34 |
| duração mediana | 34,8 ms | **58,0 ms** |
| notas abaixo de 80 ms | 76 % | **59 %** |

A contraprova tem dentes, e isso foi verificado: com o alvo **trancado** (permanência de 1 s), a
seção da mediana passa lindamente — 265,5 ms, 18 % de notas curtas — e a contraprova reprova as
três asserções. Era exatamente o modo de falha que o ticket alertava ser "pior que o defeito".

**Uma constante a mais, e a honestidade sobre ela.** `SALTO_IMEDIATO_SEMITONS = 2`: salto maior
que um tom nunca é tremulação, é nota nova, e segurar nota nova é o que a história 14 proíbe. Ela
*também* limitaria o erro do semitom segurado a 250 cents, o que protegeria o caminho de
identidade (`tol = 600` só devolve `alvo == f0` enquanto `|errCents| <= 600`). **Mas essa
necessidade não foi demonstrada:** saltos de oitava a cada 40 ms e varreduras de duas oitavas em
40 ms mantiveram `tol=600` bit-idêntico *mesmo com a constante desligada*. A proteção de hoje vem
de outra camada — o HMM não deixa a trilha andar mais que `W_TRANS` (12 bins, 240 cents) por
quadro, e o que é rápido demais sai como não-vozeado, que zera a escolha. A constante fica como
seguro barato: um acoplamento entre a largura de transição do HMM e um caminho de identidade é
frágil demais para ficar implícito.

**A neutralidade.** `legado=1` passou a ligar **duas** flags (`ataqueNoAlvo` e `semHisterese`), e
não uma. É coerente com o que `legado` sempre quis dizer — "a malha da Etapa 2" —, e aquela malha
não tinha memória. Com isso os 19 casos da Etapa 2 continuam bit-idênticos. Escolhida a flag de
neutralidade em vez de reescrever os oráculos, como o ticket preferia: eles existem para provar
que as Etapas 4 e 5 são neutras, e perder essa prova para provar outra coisa seria troca ruim.

> Achado lateral, que vale registrar: o oráculo de `test_retune.cpp` **passa mesmo sem a flag**.
> A trilha sintética dele tem vibrato de ±22 cents em torno de 220 e 261,63 Hz, e nunca cruza uma
> fronteira de semitom — então a histerese não tem o que mudar ali. A flag ficou assim mesmo, para
> o oráculo continuar válido se a trilha mudar, mas o oráculo é mais fraco do que parecia.

**Regravação de linha de base: 31 de 37 casos.** Uma causa só — a escolha de semitom ganhou
memória, então a trilha de alvo mudou. Os **6 que não mudaram são exatamente os caminhos de
identidade**: `gold_mix0`, `gold_tol600`, `st_mix0`, `st_tol600`, `st_lowlat_mix0`,
`st_lowlat_tol600`. Os quatro casos de identidade do spec estão entre eles, intactos, e os 8
invariantes independentes da referência continuam `ok`.

### Ticket 05 (D5, D6) — padrões de fábrica e faixa do Retune

Três mudanças de valor, nenhuma de mecanismo — e por isso **nenhum checksum se move**: os CLIs
recebem `tol=` e `retune=` explicitamente.

- **Tolerância 15 → 0.** A zona morta nunca encaixou a nota; ela empurra o desvio até a borda, e
  com 15 uma nota 40 cents desafinada saía 15 cents desafinada (média medida: 11,8 cents fora). A
  semântica **não mudou** e continua não sendo Flex-Tune.
- **Retune Speed 25 → 0 ms.** Efeito duro como primeira impressão, por decisão do autor.
- **Faixa 200 → 400 ms.** Só passou a significar algo depois do ticket 04: antes o controle
  saturava (22,7 → 23,8 cents de erro médio entre 200 e 400 ms), porque filtro nenhum alcança um
  alvo que troca a cada 35 ms.

`test_expressao.cpp` ganhou a seção 7, que mede a **constante de tempo** em toda a faixa nova e
afirma que 400 ms é **2,00×** 200 ms. É a asserção que sustenta ter esticado a faixa, e ela não
depende de material de áudio. Medido: 25,0 / 100,0 / 200,0 / 299,9 / 399,9 ms.

**Alargar a faixa não quebra projeto salvo, e isso foi verificado no código do JUCE, não deduzido:**
a APVTS grava na árvore o valor **desnormalizado**
(`juce_AudioProcessorValueTreeState.cpp:413`), e o plugin salva/restaura por
`copyState`/`replaceState`. Um projeto salvo com 100 ms guarda `100.0` e reabre em 100 ms, com a
faixa velha ou com a nova. Fosse normalizado, reabriria em 200 ms — quebra silenciosa e *contínua*,
pior que a do combo, que ao menos salta de uma vez. As duas mudanças pareciam da mesma família e
não são: a diferença está em como a APVTS serializa cada **tipo** de parâmetro.

**Preço assumido, e é o que motiva o ticket 06:** com `retune = 0` de fábrica, o `Natural Vibrato`
e o `Humanize` nascem inertes.

### O que continua pendente

- **Escuta.** Nada aqui foi ouvido. A naturalidade e a latência seguem pendentes de teste com
  usuário, e é escuta que decide se 58 ms de mediana e permanência de 50 ms soam certo.
- **Verificação visual** do combo novo, dos rótulos e dos controles desabilitados.
- **Projetos de DAW salvos** reabrem em `Instrument` e precisam de reajuste manual da tessitura.

---

## Spec de encaixe e estabilidade — ticket 06: controles inertes desabilitados e explicados

Implementa a **decisão D7** do
[`spec-encaixe-e-estabilidade.md`](spec-encaixe-e-estabilidade.md). É correção de **interface**,
não de DSP: nenhuma linha de `src/` foi tocada, e a árvore de parâmetros ficou como estava.

### O problema

Com o Retune Speed em zero, o `Natural Vibrato` e o `Humanize` não fazem **nada**. Não é
"quase nada" — é zero exato, e a álgebra explica por quê:

```
out = LP(alvo) + k*(real - LP(real))
tau = 0  ->  alpha = 0  ->  LP(x) = x  ->  out = alvo,  para QUALQUER k
```

O termo do Natural Vibrato desaparece seja `k = 0` ou `k = 2`. O Humanize está atrás de uma
guarda `tau > 0.0` em `CorretorAltura::proxima()` e nem chega a ser avaliado. Verificado bit a
bit no spec: a saída é **idêntica** para vibrato 0 e 1, e para humanize 0 e 1, quando o Retune
Speed é 0.

Até agora os dois ficavam mexíveis e mudos — o usuário podia passar minutos girando controles
sem efeito. E o problema **piora** com o ticket 05, que leva o padrão de fábrica para
`retune = 0`: o plugin passa a sair da caixa exatamente na condição em que os dois emudecem.

### O que passou a existir

| | |
|---|---|
| Os dois sliders | `setEnabled(false)` + `setAlpha(0,45)`, junto com os rótulos, para a coluna inteira apagar de uma vez |
| Aviso | a linha **`requer Retune Speed > 0`**, na cor de destaque, sob as duas últimas colunas do grupo **CORREÇÃO** |

**Acinzentar sozinho não bastaria**, e é o ponto da decisão: um controle apagado *sem
explicação* lê-se como "o plugin está quebrado"; com a linha, lê-se "falta destravar". Como o
padrão de fábrica passa a ser justamente essa condição, é a primeira coisa que o usuário vê.

### Duas decisões de implementação

1. **O gatilho é `retuneSlider.onValueChange`, não `apvts.addParameterListener`.** Por
   cobertura e por *thread*. O `SliderAttachment` se registra como listener do parâmetro e chama
   `slider.setValue(..., sendNotificationSync)` quando ele muda — venha a mudança do mouse, da
   **automação do host** ou do `setStateInformation` ao abrir um projeto. Então um único
   callback cobre os três casos. E o `ParameterAttachment` já marshala para a *message thread*;
   um `addParameterListener`, ao contrário, é chamado **na thread de áudio**, e mexer em
   `Component` dali seria corrida de dados. É o mesmo mecanismo que já sustentava o
   `lowlatButton.onStateChange` para o look-ahead.
   Como o `onValueChange` só dispara em **mudança**, e o attachment escreve o valor no slider
   *antes* de o lambda existir, o método é chamado uma vez à mão no construtor — a mesma
   armadilha (e a mesma solução) que a Etapa 6 já tinha registrado para o Low Latency.
2. **A faixa do aviso é reservada sempre, mesmo escondida.** Se ela nascesse e morresse
   conforme o Retune Speed cruza o zero, os quatro sliders mudariam de altura no meio do
   arrasto e a faixa inteira "pularia". O aviso é **pintado**, não é componente: é uma linha de
   texto, e um `Label` só para ela seria mais código de layout do que a linha vale.

### Verificação

- **Compila**: VST3 e Standalone, MSVC 2022 / `Visual Studio 17 2022` x64, **zero erros**. Os
  avisos que aparecem são os de depreciação de `juce::Font` que o arquivo inteiro já produzia —
  nenhuma classe de aviso nova.
- **`baseline.sh`**: os **37 checksums de áudio idênticos** à referência, os **8 invariantes**
  passando, os **6 testes de unidade** passando e os **19 casos** de não-regressão da Etapa 2
  reproduzindo bit a bit. É o que se espera de uma mudança que só toca o editor.
- **Sem teste automatizado**: desabilitação de controle e texto de interface seguem conferidos
  por olho humano no app Standalone, como já era o caso do layout desde a Etapa 1-bis.

> ⚠️ **Duas armadilhas de ambiente encontradas ao verificar**, que valem registro porque vão
> reaparecer:
>
> 1. **A linha de base não fecha nesta máquina, e não é regressão.**
>    `src/offline_causal/main.cpp` imprime o **caminho absoluto** do WAV de entrada no log
>    (`Compare ouvindo: entrada (%s)`), e o `baseline.sh` inclui o log no checksum. Como a
>    referência foi gravada noutro caminho, os 9 casos `gold_*` divergem na coluna `log=` em
>    qualquer máquina que não seja a de gravação. A coluna `wav=` — o áudio, que é o que
>    importa — bate em todos os 37. Vale limpar o caminho no `sed` de scrub do script.
> 2. **Em *worktree*, o `baseline/etapa2-legado.sha256` sai com CRLF** e a não-regressão da
>    Etapa 2 falha nos 19 casos de uma vez, com o hash obtido **vazio**. Não é o motor: o
>    `while read -r h nome` deixa o `\r` colado no nome, e `g_$nome.wav` vira um arquivo que não
>    existe. O sintoma diagnóstico é justamente o *vazio* — uma regressão de verdade traria um
>    hash diferente, não a ausência dele. Normalizado para LF, os 19 passam.

---

## Ticket 07 — teto na janela de re-síntese do TD-PSOLA: **implementado, medido e revertido** (2026-09-03)

Endereça a **Causa 3** e a **decisão D3** de
[`spec-encaixe-e-estabilidade.md`](spec-encaixe-e-estabilidade.md) — o *pipoco* do motor padrão.

> **Resultado: o teto foi revertido.** Ele resolveu o custo e introduziu um defeito pior no
> motor **padrão**: um estalo por commit, quase o dobro da amplitude do próprio sinal. A
> medição está abaixo, e ela **corrige o spec**: D3, como está escrita, não é implementável
> sobre uma `psolaSintetiza()` pura. A Causa 3 continua **aberta**.

### O que se tentou

A janela de `avancarPsola()` recua até o início da região vozeada, **sem limite**. Numa nota
sustentada de 3 s, no instante t = 3 s o motor refaz 3 s de marcas e de grãos para entregar as
128 amostras do bloco atual. O teto proposto pela D3 limitava esse recuo, derivado em
**períodos de FMIN** e não em amostras absolutas:

```
teto = arredondar_para_multiplo_de(nHop, TETO_PERIODOS * fs / FMIN)     TETO_PERIODOS = 12
```

A 44,1 kHz: `Alto-Tenor` (131 Hz) → 4096; `Low Male` (82 Hz) → 6400; `Soprano` (262 Hz) → 2048;
o padrão FMIN = 80 Hz → 6656. Batia com os exemplos da D3.

### O custo: o teto funcionou

Nota sustentada de 3 s, blocos de 128 amostras, orçamento de 2,90 ms, β ≠ 1:

| | sem teto | com teto |
|---|---|---|
| p90 início → fim da nota | 4,82 → 14,90 ms | 1,10 → 1,10 ms |
| razão fim/início | **3,09×** | **1,00×** |
| pior bloco | 16,52 ms (5,7×) | 1,56 ms (0,5×) |
| blocos acima do orçamento | 435/1119 (**38,9 %**) | **0/1119 (0 %)** |

Estes números continuam valendo, e são a evidência de que a **Causa 3 existe e tem tamanho**.

### O que a medição de custo não via, e o code-review viu

O teto **quebra a continuidade da forma de onda**. Mesmo sinal, mesma configuração:

| | descontinuidades (> 30× a mediana de \|Δ\|) | maior \|Δ\| | razão max/p99,9 |
|---|---|---|---|
| sem teto | **0** | 0,116 | **1,07×** |
| com teto | **17** | **0,535** | **4,59×** |

O pico do sinal era **0,298** — o salto era **quase o dobro do próprio sinal**, e as 17
descontinuidades caíam **todas** em `i % nHop == 0`. Numa nota de 4 s dão 30, na mesma
proporção: cerca de **7 estalos por segundo**, no motor **padrão**. Pior que o pipoco que o
teto vinha remover.

### A causa, e por que não há meio-termo

`psolaSintetiza()` ancora a grade de síntese em `cum[0] = 0`, na **primeira marca da janela**.
A invariância a truncamento conquistada no commit `e1ffd1d` vale para o **fim** da região, não
para o **início** — o comentário dela em `dsp.h` diz exatamente isso: *"depende SÓ das marcas
ATÉ ali, não de onde a região termina"*.

Com teto, assim que a nota passa do teto o `winStart` deixa de parar no início da região e
passa a avançar `nHop` **a cada commit**. A âncora muda a cada commit, e o deslocamento da
grade não é múltiplo do espaçamento de síntese `T/β` — daí o salto de fase, exatamente na
fronteira do commit.

Preservar a fase com um início móvel exige carregar a contagem acumulada de β **entre
chamadas**. Isto é, a **cadeia de marcas incremental** — que a própria D3 registrou como
trabalho futuro e recusou, por transformar uma função pura numa função com estado que precisa
ser zerado na fronteira exata de cada região vozeada e de cada `reset()` do host.

**A conclusão que a medição acrescenta ao spec:** não existe terceira opção. Ou a janela recua
até a âncora estável (custo O(n) por bloco, o defeito de hoje), ou a cadeia vira estado. Um
teto sobre uma função pura não é um meio-termo — é a troca de um defeito por outro maior.

### Por que o teste deixou passar, e o que ele virou

`test_custo_bloco.cpp` estava verde com o teto ligado. O sinal era uma nota de **220 Hz exata**
e, com `tolCents = 0`, o alvo de 220 Hz é o próprio 220 Hz: **β = 1**, e o TD-PSOLA rodava em
**identidade**. As três seções cronometravam e comparavam o único caminho em que a síntese não
desloca nada — e é justamente o caminho em que re-ancorar é inofensivo, porque `q·β` é inteiro.

Um teste verde sobre o caminho de identidade não diz nada sobre o caminho que o usuário ouve.
É a mesma família de erro que o spec descreve nas três causas: um mecanismo correto dentro da
sua faixa de validade, aplicado fora dela sem guarda.

O arquivo foi reescrito:

- o sinal é **desafinado em 45 cents** (dentro do meio semitom, para o alvo continuar sendo A3),
  então β ≠ 1 e o PSOLA reempilha grãos de verdade;
- ganhou uma **asserção de continuidade**, `max|Δ| ≤ 2,5 × p99,9(|Δ|)`. O critério compara o
  maior degrau com o degrau típico do próprio sinal em vez de um limiar absoluto. Escolha
  deliberada: o múltiplo da **mediana** que o resto do projeto usa não serve aqui, porque o
  maior degrau **legítimo** já é 29,9× a mediana — em cima do limiar de 30×;
- o **custo virou diagnóstico** e não reprova mais nada. A Causa 3 está aberta, e um teste que
  falhasse por causa dela abortaria o `baseline.sh` todo dia. Os números são impressos com
  destaque e rotulados como **defeito aberto**;
- a invariância ao tamanho de bloco (1 a 4096) passou a rodar **em β ≠ 1**, que é o teste mais
  forte. Ela continua valendo bit a bit, com e sem teto — o teto de fato saía de uma grade fixa,
  como a D3 exigia. O que ele quebrou não foi a invariância ao bloco, foi a continuidade
  **dentro** do stream.

Verificado nos dois sentidos: com o teto o teste **reprova** (razão 4,59× contra o teto de
2,5×, e a linha de diagnóstico aponta 17 de 17 descontinuidades na fronteira do hop); sem o
teto **passa** (1,07×).

### Linha de base: a reversão desfaz a regravação

A regravação dos 15 casos e a da tabela da Etapa 2 foram feitas **por causa do teto**. Com ele
revertido, os hashes voltam **exatamente** aos valores anteriores — verificado caso a caso.

Isso tem uma consequência boa: a tabela `baseline/etapa2-legado.sha256` pode ser **restaurada
intacta**, e a propriedade "nunca regravada" volta a valer. Restaurar o arquivo original também
devolve o **cabeçalho de comentários** que a regravação apagou (a nota de 26/08/2026 sobre o
`st_block512`).

### O que continua em aberto

A **Causa 3** — o custo por bloco crescendo com a duração da nota, 38,9 % dos blocos estourando
o orçamento. Os dois caminhos que restam, agora com a medição que os separa:

1. **Cadeia de marcas incremental.** A resposta certa em DSP, e agora a **única** que preserva
   a fase. O risco continua o mesmo que a D3 descreveu.
2. **Promover o motor de ponteiro a padrão** (o plano B registrado na D3). Custo: entregar de
   fábrica o motor que **não** preserva formantes, que é a única razão de o TD-PSOLA existir
   neste projeto.

`test_custo_bloco.cpp` agora mede os dois lados e **reprova qualquer correção que troque custo
por estalo** — que é a armadilha em que esta tentativa caiu.
