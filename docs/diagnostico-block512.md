# Diagnóstico — a saída do streaming **não** é invariante ao tamanho de bloco

**Data:** 2026-08-26 · **Escopo:** investigação read-only, nenhum arquivo do repositório foi
alterado. Toda a instrumentação foi feita sobre uma cópia dos fontes em `scratchpad/`,
tirada do commit `a615444` (`git show HEAD:…`), porque `src/core/dsp.h` estava sendo editado
em paralelo. Os números de linha citados são os de `a615444`.

---

## 1. Causa raiz

> **A cadeia de marcas do PSOLA depende de quantas amostras existem à ESQUERDA da região
> vozeada dentro da janela re-sintetizada, e `avancarPsola()` não produz esse "à esquerda"
> de forma canônica: ele o herda de `synthFront`, que avança em passos do tamanho do bloco
> do host.**

Mecanicamente, são duas peças que se encontram:

**Peça A — `dsp.h:321`, dentro de `psolaSintetiza()`.** A busca por correlação que refina
cada marca de período é abortada nas bordas da janela:

```cpp
if (c - Wc < 0 || c + Wc >= N || m - Wc < 0 || m + Wc >= N) continue;   // dsp.h:321
```

O termo `m - Wc < 0` compara a marca com o **início da janela** (`0`), com
`Wc = T/2` (meio período). Para a **primeira** marca de uma região vozeada, `m` fica a
menos de um período do início da região; se a janela começar em cima da região (ou muito
perto dela), **todos** os deslocamentos são descartados, o laço não avalia nada, e a marca
seguinte cai no fallback `pos = cand = m + T` — a posição quantizada pelo período, sem
refinamento. Se houver mais alguns milissegundos de contexto à esquerda, a correlação roda
e devolve uma posição diferente. Essa diferença de 1–7 amostras **propaga por toda a cadeia
de marcas da região**, deslocando todos os grãos: é diferença de *forma de onda*, não de ganho.

**Peça B — `autotune_stream.h:481` + `:493`.** O início da janela é

```cpp
long long winStart = std::max(0LL, synthFront - margem);          // :481  (margem = nFrame)
while (winStart > 0 && f0samp[winStart - 1] > 0.0f) --winStart;   // :493
```

O recuo de `:493` só garante uma coisa: *"`winStart` não está no MEIO de uma região vozeada"*.
Ele **não** torna `winStart` canônico. Quando `synthFront - nFrame` cai no silêncio **antes**
de uma região vozeada, o `while` não faz nada e `winStart` fica exatamente onde `synthFront`
o deixou — e `synthFront` avança em passos que dependem do tamanho de bloco do host.

Resultado: o mesmo bloco de 256 amostras de saída é sintetizado a partir de janelas com
quantidades diferentes de contexto à esquerda, e a Peça A converte essa diferença em marcas
diferentes.

### Evidência direta (as duas regiões, medidas)

`probe2.cpp` chama `psolaSintetiza()` nas duas janelas reais e imprime a cadeia de marcas:

**Região 1 — commit `outBuf[13234, 13490)` (saída `[16384, 16639]`, t≈0,372 s).**
Região vozeada começa na amostra 12032, F0 ≈ 335 Hz, T = 132, `Wc` = 66.

| | `winStart` | âncora (janela) | 2ª marca | cadeia (absoluta) |
|---|---|---|---|---|
| block ≤ 256 | **12032** | m = 46 → `46 - 66 < 0` → **0 candidatos avaliados** | `cand` = 178 | 12078 **12210** 12343 12474 … |
| block = 512 | **11954** | m = 124 → 67 candidatos avaliados | corr → 258 | 12078 **12212** 12345 12476 … |

**Região 2 — commit `outBuf[196018, 196274)` (saída `[199168, 199423]`, t≈4,516 s).**
Região vozeada começa em 195072, F0 ≈ 197 Hz, T = 224, `Wc` = 112.

| | `winStart` | âncora (janela) | 2ª marca | cadeia (absoluta) |
|---|---|---|---|---|
| block ≤ 256 | **194994** | m = 84 → `84 - 112 < 0` → **0 candidatos** | `cand` = 308 | 195078 **195302** 195525 … |
| block = 512 | **194738** | m = 340 → 113 candidatos | corr → 571 | 195078 **195309** 195531 … |

Nos dois casos o `winEnd` é o **mesmo** (14592 e 197376). A única variável é `winStart`.
Re-sintetizando as duas janelas e comparando só o miolo cometido:
254/256 e 255/256 amostras diferentes — exatamente as regiões medidas no WAV.

### O que fica descartado (com evidência)

