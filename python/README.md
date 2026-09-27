# Scripts de medição

Dez scripts, e **eles não rodam todos no mesmo lugar** — é a primeira coisa que se precisa
saber antes de citar qualquer número que saia daqui.

## Onde cada um roda

| Script | Ambiente | O que mede |
|---|---|---|
| **`medir_qualidade.py`** | 🟢 **qualquer máquina** | linha de base de qualidade dos três caminhos: latência, xRT, correlação entre caminhos, trilha de F0, contagem de cliques, invariância ao tamanho de bloco |
| **`medir_formante_resample.py`** | 🟢 **qualquer máquina** | quanto de formante a reamostragem deslocaria — decide a viabilidade do motor de baixa latência (v3) |
| **`medir_v3.py`** | 🟢 **qualquer máquina** | PSOLA × Ponteiro × Low Latency (Etapa 6): latência fixa e variável, erro de afinação estável e de ataque, contagem de degraus — a tabela usada na Etapa 6 do [diário](../docs/execucao-do-plano.md#etapa-6--motor-v3-de-ponteiro-móvel-low-latency) |
| `formantes.py` | 🟢 qualquer máquina | preservação de formantes por envelope cepstral, entrada × saída |
| `bench_stream.py` | 🟡 Windows | streaming × causal e invariância ao tamanho de bloco |
| `bench_pitch.py` | 🟡 Windows | trilha de F0 do streaming × causal |
| `bench_frames.py` | 🟡 Windows | disparo de quadros do ring buffer |
| `bench_latencia.py` | 🟡 Windows | varredura de look-ahead: latência × qualidade × xRT |
| `bench_nframe.py` | 🟡 Windows | varredura de `N_FRAME`: piso de latência × `fmin` detectável |
| `bench_fmin.py` | 🟡 Windows | varredura de tessitura: latência × notas perdidas |

Os **`bench_*.py`** foram escritos para Windows: procuram `./autotune_rt.exe` no diretório
corrente, usam um `audioteste.wav` que **não está versionado** e dependem do venv do
repositório irmão (`..\TCC-autotune-python\.venv\Scripts\python.exe`). Nenhum deles roda numa
máquina limpa.

Os **`medir_*.py`** foram escritos depois, justamente para isso: compilam o que precisam, usam
o `exemplo-antes.wav` que **está** no repo, e não dependem de `.exe` nem de Windows.

> ⚠️ **Os números de correlação citados no README da raiz vieram dos `bench_*`, medidos em
> outra máquina.** Eles não foram reproduzidos aqui. Os números do `medir_qualidade.py` são os
> atuais e conferíveis — e quando os dois discordam, vale o segundo. Ver
> [`../docs/execucao-do-plano.md`](../docs/execucao-do-plano.md), "Achados de medição".

## Ambiente

Os scripts portáveis precisam de `numpy` e `soundfile`. Há um venv local, fora do
versionamento:

```sh
python3 -m venv .venv                     # na raiz do repositório
.venv/bin/pip install numpy soundfile

.venv/bin/python python/medir_qualidade.py
.venv/bin/python python/medir_formante_resample.py
.venv/bin/python python/medir_v3.py
```

`medir_qualidade.py` e `medir_formante_resample.py` aceitam `--wav` para trocar o material e
`--bin DIR` para pular a compilação e usar binários já prontos; o `medir_qualidade.py` aceita
ainda `--fonte` para medir uma árvore de código diferente da do repositório — útil para comparar
contra uma versão anterior. O `medir_v3.py` aceita só `--wav` (compila os binários que precisa
num diretório temporário, sempre).

## O que é verificação e o que é medição

Não confundir os dois papéis, porque só um deles falha quando algo quebra:

- **`../baseline.sh` é a verificação.** Roda 37 casos de áudio, 8 invariantes e os 19 testes
  legados, e responde `IDENTICO — nada mudou`. É o que se roda **antes e depois** de
  qualquer mudança em DSP. Ele compila e executa as suítes de `../src/tests/` junto.
- **Os scripts daqui são a medição.** Produzem números para o texto do TCC. Não têm critério
  de aprovação — descrevem o estado atual.

Um baseline verde não diz que o sistema está bom; diz que ele não mudou.
