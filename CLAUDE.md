# CLAUDE.md — guia para agentes

Contexto operacional deste repositório. Leia antes de mexer no código.

## O que é

Protótipo de autotune (correção automática de afinação vocal) em C++17, parte prática de um
TCC de Ciência da Computação (PUCRS, 2026). Pipeline: **pYIN** (detecção de pitch) →
**nota-alvo** (quantização 12-TET com zona morta e glide) → **TD-PSOLA** **ou** **ponteiro
móvel (v3)** (dois motores de síntese, o mesmo deslocamento de pitch; só o TD-PSOLA preserva
formantes — o ponteiro troca essa preservação por latência quase nula, ver
[`docs/especificacao-v3-ponteiro.md`](docs/especificacao-v3-ponteiro.md) §8).

Repositório irmão: [`TCC-autotune-python`](https://github.com/gacherubini/TCC-autotune-python)
— estudo comparativo de algoritmos de detecção de pitch (autocorrelação, YIN, pYIN, SWIPE′)
que fundamentou a escolha do pYIN.

## Onde ler antes de agir

| Preciso de… | Leia |
|---|---|
| Entender o sistema inteiro | [`docs/documentacao-tecnica.md`](docs/documentacao-tecnica.md) |
| Saber o que está quebrado hoje | [`docs/teste-de-usuario.md`](docs/teste-de-usuario.md) e §8 do doc técnico |
| Saber o que fazer a seguir | §9 (soluções) e §10 (backlog priorizado) do doc técnico |
| Mexer no motor de tempo real | [`docs/arquitetura-streaming.md`](docs/arquitetura-streaming.md) |
| Saber se algo já foi tentado | [`docs/historico-e-decisoes.md`](docs/historico-e-decisoes.md) |
| Mexer em **qualquer parâmetro do plugin** | [`docs/comparacao-antares.md`](docs/comparacao-antares.md) — as decisões já tomadas estão no histórico |
| **Implementar qualquer coisa** | [`docs/plano-de-implementacao.md`](docs/plano-de-implementacao.md) (o plano) + [`docs/execucao-do-plano.md`](docs/execucao-do-plano.md) (o que já foi feito) |
| **Verificar que não quebrou nada** | `./baseline.sh conferir` — 37 casos, espera `IDENTICO`. Rode **antes e depois** de qualquer mudança |
| Trabalhar no **modo de baixa latência** | [`docs/modo-baixa-latencia.md`](docs/modo-baixa-latencia.md) — ⚠️ há 6 questões em aberto a resolver antes de codar |
| **Entender por que o Auto-Tune tem 0,84 ms** e o que é alcançável aqui | [`docs/pesquisa-latencia-antares.md`](docs/pesquisa-latencia-antares.md) — ⚠️ o plano v1/v2 tem um teto: sem `nFrame` e `look`, sobra a guarda do PSOLA, que é `fs/FMIN` |
| Mexer em **Retune Speed, vibrato ou qualquer controle de expressão** | [`docs/pesquisa-retune-speed-e-cor.md`](docs/pesquisa-retune-speed-e-cor.md) — ⚠️ `Tolerancia` **não** é Flex-Tune, e formante **não** resolve "cor" |
| **Discutir v1/v2/v3 ou citar qualquer número de latência** | [`docs/analise-v1-v2-v3.md`](docs/analise-v1-v2-v3.md) — ⚠️ corrige dois números errados nos outros dois docs; a §8 da especificação está desatualizada |
| Entender a **tela do plugin** | [`plugin/README.md`](plugin/README.md#a-tela-gui-custom-31082026) + [Redesenho da interface](docs/execucao-do-plano.md#redesenho-da-interface-2026-08-31--fora-do-plano) |
| Mexer no **motor v3 / Low Latency** | [`docs/especificacao-v3-ponteiro.md`](docs/especificacao-v3-ponteiro.md) + [Etapa 6 do diário](docs/execucao-do-plano.md#etapa-6--motor-v3-de-ponteiro-móvel-low-latency) |
| Citar qualquer coisa no texto do TCC | [`docs/pesquisa-bibliografica.md`](docs/pesquisa-bibliografica.md) — e confira a [errata](docs/historico-e-decisoes.md#errata--afirmações-corrigidas-pela-pesquisa-bibliográfica-2026-08-26) antes de citar o doc técnico |

**Não reimplemente nada antes de checar `historico-e-decisoes.md`** — vários bugs sutis
(drift de fase do PSOLA, cliques, compressão temporal) já foram caçados e resolvidos, e as
soluções têm justificativa registrada.

## Arquitetura em uma tela

```
src/core/dsp.h                      ← TODA a matemática. Compartilhado, sem cópia, por
                                      offline + causal + streaming + plugin.
                                      Mexer aqui afeta os quatro.
src/offline_causal/main.cpp         → autotune.exe      (offline, Viterbi global = GOLD)
src/offline_causal/autotune_rt.cpp  → autotune_rt.exe   (causal, Viterbi de lag fixo)
src/c1_streaming/autotune_stream.h  ← núcleo de streaming (header-only). O plugin é uma
                                      casca fina em volta disto.
src/c1_streaming/stream_test.cpp    → stream_test.exe   (driver headless)
plugin/                             → VST3 + Standalone (JUCE). Zero DSP próprio.
```

Regra do projeto: **DSP novo entra em `dsp.h` ou em `autotune_stream.h`. O plugin não
implementa algoritmo.**

## Build

```bat
compilar.bat            REM Windows: os 3 executáveis, via g++ (MinGW/scoop).
cd plugin && build.bat  REM Windows: o plugin VST3 — exige MSVC Build Tools 2022, não g++.
```

```sh
./baseline.sh conferir  # macOS/Linux: compila os 3 CLIs + testes e confere a linha de base
cd plugin && ./build.sh # macOS/Linux: o plugin VST3 + Standalone, e valida com o pluginval
```

No Windows são duas toolchains por necessidade: o caminho oficial do JUCE para VST3 lá é MSVC.
No macOS bastam as Command Line Tools + `brew install cmake ninja` — o mesmo `CMakeLists.txt`
serve aos dois. O build do macOS vai para `plugin/build-mac/`, para não colidir com o cache do
Windows. Ver [`docs/execucao-do-plano.md`](docs/execucao-do-plano.md), seção *Etapa 1-bis*.

## Verificação — rode isto depois de mexer em DSP

Os scripts ficam em `python/` e usam o venv do repositório irmão
(`..\TCC-autotune-python\.venv\Scripts\python.exe`). Precisam dos `.exe` compilados.

```bat
python python\bench_stream.py    REM streaming vs. gold + invariância ao tamanho de bloco
python python\bench_pitch.py     REM trilha de F0 do streaming vs. gold
python python\bench_frames.py    REM disparo de quadros do ring buffer
python python\bench_latencia.py  REM latência × qualidade × xRT, e contagem de cliques
```

**Invariantes que não podem quebrar:**

1. **Dois caminhos de identidade, que têm de concordar, nos dois motores** (checados por
   `./baseline.sh`): `mix = 0` (bypass — o motor de síntese roda mas o resultado é descartado) e
   `tol = 600` (tolerância maior que meio semitom → alvo = f0 → **β = 1**, o motor roda em
   identidade e a saída é usada). Os dois devolvem áudio bit-idêntico à entrada, e **um diferir
   do outro é erro de fase no motor**. Até a Etapa 2 esse papel era da `forca = 0`, que fazia as
   duas coisas ao mesmo tempo; ao removê-la, o teste foi desdobrado em dois para não perder
   cobertura. Desde a Etapa 6 o par existe **duplicado**: `mix=0`/`tol=600` para o PSOLA e
   `st_lowlat_mix0`/`st_lowlat_tol600` para o ponteiro (`lowlat=1`).
2. Correlação do streaming com o **causal** (`autotune_rt`) ≥ **0,995** (medido: **0,9981**
   pelo `medir_qualidade.py`, que roda aqui; os 0,9996 que aparecem no diário são do
   `bench_stream.py`, medido noutra máquina — quando os dois discordam, vale o primeiro).
   ⚠️ **Cuidado com o nome:** o `bench_stream.py` chama `autotune_rt` de `_gold.wav`. Ele
   verifica *streaming ≡ causal*, **não** *streaming ≡ offline*. Contra o offline a correlação
   medida em 26/08/2026 é **0,5695** (0,5123 por região vozeada). Ver
   `docs/execucao-do-plano.md`, "Achados de medição".
3. Contagem de "pipoco" (descontinuidades > 30× a mediana) = **0**. ⚠️ Não se reproduz: com
   voz real a própria **entrada** pontua ~2900 por esse critério. O limiar relativo mede
   conteúdo de alta frequência, não clique. Com limiar absoluto (|Δ| > 0,25) dá **13/13/12**
   (offline/causal/streaming) contra **2916 da entrada** — o invariante defensável é *ficar
   abaixo da entrada*, não "= 0". Ver "Achados de medição".
4. A saída é **idêntica para qualquer tamanho de bloco** do host, **nos dois motores**. Estava
   **quebrada** acima de `nHop` (256) até 26/08/2026 — e a afirmação nunca tinha sido testada,
   porque o `baseline.sh` rodava `block=64` e `block=512` mas não comparava um com o outro. Hoje
   compara, e no PSOLA a invariância é estrutural (ver `avancarPsola()`), não empírica —
   verificada de 1 a 4096. No motor de ponteiro é estrutural por construção: `processar()` é
   chamado uma vez por amostra dentro do laço, então a decisão que ele lê nunca depende de onde
   o host corta o bloco; verificada com `st_lowlat_block64` == `st_lowlat_block512`.

## Armadilhas conhecidas

- **`W_TRANS` e `SIGMA_TRANS` estão em bins, não em cents.** Mudar `RES_CENTS` sem reescalar
  esses dois muda o modelo probabilístico do HMM. Ver doc técnico §9.2 C2.
- **O Create Vibrato existe no DSP, nos CLIs e no APVTS, mas *não* na GUI — de propósito.**
  Os quatro widgets (`Create Vib`, `Vib Rate`, `Vib Depth`, `Vib Amp`) foram retirados do editor
  em 31/08/2026: é um **gerador** num protótipo que se declara **corretor**. Os parâmetros
  continuam automatizáveis pelo host e alcançáveis por `vibforma=`/`vibtaxa=`/`vibprof=`/`vibamp=`.
  Não é widget esquecido — não "conserte". Ver
  [Decisão 8](docs/historico-e-decisoes.md#decisão-8--create-vibrato-sai-da-interface-fica-no-dsp-2026-08-31).
- **Duas fórmulas de latência divergentes.** `autotune_rt.cpp` usa `1·fs/FMIN + block`;
  `autotune_stream.h` usa `2·fs/FMIN` sem bloco. Padronizar antes de citar números.
- **A latência declarada com Low Latency é só a parte fixa.** A parte variável (distância entre
  ponteiros, 0..T) não entra no `setLatencySamples`. Ver
  [`docs/especificacao-v3-ponteiro.md` §3.4](docs/especificacao-v3-ponteiro.md) e a
  [Etapa 6 do diário](docs/execucao-do-plano.md#etapa-6--motor-v3-de-ponteiro-móvel-low-latency).
- **`psolaSintetiza()` normaliza por pico.** No streaming ela roda uma vez por janela, então
  janelas diferentes podem receber ganhos diferentes. Ver §8.3, Achado 3.
- **O streaming aloca dentro do callback de áudio** (`push_back` por amostra em `xAll`,
  `f0samp`, `foutSamp`, `outBuf`). Viola a regra RT-safe. Ver §8.3, Achado 2.
- **A janela de re-síntese cresce sem limite** durante notas longas (`autotune_stream.h:481`
  recua até o início da região vozeada). Custo quadrático por frase. Ver §8.3, Achado 1.
- **`*.wav` está no `.gitignore`.** Áudio gerado fica fora do repo (exceção versionada:
  `exemplo-antes.wav`).

## Texto do TCC — não está aqui

O LaTeX do trabalho vive em **repositório próprio**:
[`gacherubini/TCC-TEXT`](https://github.com/gacherubini/TCC-TEXT) (localmente em
`../TCC/`). Este repositório é **só o código**.

O Overleaf (conta gratuita, sem integração Git) é onde o autor escreve; a sincronização é
via ZIP com o `sync-overleaf.sh` **daquele** repositório. **Não edite o texto do TCC sem
avisar que isso vai precisar ser subido manualmente no Overleaf** — e, sobretudo, **não
commite o texto**: o autor revisa e commita a redação ele mesmo.

O que este repositório oferece ao texto é a **documentação técnica** de `docs/`, que é o
material de apoio para a redação: números medidos, registro do teste de usuário e histórico
de decisões.

## Problemas em aberto

O teste com usuário reprovou dois requisitos não funcionais:

| | Situação | Meta |
|---|---|---|
| **Latência** | ✅ **0,18 ms fixos com Low Latency** (+ 0..T variável, não declarada ao host) — **falta a escuta**. Com o PSOLA (padrão) continua em 71,4 ms / 57,9 ms (ver ressalva) | ver ressalva |
| **Naturalidade** | endereçada pelas Etapas 3–5 (Retune Speed, Humanize, Natural Vibrato) — **falta a escuta** | vibrato preservado, ataque com glide |

> ⚠️ **Latência sempre tem de ser citada junto com o FMIN**, senão o número não quer dizer
> nada: a guarda do PSOLA é proporcional a `fs/FMIN`. Os 57,9 ms que circulam no texto são do
> preset contralto.
>
> ⚠️ **A meta "≤ 20 ms" não tem respaldo revisado por pares** — ver a
> [errata](docs/historico-e-decisoes.md#errata--afirmações-corrigidas-pela-pesquisa-bibliográfica-2026-08-26).
> A literatura de monitoração vocal é mais exigente: 7–13 ms para o limiar de coloração
> tímbrica (Marentakis et al.), e da ordem de 1 ms para avaliação "boa" com fone
> intra-auricular (Lester e Boley).

Diagnóstico completo em §8 do doc técnico; soluções em §9; ordem de ataque em §10. O estado
da naturalidade está em [`docs/execucao-do-plano.md`](docs/execucao-do-plano.md).

## Convenções

- **Idioma:** código, comentários e documentação em **português**. Mensagens de commit em
  **inglês**.
- **Comentários:** o código é didático por decisão de projeto (é um TCC) — comentários longos
  explicando o *porquê* são a norma, não ruído. Mantenha o estilo ao editar.
- **Nomes:** identificadores em português (`notaAlvo`, `misturar`, `lpAlvo`, `tinhaNota`).
- **Sem dependências novas** sem necessidade: hoje são só `dr_wav.h` (header-only) e o JUCE
  (só no plugin, via FetchContent).
