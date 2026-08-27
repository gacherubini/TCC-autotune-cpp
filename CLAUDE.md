# CLAUDE.md — guia para agentes

Contexto operacional deste repositório. Leia antes de mexer no código.

## O que é

Protótipo de autotune (correção automática de afinação vocal) em C++17, parte prática de um
TCC de Ciência da Computação (PUCRS, 2026). Pipeline: **pYIN** (detecção de pitch) →
**nota-alvo** (quantização 12-TET com zona morta e glide) → **TD-PSOLA** (deslocamento de
pitch preservando formantes).

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
| **Verificar que não quebrou nada** | `./baseline.sh conferir` — 17 casos, espera `IDENTICO`. Rode **antes e depois** de qualquer mudança |
| Trabalhar no **modo de baixa latência** | [`docs/modo-baixa-latencia.md`](docs/modo-baixa-latencia.md) — ⚠️ há 6 questões em aberto a resolver antes de codar |
| **Entender por que o Auto-Tune tem 0,84 ms** e o que é alcançável aqui | [`docs/pesquisa-latencia-antares.md`](docs/pesquisa-latencia-antares.md) — ⚠️ o plano v1/v2 tem um teto: sem `nFrame` e `look`, sobra a guarda do PSOLA, que é `fs/FMIN` |
| Mexer em **Retune Speed, vibrato ou qualquer controle de expressão** | [`docs/pesquisa-retune-speed-e-cor.md`](docs/pesquisa-retune-speed-e-cor.md) — ⚠️ `Tolerancia` **não** é Flex-Tune, e formante **não** resolve "cor" |
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

1. **Dois caminhos de identidade, que têm de concordar** (checados por `./baseline.sh`):
   `mix = 0` (bypass — o PSOLA roda mas o resultado é descartado) e `tol = 600` (tolerância
   maior que meio semitom → alvo = f0 → **β = 1**, o PSOLA roda em identidade e a saída é
   usada). Os dois devolvem áudio bit-idêntico à entrada, e **um diferir do outro é erro de
   fase no PSOLA**. Até a Etapa 2 esse papel era da `forca = 0`, que fazia as duas coisas ao
   mesmo tempo; ao removê-la, o teste foi desdobrado em dois para não perder cobertura.
2. Correlação do streaming com o **causal** (`autotune_rt`) ≥ **0,995** (medido: 0,9996).
   ⚠️ **Cuidado com o nome:** o `bench_stream.py` chama `autotune_rt` de `_gold.wav`. Ele
   verifica *streaming ≡ causal*, **não** *streaming ≡ offline*. Contra o offline a correlação
   medida em 26/08/2026 é **0,5695** (0,5123 por região vozeada). Ver
   `docs/execucao-do-plano.md`, "Achados de medição".
3. Contagem de "pipoco" (descontinuidades > 30× a mediana) = **0**. ⚠️ Não se reproduz: com
   voz real a própria **entrada** pontua ~2900 por esse critério. O limiar relativo mede
   conteúdo de alta frequência, não clique. Com limiar absoluto (|Δ| > 0,25) dá **13/13/12**
   (offline/causal/streaming) contra **2916 da entrada** — o invariante defensável é *ficar
   abaixo da entrada*, não "= 0". Ver "Achados de medição".
4. A saída é **idêntica para qualquer tamanho de bloco** do host. Estava **quebrada** acima de
   `nHop` (256) até 26/08/2026 — e a afirmação nunca tinha sido testada, porque o `baseline.sh`
   rodava `block=64` e `block=512` mas não comparava um com o outro. Hoje compara, e a
   invariância é estrutural (ver `avancarPsola()`), não empírica. Verificada de 1 a 4096.

## Armadilhas conhecidas

- **`W_TRANS` e `SIGMA_TRANS` estão em bins, não em cents.** Mudar `RES_CENTS` sem reescalar
  esses dois muda o modelo probabilístico do HMM. Ver doc técnico §9.2 C2.
- **Duas fórmulas de latência divergentes.** `autotune_rt.cpp` usa `1·fs/FMIN + block`;
  `autotune_stream.h` usa `2·fs/FMIN` sem bloco. Padronizar antes de citar números.
- **`psolaSintetiza()` normaliza por pico.** No streaming ela roda uma vez por janela, então
  janelas diferentes podem receber ganhos diferentes. Ver §8.3, Achado 3.
- **O streaming aloca dentro do callback de áudio** (`push_back` por amostra em `xAll`,
  `f0samp`, `foutSamp`, `outBuf`). Viola a regra RT-safe. Ver §8.3, Achado 2.
- **A janela de re-síntese cresce sem limite** durante notas longas (`autotune_stream.h:481`
  recua até o início da região vozeada). Custo quadrático por frase. Ver §8.3, Achado 1.
- **`*.wav` está no `.gitignore`.** Áudio gerado fica fora do repo (exceção versionada:
  `exemplo-antes.wav`). O texto do TCC **é** versionado, em `tcc-texto/` — só os artefatos de
  compilação do LaTeX (`.aux`, `.bbl`, `.pdf`…) ficam de fora.

## Texto do TCC

O LaTeX está em `tcc-texto/`, versionado. O Overleaf (conta gratuita, sem integração Git) é
onde o autor escreve; a sincronização é via ZIP com `tcc-texto/sync-overleaf.sh` — veja
`tcc-texto/README.md`. **Não edite `tcc-texto/` sem avisar que isso vai precisar ser subido
manualmente no Overleaf** (`./tcc-texto/sync-overleaf.sh --alterados` lista o que subir).

## Problemas em aberto

O teste com usuário reprovou dois requisitos não funcionais:

| | Situação | Meta |
|---|---|---|
| **Latência** | **71,4 ms** com o FMIN padrão (80 Hz); 57,9 ms com `voz=contralto` (FMIN 175 Hz) | ver ressalva |
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