- **"Achado 3" de `docs/documentacao-tecnica.md` §8.3 — normalização por pico.** Instrumentei
  `dsp.h:305`: em `exemplo-antes.wav`, `psolaSintetiza()` é chamada 850 vezes (block=64) /
  426 vezes (block=512) e a normalização `pico > 1.0` dispara **0 vezes**. Ela não pode
  explicar nada aqui. (E a razão amostra-a-amostra não é constante dentro da região, como o
  enunciado já observou.)
- **Análise / detecção de pitch.** Ver §2.
- **A lógica de "troco" (`desdeUltimo -= p.nHop`) e o caso `primeiroQuadro`** (`:261-262`).
  Estão corretos: os dumps de quadros são bit-idênticos entre block=64 e block=512.
- **O `psolaGuard` / borda direita da janela.** Com a correção proposta (§5), block=512
  continua usando `winEnd` diferente de block=64 em cada passo e mesmo assim a saída fica
  bit-idêntica — a folga de 2·fs/FMIN cumpre o que promete **na borda direita**. O bug é
  inteiramente na borda **esquerda**, que ninguém tinha orçado.

---

## 2. A bifurcação: análise × síntese

**A divergência nasce inteiramente na SÍNTESE. A decisão de pitch é bit-idêntica.**

```
$ diff fr_64.txt fr_512.txt   →  IGUAL   (858 quadros disparados, mesmos índices)
$ diff f0_64.txt f0_512.txt   →  IGUAL   (854 quadros emitidos, mesmo F0 em 4 casas)
$ md5 out_64.wav out_512.wav  →  DIFERENTE
```

Como `f0samp`/`foutSamp` são função determinística da trilha de F0 e do estado do
`CorretorAltura` (ambos idênticos), a entrada de `psolaSintetiza()` — sinal, F0 por amostra,
alvo por amostra — é a mesma nos dois casos. **Muda só o recorte da janela.** É reconstrução
diferente com a mesma decisão de pitch.

---

## 3. Por que em block=512 e não em 64/128/256 — e por que só duas regiões

### O limiar é X = `nHop` = 256

`avancarPsola()` roda **uma vez por chamada de `process()`**, e `alvo = f0samp.size() - guarda`.
`f0samp` só cresce quando um quadro é *emitido*, ou seja, em passos de exatamente `nHop = 256`
amostras de entrada.

- **block ≤ nHop:** no máximo uma emissão de quadro cai dentro de uma chamada. As chamadas
  intermediárias saem no `return` da linha 475 (`alvo <= synthFront`). Logo `synthFront`
  percorre **sempre** a grade `k·nHop − psolaGuard`, seja o bloco 1, 32, 64, 100, 128, 192,
  255 ou 256 — e `winStart` é o mesmo em todos. Verificado: **os 8 tamanhos ≤ 256 dão o mesmo
  MD5**.
- **block > nHop:** duas (ou mais) emissões podem cair na mesma chamada, `synthFront` salta
  512 de uma vez, a grade perde metade dos pontos e os `winStart` deslocam-se de 256.

Confirmação empírica (mesmo WAV, `mix=1`, cromático):

| block | 1 | 32 | 64 | 100 | 128 | 192 | 255 | 256 | 257 | 300 | 320 | 384 | 511 | 512 | 513 | 1024 | 2048 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| igual a 64? | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✘ | ✘ | ✔ | ✘ | ✘ | ✘ | ✘ | ✘ |

`block > 256` é **necessário**, não suficiente: 257 e 384 também fazem commits duplos
(3 e 283 deles), mas por acaso não caem em cima das duas passagens sensíveis — o traço mostra
que ambos cobrem `synthFront=13234` e `196018` com o mesmo `winStart` de block=64.
Já `block=300` acerta uma delas (e uma terceira em t≈4,081 s) e diverge.

### Por que só DUAS regiões em 5 segundos

Para divergir é preciso a conjunção de três coisas:

1. um commit em que `synthFront` saltou mais que `nHop` (só com block > 256);
2. que esse commit seja justamente o **primeiro** cujo `synthFront - nFrame` cai perto do
   início de uma região vozeada — há no máximo **um** commit assim por ataque de nota;
3. que os dois `winStart` fiquem em **lados opostos** do limiar `m - Wc < 0`, isto é, um com
   menos de `T/2` de contexto e outro com mais. Com T entre 130 e 224 amostras, a janela de
   sensibilidade tem só ~65–112 amostras de largura, contra passos de 256.
4. e ainda que a correlação, quando roda, devolva algo **diferente** de `cand` (nas duas
   regiões devolveu `cand+2` e `cand+7`; quando devolve `cand` exato, não há divergência).

O arquivo tem poucos ataques de nota e a probabilidade de a grade de commits cair na janela
certa é baixa: dois acertos. **Não é um caso de borda raro — é o mesmo defeito, disparando
onde o alinhamento permite.** Com outro áudio, outro `hop`, outro `look` ou outro bloco, o
número de regiões muda (block=300 dá três).

