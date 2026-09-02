# TCC Autotune — protótipo em C++

Protótipo de **correção automática de afinação vocal (autotune)** em tempo real, construído
com apoio de bibliotecas de software livre (`dr_wav`, JUCE). Detecta o pitch com **pYIN**
(YIN probabilístico + HMM/Viterbi) e corrige a afinação por um de **dois motores de síntese**,
com o mesmo deslocamento de pitch: o **TD-PSOLA** (padrão), que preserva os formantes ao custo
de uma latência proporcional a `fs/FMIN`, ou o **ponteiro móvel** (v3, botão *Low Latency*),
que derruba a latência fixa para 8 amostras e paga com o formante. A escolha entre os dois é a
principal contribuição de engenharia do TCC 2 — ver a
[linha do tempo](#linha-do-tempo-das-mudanças-tcc-2).

Entrega em três formas: executáveis de linha de comando (offline e causal), um núcleo de
streaming header-only e um **plugin VST3 / Standalone** testado no Ableton Live.

Parte prática do TCC (PUCRS, 2026) — *Desenvolvimento de um Protótipo Gratuito de Correção
Automática de Afinação Vocal*.

> **Repositório irmão:** [`TCC-autotune-python`](https://github.com/gacherubini/TCC-autotune-python)
> — o estudo comparativo de algoritmos de detecção de pitch que fundamentou a escolha do pYIN.

---

## 📖 Documentação

**Comece por [`docs/README.md`](docs/README.md)** — índice completo.

| Documento | Para quê |
|---|---|
| [`docs/documentacao-tecnica.md`](docs/documentacao-tecnica.md) | **Referência completa**: o sistema inteiro, diagnóstico dos problemas em aberto, soluções propostas e backlog priorizado |
| [`docs/teste-de-usuario.md`](docs/teste-de-usuario.md) | O que o teste com usuário reprovou (latência e naturalidade) |
| [`docs/arquitetura-streaming.md`](docs/arquitetura-streaming.md) | Como o motor de tempo real funciona |
| [`docs/historico-e-decisoes.md`](docs/historico-e-decisoes.md) | Bugs caçados, decisões, varreduras experimentais e a **errata da revisão bibliográfica** |
| [`docs/comparacao-antares.md`](docs/comparacao-antares.md) | Comparação controle a controle com o Auto-Tune: o que falta para ser um plugin completo |
| [`docs/plano-de-implementacao.md`](docs/plano-de-implementacao.md) | 📐 **O que vai ser implementado e como** — 6 etapas verificáveis |
| [`docs/execucao-do-plano.md`](docs/execucao-do-plano.md) | 📓 **O que já foi feito** — diário das etapas, com a verificação de cada uma |
| [`docs/modo-baixa-latencia.md`](docs/modo-baixa-latencia.md) | Especificação do modo de baixa latência — ⏸️ **superado pela v3** ([especificação](docs/especificacao-v3-ponteiro.md), implementada na Etapa 6) |
| [`docs/especificacao-v3-ponteiro.md`](docs/especificacao-v3-ponteiro.md) | Especificação do **motor v3 de ponteiro móvel** — a mecânica do anel, a fiação no streaming e o que ele custa em formante |
| [`docs/plano-v3-ponteiro.md`](docs/plano-v3-ponteiro.md) | 📐 O plano da Etapa 6, tarefa a tarefa |
| [`docs/analise-v1-v2-v3.md`](docs/analise-v1-v2-v3.md) | ⚠️ **Leia antes de citar qualquer número de latência** — compara v1/v2/v3 por estágio e corrige dois números errados nos outros documentos |
| [`docs/pesquisa-latencia-antares.md`](docs/pesquisa-latencia-antares.md) | 🔬 Como o Auto-Tune declara 0,84 ms: a dedução das **37 amostras fixas** e a arquitetura de ponteiro móvel da patente |
| [`docs/diagnostico-block512.md`](docs/diagnostico-block512.md) | 🔬 A invariância ao tamanho de bloco: causa raiz, medições e as três correções avaliadas |
| [`docs/pesquisa-bibliografica.md`](docs/pesquisa-bibliografica.md) | As fontes: artigos, a patente do Auto-Tune, manuais |
| [`docs/pesquisa-retune-speed-e-cor.md`](docs/pesquisa-retune-speed-e-cor.md) | O que é o Retune Speed, e por que **formante não dá "cor"** a um corretor |
| [`tcc-texto/`](tcc-texto/) | O texto do TCC em LaTeX |

> **Estado atual:** o pipeline funciona ponta a ponta e o plugin roda no Ableton. O teste de
> usuário original reprovou dois requisitos — **latência** e **naturalidade** ("duro, robótico").
> O diagnóstico está em [`docs/documentacao-tecnica.md`](docs/documentacao-tecnica.md) §8 e §9.
>
> - **Naturalidade:** endereçada pelas Etapas 3 a 5 do plano (Retune Speed com fusão do Glide,
>   Humanize, Natural Vibrato e Create Vibrato), **implementadas e verificadas em 26/08/2026**.
>   Falta a **reavaliação de escuta** com o mesmo usuário — é o próximo passo do trabalho.
> - **Latência:** ✅ **v3 implementada** (Etapa 6, 2026-09-02) — motor de **ponteiro móvel**
>   selecionável por `motor=`/`lowlat=` ou pelo botão **Low Latency** do plugin, com latência
>   fixa de **8 amostras (0,18 ms)**, no lugar do piso `fs/FMIN` do TD-PSOLA. A parte **variável**
>   (0 a T da nota cantada) não é declarada ao host e precisa ser citada junto no texto do TCC.
>   Falta a reavaliação de escuta, que agora também responde ao erro de ataque. Ver
>   [Etapa 6 do diário](docs/execucao-do-plano.md#etapa-6--motor-v3-de-ponteiro-móvel-low-latency)
>   e [`docs/especificacao-v3-ponteiro.md`](docs/especificacao-v3-ponteiro.md). O modo v1/v2 por
>   parâmetros de [`docs/modo-baixa-latencia.md`](docs/modo-baixa-latencia.md) fica superado.
>
> ⚠️ **Latência sempre tem de ser citada junto com o FMIN — e, no motor v3, junto com a parte
> variável.** A guarda do PSOLA é proporcional a `fs/FMIN`: **71,4 ms** com o FMIN padrão de
> 80 Hz, e **57,9 ms** com `voz=contralto` (FMIN 175 Hz) — o valor citado no texto do TCC 1. No
> motor de ponteiro, a parte fixa é **8 amostras (0,18 ms)** para qualquer FMIN, mas soma-se uma
> parte variável de 0 a T (o período da nota cantada) que **não** entra no `setLatencySamples`.
>
> A revisão bibliográfica de 2026-08-26 **corrigiu oito afirmações** dessa documentação,
> incluindo a própria meta de latência (os ≤ 20 ms não têm respaldo revisado por pares).
> O texto original foi preservado; as correções estão na
> [errata](docs/historico-e-decisoes.md#errata--afirmações-corrigidas-pela-pesquisa-bibliográfica-2026-08-26).

---

## Linha do tempo das mudanças (TCC 2)

Tudo o que entrou no código depois do teste com usuário, em ordem, com a data em que foi
concluído e onde está a verificação. É esta tabela que alimenta o capítulo de desenvolvimento
do texto. O diário completo, com o que cada etapa provou e o que não provou, está em
[`docs/execucao-do-plano.md`](docs/execucao-do-plano.md); os bugs e decisões anteriores a
26/08/2026 estão em [`docs/historico-e-decisoes.md`](docs/historico-e-decisoes.md).

| Data | O que mudou | Onde, no código | Como foi verificado |
|---|---|---|---|
| 2026-08-26 | **Etapa 0** — a malha de correção dos três caminhos vira uma só (`CorretorAltura`). Offline, causal e streaming deixam de ter três cópias da mesma matemática | `src/core/dsp.h` | 17/17 casos do `baseline.sh` idênticos |
| 2026-08-26 | **Etapa 1** — as tonalidades vão de 6 para **24** (12 tônicas × maior/menor). A tabela de nomes ficou em `dsp.h`, não na GUI, para o teste e o plugin lerem a mesma lógica | `dsp.h`, `PluginProcessor`, `PluginEditor` | `test_escalas` (36 combos) |
| 2026-08-26 | **Etapa 1-bis** — toolchain macOS (`build.sh`, `build-mac/`) e validação com `pluginval` no nível 10 | `plugin/build.sh`, `plugin/CMakeLists.txt` | `pluginval` = `SUCCESS` |
| 2026-08-26 | **Etapa 2** — `Forca` sai, entra **Mix** seco/molhado. Um valor intermediário deixa de significar "corrija pela metade" e passa a significar "ouça metade de cada" | `dsp.h`, os três CLIs, plugin | `test_mix` + invariante `mix=0` == bypass |
| 2026-08-26 | **Etapa 3** — **Retune Speed** absorve o `Glide`, e nasce o **Natural Vibrato**. O filtro passou a agir sobre a *correção*, não sobre o *alvo*: é o que separa a deriva de afinação do vibrato | `dsp.h` | `test_retune` (ganho de vibrato dentro de 0,1 % da teoria) |
| 2026-08-26 | **Invariância ao tamanho de bloco vira estrutural.** Estava **quebrada** acima de `nHop` (256), e a afirmação nunca tinha sido testada porque o script rodava 64 e 512 sem comparar um com o outro | `autotune_stream.h` (`avancarPsola()`) | invariantes 64 == 512 e 64 == 1024; diagnóstico em [`diagnostico-block512.md`](docs/diagnostico-block512.md) |
| 2026-08-26 | **Etapas 4 e 5** — **Humanize** (afrouxa o Retune Speed na sustentação) e **Create Vibrato** (gera vibrato: forma, taxa, profundidade e amplitude) | `dsp.h` | `test_expressao` + invariantes "valor neutro não muda nada" |
| 2026-08-26 | Os três CLIs param de ler as mesmas flags com três cópias do `if/else` — vira `lerFlagCorrecao()`. Flag ignorada em silêncio é bug que nenhum teste pega | `dsp.h` | os três CLIs aceitam a mesma malha |
| 2026-08-27 | Medição de **quanto formante a reamostragem custaria**, que decidiu a viabilidade do motor v3 | `python/medir_formante_resample.py` | 2,93 % em cromática, 5,95 % em maior/menor |
| 2026-08-31 | **Decisão 8** — os quatro widgets do Create Vibrato **saem da GUI** e ficam no DSP, nos CLIs e no APVTS. É um gerador num protótipo que se declara corretor | `PluginEditor.cpp` | [Decisão 8](docs/historico-e-decisoes.md#decisão-8--create-vibrato-sai-da-interface-fica-no-dsp-2026-08-31) |
| 2026-08-31 | **GUI custom, no lugar da genérica do JUCE.** Painel afinador em cima (nota-alvo, arco de desvio com a zona morta desenhada, histórico de 2,5 s da correção) e 9 controles em três grupos embaixo (Escala \| Correção \| Motor). Tema verde escuro. Nenhuma linha de DSP | `plugin/PluginEditor.h/.cpp` | `baseline.sh` = `IDENTICO` antes e depois; `pluginval` = `SUCCESS` |
| 2026-09-02 | **O texto do afinador piscava a 60 Hz.** O `f0` muda a cada hop (5,8 ms) e o timer amostrava direto no texto. As agulhas ficam no ritmo cheio; o texto passa a 7,5 Hz | `plugin/PluginEditor.h/.cpp` | `baseline.sh` = `IDENTICO`; `pluginval` = `SUCCESS` |
| 2026-09-02 | **Etapa 6** — **motor v3 de ponteiro móvel** e o botão **Low Latency**. Latência fixa de **8 amostras (0,18 ms)**, contra `fs/FMIN` do TD-PSOLA, ao custo de não preservar formantes | `dsp.h` (`MotorPonteiro`), `autotune_stream.h`, `stream_test`, plugin | `test_ponteiro` (18 verificações), 2 invariantes novos, `python/medir_v3.py` |

**Três coisas que a tabela não diz, e que o texto do TCC precisa dizer junto:**

1. Toda etapa foi verificada por **não-regressão de áudio**, não por escuta. O `baseline.sh`
   responde "não mudou"; ele não responde "ficou bom".
2. A **reavaliação de escuta** com o mesmo usuário do teste original **ainda não aconteceu** —
   é o que fecha as duas reprovações (latência e naturalidade).
3. Os 19 casos de **não-regressão legada** (`legado=1 vibrato=0`) travam o comportamento da
   Etapa 2 num hash que nunca é regravado. É o que garante que as Etapas 3 a 6 são
   generalizações, e não mudanças de comportamento disfarçadas.

---

## Como compilar

```bat
.\compilar.bat
```

No macOS/Linux, o `baseline.sh` compila com o compilador disponível (`clang++` ou `g++`) e
ainda confere que a saída não mudou:

```bash
./baseline.sh conferir     # 37 casos; espera "IDENTICO — nada mudou."
```

Gera os três executáveis. O `.bat` já força o PATH do g++ (MinGW via scoop).
Toolchain: g++ 15.2 + CMake 4.3. Flags: `-std=c++17 -O2 -I external`.

| Executável | Papel |
|---|---|
| `autotune.exe` | offline / padrão-ouro (Viterbi global) |
| `autotune_rt.exe` | causal, com relatório de latência e xRT |
| `stream_test.exe` | driver headless do núcleo de streaming |

Para o **plugin VST3**, veja [`plugin/README.md`](plugin/README.md) (exige MSVC).

---

## Como usar

```bat
.\autotune.exe <entrada.wav> [saida.wav] [mix 0..1] [escala] [flags da malha]
```

Os **três** executáveis leem as mesmas flags de correção, pelo mesmo `lerFlagCorrecao()` em
`dsp.h` (unificado em 26/08/2026, Etapa 5). Cada um acrescenta as suas — ver as seções
seguintes.

- **mix**: cruzamento seco/molhado. `0` = só o sinal original (bypass exato) · `0.5` = metade
  de cada · `1` = só o corrigido (padrão).
- **escala**: `crom` (padrão, cromática) · `C`, `G`, `F#`... (maior) · `Am`, `C#m`... (menor).
- **tol=** (cents): zona morta — desvios menores que isso **não** são corrigidos, preservando
  vibrato e micro-afinação. Padrão `0`. Sugerido `10–20`. *(Etapa 2)*
- **retune=** (ms): **Retune Speed** — quanto tempo a correção leva para chegar à nota.
  Padrão `0` (imediato, efeito "duro"). O manual da Antares recomenda `10–50` para som
  natural. *(Etapa 3; `glide=` continua valendo como apelido do nome antigo.)*
- **vibrato=** (k): quanto do vibrato do cantor sobrevive. `0` = removido (comportamento até a
  Etapa 2) · `1` = preservado (padrão) · `>1` = exagerado. *(Etapa 3)*
- **humanize=** (0–1): afrouxa o Retune Speed na **sustentação** da nota, mantendo o ataque
  rápido. Padrão `0`. *(Etapa 4)*
- **vibforma=** (`0` off · `1` senoide · `2` triangular · `3` quadrada): **Create Vibrato**, o
  vibrato **gerado** — não confundir com o `vibrato=`, que **preserva** o do cantor. *(Etapa 5)*
- **vibtaxa=** (Hz, padrão `5.5`) · **vibprof=** (cents, padrão `0`) · **vibamp=** (0–1):
  taxa, profundidade e modulação de amplitude do vibrato gerado. Com `vibprof=0` o Create
  Vibrato não faz nada, qualquer que seja a forma. *(Etapa 5)*
- **legado=1**: faz a nota nascer **no alvo** em vez de na altura real do cantor, que era o
  comportamento até a Etapa 2. Existe para a não-regressão do `baseline.sh` travar o passado;
  não é um modo de uso.
- **mix negativo** (`-1`) = modo cópia (só converte pra mono, sem processar — diagnóstico).

> O `autotune.exe` **ignora em silêncio** `dumpf0=` (só o causal e o streaming gravam a trilha
> de F0). Item aberto no backlog.

> ⚠️ **Mudou na Etapa 2 (26/08/2026).** O 3º argumento posicional era `forca` (0–1, fração do
> desvio a corrigir) e passou a ser `mix` (seco/molhado). Os extremos se comportam igual —
> `1.0` continua sendo efeito cheio e `0.0` continua devolvendo a entrada — mas um valor
> **intermediário significa outra coisa**: antes era "corrija pela metade" (o cantor terminava
> permanentemente desafinado), agora é "ouça metade de cada sinal". Comandos antigos com `0.7`
> produzem áudio diferente. Ver [`docs/execucao-do-plano.md`](docs/execucao-do-plano.md),
> Etapa 2.

```bat
.\autotune.exe exemplo-antes.wav corrigido.wav 0.7                        REM 70% do efeito
.\autotune.exe exemplo-antes.wav corrigido.wav 1.0 Am                     REM duro, Lá menor
.\autotune.exe exemplo-antes.wav corrigido.wav 1.0 crom tol=15 glide=40   REM preset NATURAL
.\autotune.exe exemplo-antes.wav corrigido.wav 1.0 crom tol=600           REM PSOLA em beta=1
```

**Preset natural recomendado:** `mix 1.0  tol=15  retune=25  vibrato=1  humanize=0.5`.

> `tol=600` (última linha do exemplo) é um caso de **teste**, não de uso: uma tolerância maior
> que meio semitom faz o alvo coincidir com o pitch detectado, então o TD-PSOLA roda inteiro
> com β = 1 e a saída tem de sair idêntica à entrada. É o que verifica drift de fase no PSOLA.

### Versão tempo real (causal) — `autotune_rt.exe`

Mesmo áudio, mas com detecção de pitch **causal** (Viterbi de lag fixo) e relatório de
**latência (ms)** e **fator de tempo real (xRT)**:

```bat
.\autotune_rt.exe <in.wav> [out.wav] [mix] [escala] [tol=] [retune=] [vibrato=] [look=L] [block=N] [frame=] [hop=] [voz=] [fmin=] [fmax=] [dumpf0=]
```

- **look=** : quadros de look-ahead do Viterbi causal. `0` = guloso (menor latência, menor
  qualidade); maior = mais perto do offline, mais latência. Padrão `4`.
- **block=** : tamanho do bloco de áudio (entra na conta de latência). Padrão `256`.
- **frame= / hop=** : tamanho do quadro de análise e o passo. Frame menor = menos latência,
  mas detecta menos graves (mín. detectável = `fs/(frame/2)`). Padrão `1024`/`256`.
- **voz=** : preset de tessitura, igual ao *Vocal Range* do Auto-Tune (Fach/SATB):
  `baixo` (E2–E4), `baritono` (G2–G4), `tenor` (C3–C5), `contralto` (F3–F5), `mezzo` (A3–A5),
  `soprano` (C4–C6); além de `lowmale`, `altotenor` e `instrumento`.
- **fmin= / fmax=** : faixa de busca em Hz (sobrescreve o `voz=`). **`fmin` domina o piso de
  latência** (termo PSOLA = `fs/fmin`).
- **dumpf0=** : grava o F0 detectado por quadro num `.txt` (só análise).

```bat
.\autotune_rt.exe exemplo-antes.wav rt.wav 1.0 crom tol=15 glide=40 look=8 voz=contralto
```

### Motor de streaming — `stream_test.exe`

Driver headless do núcleo que o plugin usa (`src/c1_streaming/autotune_stream.h`). Aceita os
mesmos parâmetros de correção do `autotune_rt.exe`, mais a escolha do **motor de síntese**
(Etapa 6):

```bat
.\stream_test.exe <in.wav> [out.wav] [mix] [escala] [flags da malha] [look=] [block=] [frame=] [hop=] [voz=] [fmin=] [fmax=] [motor=psola|ponteiro] [lowlat=1] [dumpf0=] [dumpframes=] [dumpbeta=]
```

- **motor=** : `psola` (padrão) — TD-PSOLA, motor de referência — ou `ponteiro` (aceita também
  `v3`) — o motor de **ponteiro móvel**, que troca análise-e-ressíntese por leitura contínua de
  um anel circular a velocidade `β`. *(Etapa 6, 02/09/2026)*
- **lowlat=1** : atalho que reproduz o botão **Low Latency** do plugin — `motor=ponteiro` **e**
  `look=0` de uma vez, o que zera a parte fixa da latência para **8 amostras (0,18 ms)**.
  *(Etapa 6, 02/09/2026)*
- **dumpframes=** : grava o instante de disparo de cada quadro do ring buffer num `.txt`.
- **dumpbeta=** : grava, por hop, o `β` aplicado e o alvo `fout`. É de onde o `medir_v3.py`
  tira o alvo por amostra para medir o erro de afinação dos dois motores.

Ver [`docs/especificacao-v3-ponteiro.md`](docs/especificacao-v3-ponteiro.md) para a mecânica do
motor, e a [Etapa 6 do diário](docs/execucao-do-plano.md#etapa-6--motor-v3-de-ponteiro-móvel-low-latency)
para os números medidos.

---

## Como funciona (pipeline)

1. **Ler WAV → mono** (`dr_wav.h`, header-only).
2. **Detecção de pitch (pYIN)**: YIN (CMNDF) com multi-limiar Beta(2,18) → matriz de
   observação → **HMM/Viterbi** sobre uma grade de pitch de 20 cents. Resultado: F0 por
   quadro, estável e com decisão de vozeado/não-vozeado.
   Parâmetros no topo de `src/core/dsp.h`: `N_FRAME=1024`, `N_HOP=256`, `FMIN`, `FMAX`.
3. **Suavização do vozeamento**: tampa buracos curtos e remove ilhas curtas *(só no offline —
   é não-causal)*.
4. **Nota-alvo**: encosta na nota mais próxima da escala, com **zona morta** `tol`.
5. **Trajetória (Retune Speed + Natural Vibrato)**: dois filtros de 1 polo, um sobre o alvo e
   outro sobre a altura real, combinados como `LP(alvo) + k·(real − LP(real))`. O filtro age
   sobre a **correção**, não sobre o alvo — é o que separa a deriva lenta de afinação (a
   corrigir) do vibrato (a preservar). No ataque a nota nasce na altura real do cantor.
6. **Correção (TD-PSOLA)**: marcas de análise por período (alinhadas por correlação), síntese
   por overlap-add no novo período, reconstrução por cobertura. Como copia grãos no tempo e
   só muda o espaçamento, **preserva os formantes** (timbre).
6-bis. **Motor alternativo (v3)**: em vez do TD-PSOLA, o motor de **ponteiro móvel**
   (`MotorPonteiro`, Etapa 6) lê a mesma entrada de um anel circular com um ponteiro fracionário
   que avança a velocidade `β` — o mesmo `β` de sempre, do mesmo lugar —, saltando um período
   inteiro quando a distância até a escrita sai da faixa permitida. Latência fixa de **8
   amostras (0,18 ms)**, contra os `fs/FMIN` do PSOLA — mas ao reamostrar em vez de copiar
   grãos, **não preserva formantes**: o deslocamento é limitado a 2,93 % em cromática e 5,95 %
   em maior/menor antes de soar artificial (teto documentado, não imposto pelo código).
   Selecionável por `motor=`/`lowlat=` nos CLIs, ou pelo botão **Low Latency** do plugin. Ver
   [`docs/especificacao-v3-ponteiro.md`](docs/especificacao-v3-ponteiro.md).
7. **Mix seco/molhado**: cruzamento linear entre a entrada e o sinal corrigido (`mix`).
   No streaming o seco é atrasado da latência do motor, para os dois ficarem alinhados.
8. **Gravar WAV** 16-bit PCM mono.

Detalhamento matemático de cada estágio:
[`docs/documentacao-tecnica.md` §4](docs/documentacao-tecnica.md#4-repositório-c-o-pipeline-em-7-estágios).

---

## Estrutura do repositório

```
src/core/dsp.h                      DSP compartilhada (CMNDF, nota-alvo, TD-PSOLA, WAV)
                                    — usada por TUDO
src/offline_causal/main.cpp         autotune OFFLINE (referência/gold)  -> autotune.exe
src/offline_causal/autotune_rt.cpp  autotune CAUSAL (Viterbi lag fixo)  -> autotune_rt.exe
src/c1_streaming/autotune_stream.h  núcleo de STREAMING (header-only, usado pelo plugin)
src/c1_streaming/stream_test.cpp    driver headless do streaming        -> stream_test.exe

src/tests/test_escalas.cpp          as 24 tonalidades e os 36 combos da GUI   (Etapa 1)
src/tests/test_mix.cpp              o cruzamento seco/molhado                 (Etapa 2)
src/tests/test_retune.cpp           Retune Speed e Natural Vibrato            (Etapa 3)
src/tests/test_expressao.cpp        Humanize e Create Vibrato              (Etapas 4 e 5)
src/tests/test_ponteiro.cpp         o motor de ponteiro móvel                 (Etapa 6)

plugin/PluginProcessor.h/.cpp       APVTS <-> StreamParams, processBlock, latência declarada
plugin/PluginEditor.h/.cpp          GUI custom: painel afinador + 3 grupos    (31/08/2026)
external/dr_wav.h                   leitor/gravador WAV (header-only)
compilar.bat                        build rápido com g++ (compila os 3 exes)
baseline.sh                         VERIFICAÇÃO: 37 casos + 8 invariantes + 19 legados
                                    + as 5 suítes de src/tests/. Rode antes e depois.
exemplo-antes.wav                   excerto de voz cantada (Vocadito, CC-BY 4.0)

docs/                               DOCUMENTAÇÃO — comece por docs/README.md
tcc-texto/                          texto do TCC em LaTeX (classe EP-TCC / PUCRS)

python/medir_qualidade.py           MEDIÇÃO: latência, xRT, correlação, F0, cliques, bloco
python/medir_formante_resample.py   MEDIÇÃO: o formante que a reamostragem custa    (v3)
python/medir_v3.py                  MEDIÇÃO: PSOLA × Ponteiro × Low Latency   (Etapa 6)
python/formantes.py                 verifica preservação dos formantes (entrada vs saída)
python/bench_stream.py              valida o streaming vs. o gold, e a invariância ao bloco
python/bench_pitch.py               compara a trilha de F0 do streaming vs. o gold
python/bench_frames.py              valida o disparo de quadros do ring buffer
python/bench_latencia.py            varre look-ahead: latência × qualidade × xRT
python/bench_nframe.py              varre N_FRAME: piso de latência × fmin detectável
python/bench_fmin.py                varre presets de tessitura: latência × notas perdidas
```

**`baseline.sh` é verificação; os scripts de `python/` são medição.** Só o primeiro falha
quando algo quebra — os outros descrevem o estado atual e produzem os números do texto. Os
**`medir_*.py`** rodam em qualquer máquina (compilam o que precisam e usam o
`exemplo-antes.wav` versionado); os **`bench_*.py`** são de Windows e dependem do venv do
repositório irmão (`..\TCC-autotune-python\.venv\Scripts\python.exe`) e de um
`audioteste.wav` que não está versionado. Detalhes em [`python/README.md`](python/README.md).

---

## Testes de validação

A equivalência do núcleo de streaming com a versão offline (gold) e a ausência de cliques são
verificadas pelos scripts em `python/`, que comparam a saída causal com a de referência
amostra a amostra.

| Verificação | Script | Resultado atual |
|---|---|---|
| Identidade em `mix = 0` (bypass) | `baseline.sh`, `test_mix` | **bit-perfect** |
| Identidade em `tol = 600` (PSOLA com β = 1) | `baseline.sh` | **bit-perfect** |
| Correlação **streaming × causal**, `mix = 1` | `medir_qualidade.py` | **0,9981** (0,9979 por região vozeada) |
| Correlação **streaming × offline**, `mix = 1` | `medir_qualidade.py` | **0,5695** ⚠️ ver ressalva abaixo |
| Invariância ao tamanho de bloco (1–4096) | `baseline.sh` | **confirmada** (estava quebrada acima de 256 até 26/08/2026) |
| Trilha de F0 **streaming × causal** | `medir_qualidade.py` | **100 %**, erro máx. 0,0000 Hz |
| Disparo de quadros | `bench_frames.py` | **confirmado** |
| Cliques ("pipoco"), limiar absoluto \|Δ\| > 0,25 | `medir_qualidade.py` | **13 / 13 / 12** (offline / causal / streaming), contra **2916 da entrada intocada** |
| Preservação de formantes | `formantes.py` | **confirmada** |
| Identidade no ponteiro (`lowlat`) | `baseline.sh` | **bit-perfect** (β = 1 no motor de ponteiro == bypass) |
| Invariância ao bloco no ponteiro | `baseline.sh` | **confirmada** (64 == 512) |

> ⚠️ **Duas ressalvas de leitura, medidas em 26/08/2026** (detalhes em
> [`docs/execucao-do-plano.md`](docs/execucao-do-plano.md#achados-de-medição--três-afirmações-do-projeto-que-não-se-sustentam)):
>
> 1. **"Correlação com o gold" precisa dizer *com qual* caminho.** O que o projeto sempre
>    verificou é *streaming ≡ causal* (0,998), não *streaming ≡ offline* (0,570). A lacuna
>    contra o offline é esperada em espécie — o offline usa Viterbi global e
>    `suavizarVozeamento()`, não-causais por construção — mas o **tamanho** dela não está
>    explicado: concentra-se em poucas janelas, com correlação negativa. Item de backlog.
> 2. **"Pipoco = 0" era artefato do limiar.** O critério antigo ("descontinuidade > 30× a
>    mediana") conta conteúdo de alta frequência, não clique: dá 2916 na **entrada intocada**.
>    Com limiar absoluto a medida é defensável, e o resultado que importa se mantém — **os
>    três caminhos ficam muito abaixo da entrada; nenhum introduz descontinuidade.**

---

## Próximos passos

O backlog priorizado, com ganho, esforço, risco e encaixe no cronograma do TCC, está em
**[`docs/documentacao-tecnica.md` §10](docs/documentacao-tecnica.md#10-backlog-priorizado-e-plano-por-sprint)**.

Resumo das frentes:

- **Latência** — ✅ **v3 implementada; falta a escuta.** O motor de ponteiro móvel (Etapa 6)
  substitui o TD-PSOLA e derruba a latência fixa para **8 amostras (0,18 ms)**, atravessando o
  piso `fs/FMIN` que limitava os modos v1/v2 por parâmetros (que ficam superados, ver
  [Decisão 9](docs/historico-e-decisoes.md#decisão-9--motor-v3-de-ponteiro-móvel-como-motor-paralelo-2026-09-01)).
  Falta a parte variável (0 a T da nota cantada, **não** declarada ao host) ser citada junto no
  texto do TCC, e o teste de escuta — que agora também responde ao erro de ataque. Detalhes e
  a medição: [Etapa 6 do diário](docs/execucao-do-plano.md#etapa-6--motor-v3-de-ponteiro-móvel-low-latency)
  e [`docs/especificacao-v3-ponteiro.md`](docs/especificacao-v3-ponteiro.md).
- **Naturalidade** — ✅ **implementada** (Etapas 3 a 5): o filtro passou a agir sobre a
  *correção* em vez do *alvo* (Retune Speed, que absorveu o Glide), mais Natural Vibrato,
  Humanize, Create Vibrato e mix seco/molhado. **Falta a reavaliação de escuta.**
- **Robustez** — janela de re-síntese limitada, buffers pré-alocados (RT-safe) e normalização
  consistente.
- **Funcionalidades** — detecção automática de tonalidade.
