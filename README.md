# TCC Autotune — protótipo em C++

Protótipo de **correção automática de afinação vocal (autotune)** em tempo real, construído
com apoio de bibliotecas de software livre (`dr_wav`, JUCE). Detecta o pitch com **pYIN**
(YIN probabilístico + HMM/Viterbi) e corrige a afinação com **TD-PSOLA**, preservando os
formantes.

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
| [`docs/modo-baixa-latencia.md`](docs/modo-baixa-latencia.md) | Especificação do modo de baixa latência — ⚠️ **nada implementado** |
| [`docs/pesquisa-bibliografica.md`](docs/pesquisa-bibliografica.md) | As fontes: artigos, a patente do Auto-Tune, manuais |
| [`docs/pesquisa-retune-speed-e-cor.md`](docs/pesquisa-retune-speed-e-cor.md) | O que é o Retune Speed, e por que **formante não dá "cor"** a um corretor |
| [`tcc-texto/`](tcc-texto/) | O texto do TCC em LaTeX |

> **Estado atual:** o pipeline funciona ponta a ponta e o plugin roda no Ableton, mas o teste
> de usuário reprovou dois requisitos — **latência de 57,9 ms** e **naturalidade**
> ("duro, robótico"). O diagnóstico está em
> [`docs/documentacao-tecnica.md`](docs/documentacao-tecnica.md) §8 e §9.
>
> A revisão bibliográfica de 2026-08-26 **corrigiu oito afirmações** dessa documentação,
> incluindo a própria meta de latência (os ≤ 20 ms não têm respaldo revisado por pares).
> O texto original foi preservado; as correções estão na
> [errata](docs/historico-e-decisoes.md#errata--afirmações-corrigidas-pela-pesquisa-bibliográfica-2026-08-26).

---

## Como compilar

```bat
.\compilar.bat
```

No macOS/Linux, o `baseline.sh` compila com o compilador disponível (`clang++` ou `g++`) e
ainda confere que a saída não mudou:

```bash
./baseline.sh conferir     # 17 casos; espera "IDENTICO — nada mudou."
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
.\autotune.exe <entrada.wav> [saida.wav] [mix 0..1] [escala] [tol=cents] [retune=ms] [vibrato=k]
```

- **mix**: cruzamento seco/molhado. `0` = só o sinal original (bypass exato) · `0.5` = metade
  de cada · `1` = só o corrigido (padrão).
- **escala**: `crom` (padrão, cromática) · `C`, `G`, `F#`... (maior) · `Am`, `C#m`... (menor).
- **tol=** (cents): zona morta — desvios menores que isso **não** são corrigidos, preservando
  vibrato e micro-afinação. Padrão `0`. Sugerido `10–20`.
- **retune=** (ms): **Retune Speed** — quanto tempo a correção leva para chegar à nota.
  Padrão `0` (imediato, efeito "duro"). O manual da Antares recomenda `10–50` para som
  natural. *(`glide=` continua valendo como apelido do nome antigo.)*
- **vibrato=** (k): quanto do vibrato do cantor sobrevive. `0` = removido (comportamento até a
  Etapa 2) · `1` = preservado (padrão) · `>1` = exagerado.
- **mix negativo** (`-1`) = modo cópia (só converte pra mono, sem processar — diagnóstico).

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

**Preset natural recomendado:** `mix 1.0  tol=15  retune=25  vibrato=1`.

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
plugin/                             plugin VST3/Standalone (JUCE) em volta do núcleo
external/dr_wav.h                   leitor/gravador WAV (header-only)
compilar.bat                        build rápido com g++ (compila os 3 exes)
exemplo-antes.wav                   excerto de voz cantada (Vocadito, CC-BY 4.0)

docs/                               DOCUMENTAÇÃO — comece por docs/README.md
tcc-texto/                          texto do TCC em LaTeX (classe EP-TCC / PUCRS)

python/formantes.py                 verifica preservação dos formantes (entrada vs saída)
python/bench_stream.py              valida o streaming vs. o gold, e a invariância ao bloco
python/bench_pitch.py               compara a trilha de F0 do streaming vs. o gold
python/bench_frames.py              valida o disparo de quadros do ring buffer
python/bench_latencia.py            varre look-ahead: latência × qualidade × xRT
python/bench_nframe.py              varre N_FRAME: piso de latência × fmin detectável
python/bench_fmin.py                varre presets de tessitura: latência × notas perdidas
```

Os scripts em `python/` usam o venv do repositório irmão:
`..\TCC-autotune-python\.venv\Scripts\python.exe`.

---

## Testes de validação

A equivalência do núcleo de streaming com a versão offline (gold) e a ausência de cliques são
verificadas pelos scripts em `python/`, que comparam a saída causal com a de referência
amostra a amostra.

| Verificação | Script | Resultado atual |
|---|---|---|
| Identidade em `mix = 0` (bypass) | `baseline.sh`, `test_mix` | **bit-perfect** |
| Identidade em `tol = 600` (PSOLA com β = 1) | `baseline.sh` | **bit-perfect** |
| Correlação com o gold, `mix = 1` | `bench_stream.py` | **0,997** (>0,999 por região) |
| Invariância ao tamanho de bloco (64–512) | `bench_stream.py` | **confirmada** |
| Trilha de F0 vs. gold | `bench_pitch.py` | **100 %** |
| Disparo de quadros | `bench_frames.py` | **confirmado** |
| Cliques ("pipoco") | `bench_latencia.py` | **0** |
| Preservação de formantes | `formantes.py` | **confirmada** |

---

## Próximos passos

O backlog priorizado, com ganho, esforço, risco e encaixe no cronograma do TCC, está em
**[`docs/documentacao-tecnica.md` §10](docs/documentacao-tecnica.md#10-backlog-priorizado-e-plano-por-sprint)**.

Resumo das frentes:

- **Latência** — `look=0`, quadro derivado da tessitura e guarda do PSOLA de 2 para 1 período
  levam de 57,9 para **17,3 ms** sem trocar de arquitetura.
- **Naturalidade** — filtrar a *correção* em vez do *alvo* (retune speed), refino de pitch
  dentro do bin, interpolação de F0 entre quadros e mix seco/molhado.
- **Robustez** — janela de re-síntese limitada, buffers pré-alocados (RT-safe) e normalização
  consistente.
- **Funcionalidades** — detecção automática de tonalidade; preset "Low Latency".