O tamanho ser exatamente **256** é consequência: o miolo cometido de cada passo da grade é
`nHop = 256` amostras.

---

## 4. Repro mínimo

```sh
SCR=/private/tmp/claude-501/-Users-gabrielabreucherubini-Documents-codigo-autotune/05f1053e-69a7-4aee-a7ee-6b7074f0d7b5/scratchpad
cd /Users/gabrielabreucherubini/Documents/codigo/autotune/TCC-autotune-cpp

clang++ -std=c++17 -O2 -I external src/c1_streaming/stream_test.cpp -o $SCR/stream_test

$SCR/stream_test exemplo-antes.wav $SCR/out_64.wav  1.0 crom block=64  \
    dumpf0=$SCR/f0_64.txt  dumpframes=$SCR/fr_64.txt
$SCR/stream_test exemplo-antes.wav $SCR/out_512.wav 1.0 crom block=512 \
    dumpf0=$SCR/f0_512.txt dumpframes=$SCR/fr_512.txt

diff $SCR/fr_64.txt $SCR/fr_512.txt   # vazio  -> análise idêntica
diff $SCR/f0_64.txt $SCR/f0_512.txt   # vazio  -> pitch idêntico
md5 -q $SCR/out_64.wav $SCR/out_512.wav   # DIFERENTES -> só a síntese divergiu
```

Localização das regiões (`$SCR/cmp.py`, roda com `$SCR/venv/bin/python`):

```
N= 220500 difs= 457
  regiao [16384,16639]   len=256  t=0.3715s .. 0.3773s  razao min/med/max = 0.840/0.999/1.299
  regiao [199168,199423] len=256  t=4.5163s .. 4.5221s  razao min/med/max = 0.762/1.007/3.000
```

Artefatos de instrumentação deixados no scratchpad (nenhum toca o repositório):

| arquivo | o que faz |
|---|---|
| `src/` | cópia de `a615444` de `dsp.h`, `autotune_stream.h`, `stream_test.cpp` |
| `src/c1_streaming/autotune_stream_dbg.h` | cópia com trace de `(synthFront, alvo, winStart, winEnd)` |
| `probe1.cpp` | roda o motor num bloco dado, grava o trace e despeja `xAll/f0samp/foutSamp` |
| `probe2.cpp` | re-executa `psolaSintetiza()` nas duas janelas e imprime a cadeia de marcas |
| `cmp.py` | localiza e caracteriza as regiões divergentes entre dois WAVs |
| `fixE2/`, `fixE/`, `fixC/`, `fixB/` | as correções candidatas, compiladas e medidas |

---

## 5. Correção proposta (não aplicada)

### ✅ Recomendada — **E+ : cometer em passos canônicos de `nHop`, com janela função pura de `synthFront`**

Em `avancarPsola()`, trocar o commit único por um laço em passos de no máximo `nHop`, e
derivar `winEnd` de `alvo` em vez de `decis`:

```cpp
long long alvoFinal = decis - guarda;
if (alvoFinal <= synthFront) return;
while (synthFront < alvoFinal) {
    long long alvo   = std::min(alvoFinal, synthFront + (long long)p.nHop);
    /* ... margem / winStart / recuo, exatamente como hoje ... */
    long long winEnd = alvo + guarda;        // em vez de 'decis'
    /* ... psolaSintetiza + commit ... */
    synthFront = alvo;
}
```

Com isso, **toda** chamada a `psolaSintetiza()` passa a ser função exclusiva de `synthFront`,
e `synthFront` percorre a grade `k·nHop − psolaGuard` qualquer que seja o bloco. A invariância
deixa de ser empírica e passa a ser estrutural.

Medido (18 tamanhos de bloco, de 1 a 4096, incluindo 300/320/511/513 que hoje divergem):

- **todos bit-idênticos entre si**;
- **e bit-idênticos à saída atual de block=64/128/256** → `./baseline.sh conferir` continua
  `IDENTICO` nos 17 casos, sem regravar referência nenhuma;
- identidades `mix=0` e `tol=600` preservadas em block=512;
- **latência inalterada** (3150 amostras: 1024 + 4·256 + 1102);
- **custo:** blocos grandes perdem a economia que hoje têm por acidente. Medido em
  `exemplo-antes.wav` (5 s), média de 3 execuções:

  | block | hoje | com E+ |
  |---|---|---|
  | 64 | 1,47 s | 1,47 s |
  | 256 | 1,49 s | 1,51 s |
  | 512 | 0,87 s | 1,50 s |
  | 1024 | 0,54 s | 1,48 s |

  Ou seja: o custo passa a ser **uniforme e igual ao do pior caso de hoje** (o de block=64,
  que é o caso que o plugin tem de aguentar de qualquer jeito). Nada regride em relação ao
  pior caso já suportado — o que se perde é um desconto que só existia porque o motor estava
  errado. Combina bem com o *Achado 1* de §8.3: quem resolver a janela quadrática resolve
  este custo junto.

