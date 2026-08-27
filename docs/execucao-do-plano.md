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