- **risco:** baixo. A mudança é local (≈6 linhas em `autotune_stream.h`), não toca `dsp.h`,
  não muda a latência e não muda uma única amostra do áudio hoje considerado correto.

### Alternativa C — corrigir o viés de borda dentro do `psolaSintetiza()`

Limitar a busca por correlação à própria região vozeada, em vez de à janela:

```cpp
const long long regIni = nn;                      // início da região, capturado em dsp.h:310
...
if (c - Wc < regIni || c + Wc >= N || m - Wc < regIni || m + Wc >= N) continue;   // :321
```

Isso é, argumentavelmente, o DSP **mais correto** — correlacionar através de uma fronteira
vozeada/não-vozeada não significa nada — e torna a cadeia de marcas de uma região função
apenas dessa região.

- **Medido:** também dá invariância total (9 tamanhos de bloco testados, todos com o mesmo MD5),
  e a **custo zero** de CPU.
- **Custo real:** mexe em `dsp.h`, portanto muda o áudio dos **quatro** caminhos (offline/gold,
  causal, streaming, plugin). Contra a saída atual: 3858 amostras diferentes (1,75 %), em 5
  regiões (todas ataques de nota), correlação 0,999995. As identidades `mix=0` e `tol=600`
  sobrevivem (com β = 1 as posições das marcas não importam), mas **as 17 referências do
  `baseline.sh` teriam de ser regravadas** e a correlação com o gold, remedida.
- **Risco:** médio — perde-se a linha de base como rede de proteção justamente na mudança
  que a está alterando.

### Alternativa B — canonizar `winStart` (avançar até o próximo ataque quando cair no silêncio) — **REJEITADA, testada**

Ideia: quando `winStart` cai em trecho não-vozeado, avançar até o início da próxima região
vozeada, para que a fronteira seja sempre estrutural. Implementei e medi: **não resolve**.
Block 64/128/256/300 concordam entre si, mas 320, 384, 512, 1024 e 2048 dão cinco MD5
*diferentes* — e o áudio de block=64 muda. O motivo é que quando `synthFront` já está em
trecho não-vozeado não há ataque para onde avançar, e `winStart = synthFront` volta a ser
dependente do bloco. Fica registrado como caminho já descartado.

### Recomendação

**E+ agora** (barata, sem regressão, sem regravar linha de base), e **C depois**, junto com
a refatoração do *Achado 1*, se a equipe quiser que a invariância também valha para uma
implementação futura que não comete em grade de `nHop`. E+ e C são compatíveis.

---

## 6. Frases substitutas para README.md e CLAUDE.md

A afirmação atual é falsa como escrita. Enquanto a correção não entrar:

**`CLAUDE.md`, "Invariantes que não podem quebrar", item 4** — substituir

> 4. A saída é **idêntica para qualquer tamanho de bloco** do host (64, 128, 256, 512).

por:

> 4. A saída é **idêntica para qualquer tamanho de bloco do host até `nHop` (256) amostras** —
>    de 1 a 256 o resultado é bit-a-bit o mesmo. **Acima de `nHop` a invariância está
>    quebrada:** quando duas emissões de quadro caem na mesma chamada de `process()`,
>    `avancarPsola()` comete um trecho maior de uma vez e a janela de re-síntese ganha um
>    `winStart` diferente, o que muda a cadeia de marcas do PSOLA no ataque de algumas notas.
>    Medido em `exemplo-antes.wav`: `block=512` difere em 457 de 220500 amostras, em duas
>    regiões de 256 amostras (t≈0,372 s e t≈4,516 s); `block=300`, `320`, `511`, `513`,
>    `1024` e `2048` também divergem. É diferença de **forma de onda**, não de ganho — a
>    hipótese da normalização por pico (§8.3, Achado 3) está **descartada por medição**: ela
>    não dispara nenhuma vez neste material.

**`README.md`, tabela de verificações (linha 204)** — substituir

> \| Invariância ao tamanho de bloco (64–512) \| `bench_stream.py` \| **confirmada** \|

por:

> \| Invariância ao tamanho de bloco \| `bench_stream.py` \| **parcial** — bit-perfect até
> `block = hop` (256); acima disso diverge (block = 512: 457 de 220500 amostras, 2 regiões) \|

**Sugestão adicional:** `docs/arquitetura-streaming.md` (bloco "Por que 2·fs/FMIN") afirma que
a folga garante que "a saída do streaming reproduz a do lote". Vale acrescentar que a folga
protege apenas a **borda direita** da janela de re-síntese; a borda **esquerda** (o
`winStart`) não tem folga equivalente, e é de lá que vem esta quebra.
