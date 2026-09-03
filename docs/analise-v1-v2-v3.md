# v1, v2 e v3 — o que cada um troca, e duas correções de número

> **Data:** 2026-08-31
> **Status:** 📐 **ANÁLISE — a decisão que ela pedia já foi tomada.** A rota escolhida foi a
> **v3**, implementada na Etapa 6 em 2026-09-02 (ver a atualização ao fim deste cabeçalho). O
> corpo do documento foi escrito **antes** dessa escolha e está preservado como registro do
> raciocínio que levou a ela: onde a §9 diz que a decisão segue em aberto, leia **"em aberto em
> 31/08"**. Este documento não substitui a [especificação](modo-baixa-latencia.md) nem a
> [pesquisa](pesquisa-latencia-antares.md); ele **corrige dois números dos dois** e acrescenta o
> enquadramento que faltava.
> **Leia antes:** [modo-baixa-latencia.md](modo-baixa-latencia.md) ·
> [pesquisa-latencia-antares.md](pesquisa-latencia-antares.md)
>
> ### Por que este documento existe
>
> A especificação é de 26/08 e conhecia dois caminhos. A pesquisa é de 27/08 e abriu um
> terceiro. **Ninguém reescreveu a §8 da especificação depois disso**, então as "questões em
> aberto" que bloqueiam a implementação ainda são as de um mundo com dois caminhos. Além disso,
> ao conferir a mecânica dos dois documentos apareceram dois números errados, ambos do lado
> otimista.
>
> **Atualização 2026-09-02:** a v3 foi especificada em
> [especificacao-v3-ponteiro.md](especificacao-v3-ponteiro.md) e implementada — Etapa 6 do
> [diário](execucao-do-plano.md), Decisão 9 do [histórico](historico-e-decisoes.md). Os números
> deste documento continuam valendo como projeção; os medidos estão na Etapa 6.

---

## 1. Quem faz o quê — o enquadramento que faltava

Toda a discussão fica confusa enquanto se acreditar que o TD-PSOLA corrige a afinação. **Ele não
corrige.** O pipeline tem três estágios independentes:

| | Estágio | Quem | Pergunta que responde | Entrega |
|---|---|---|---|---|
| 1 | **Detectar** | pYIN + Viterbi | que nota está sendo cantada? | `f0` |
| 2 | **Decidir** | `notaAlvo()` + `CorretorAltura` (`dsp.h`) | que nota **deveria** ser? | `f_alvo` |
| 3 | **Executar** | TD-PSOLA | como esticar o sinal nessa proporção? | áudio |

Entre o estágio 2 e o 3 passa **um número só**, e está literal em `dsp.h:585`:

```cpp
auto betaDe = [&](size_t idx) {
    double fsrc  = f0samp[a];      // estagio 1
    double falvo = foutSamp[a];    // estagio 2
    return (fsrc > 0) ? falvo / fsrc : 1.0;   // beta
};
```

O estágio 3 recebe `β` pronto e não sabe de onde ele veio. **Consequência que decide o
planejamento inteiro:** trocar o motor de síntese muda *como* o esticamento é feito, não
*quanto*. A nota de saída é idêntica, porque `β` é idêntico. Escala, tolerância, Retune Speed,
Natural Vibrato e Humanize vivem todos no estágio 2 e não são tocados.

O que muda ao trocar o estágio 3 são três efeitos colaterais, e é sobre eles que a decisão gira:
formantes, latência e o tipo de artefato.

---

## 2. O que cada versão troca

| | v1 | v2 | v3 |
|---|---|---|---|
| Estágio afetado | 1 (parâmetros) | 1 (algoritmo) | **3 (algoritmo)** |
| pYIN | fica | recursivo em vez de por quadro | **fica, inalterado** |
| `CorretorAltura` | fica | fica | **fica** |
| TD-PSOLA | fica, guarda menor | fica | **sai** |
| O áudio espera pela detecção? | sim | não | não |

- **v1** encurta a fila. O áudio continua parado esperando o `β`, só espera menos.
- **v2** tira o lote da detecção: o CMNDF vira dois acumuladores atualizados por amostra
  (recursão da patente US 5.973.252). O termo `nFrame` some. **Sobra a guarda do PSOLA, que é
  `fs/FMIN` por construção** — piso arquitetural, não parâmetro.
- **v3** tira os estágios 1 e 2 da frente do áudio. Eles continuam ~2 períodos atrasados, mas
  esse atraso deixa de segurar o som: ele passa a significar que o `β` aplicado agora foi
  decidido com o pitch de 2 períodos atrás. **A latência vira erro de ataque.** É a mesma dívida
  física, cobrada em outra moeda.

---

## 3. ⚠️ Correção 1 — o v1 em FMIN 80 Hz é 37,5 ms, não 23 ms

A [pesquisa §6](pesquisa-latencia-antares.md) lista o v1 com **~23 ms** em FMIN 80 Hz, e a linha
"`nFrame` no caminho do áudio" dessa tabela diz **512**.

Mas `nFrame = 512` dá `W = tauMax = 256` (`autotune_stream.h:104`), e portanto o f0 mais grave
detectável é `44100/256 = 172 Hz`. **Com essa configuração o sistema não detecta 80 Hz**, que era
justamente o ponto de usar FMIN 80.

Pela regra da própria especificação (`nFrame ≥ 2·fs/FMIN`, §3 L2), o cálculo honesto é:

| Termo | Fórmula | Amostras | ms |
|---|---|---:|---:|
| `nFrame` | `2·44100/80` → múltiplo de 4 | 1104 | 25,0 |
| `look · nHop` | L1 zera | 0 | 0 |
| Guarda (1 período, L3) | `44100/80` | 551 | 12,5 |
| **Total** | | **1655** | **37,5** |

A tabela da [especificação §4](modo-baixa-latencia.md) **já concorda**: preset Baixo (82 Hz),
36,6 ms. A pesquisa é que destoa.

**Consequência:** o v1 é bem pior do que a pesquisa sugere para voz grave, e a distância entre
v1 e v2 é maior do que parecia. O número errado não pode ir para o texto do TCC.

---

## 4. ⚠️ Correção 2 — a latência do v3 tem duas partes, e só uma foi contada

A pesquisa cita **~0,8 ms** para o v3. Esse número é a parte **fixa**, e está certo enquanto tal.
Falta a parte variável.

**Parte fixa: o atraso do interpolador**, ~32 amostras. Não depende de nota nem de taxa de
amostragem. É esta que a Antares declara ao host, e é o que explica as **37 amostras constantes
nas duas taxas** — a observação que sustenta a §1 da pesquisa.

**Parte variável: a distância entre os dois ponteiros.** Logo depois de um salto ela vale um
período `T = fs/f0`; ela encolhe até zero conforme a leitura acelerada consome a folga; o salto
seguinte a devolve para `T`. **Oscila entre 0 e `T`, média `T/2`.** Isso é atraso real que o
cantor ouve, e não aparece no valor declarado ao host.

| | Fórmula | Depende de |
|---|---|---|
| PSOLA (v1/v2) | `2·fs/FMIN` ou `fs/FMIN` | a nota mais grave que o preset **aceita** |
| Ponteiro móvel (v3) | `32 + [0 .. fs/f0]` | a nota que está sendo **cantada** |

| Caso | PSOLA (guarda 1T) | Ponteiro (média) |
|---|---:|---:|
| Soprano em A4 (440 Hz), preset Baixo | 1076 am. = **24,4 ms** | 32+50 = **1,9 ms** |
| Baixo em E2 (82 Hz) | 1076 am. = **24,4 ms** | 32+269 = **6,8 ms** |

**A diferença de espécie continua de pé, e fica mais defensável assim:** o PSOLA cobra pela nota
que o cantor *poderia* cantar; o ponteiro cobra pela nota que ele *está* cantando. Mesmo o pior
caso do v3 (6,8 ms) fica abaixo da faixa de coloração de 7 a 13 ms de Marentakis et al.

**Mas o número a citar é "2 ms no caso comum, 6,9 ms no pior", não "0,8 ms sempre".**

---

## 5. O L2 custa caro com voz grave, e isso não estava dito

A especificação apresenta o L2 (`nFrame` derivado do preset) como economia. Para FMIN 80 Hz a
regra dá `nFrame = 1104`, que é **maior** que os 1024 de hoje.

**O L2 só economiza em vozes agudas.** Para voz grave ele custa 80 amostras. Isso não invalida o
item, mas invalida descrevê-lo como ganho universal.

---

## 6. A §8 da especificação está desatualizada

As seis questões da [§8](modo-baixa-latencia.md) foram escritas antes da pesquisa de 27/08. A
questão 4 ainda pergunta *"o v1 sozinho justifica o esforço, ou vai direto pro v2?"* — uma
pergunta de um mundo com dois caminhos. Hoje são três, e o terceiro é o único que atende ao
requisito de uso ao vivo em qualquer preset.

Além disso, a questão 2 da [pesquisa §7](pesquisa-latencia-antares.md) (deslocamento de formante)
**já foi respondida** pelo commit `cb2a3b6`: teto estrutural de 2,93 % em escala cromática para
qualquer entrada, e 95 % dos quadros vozeados abaixo do limiar mais conservador da literatura.
**A objeção que parecia decisiva contra o v3 não se sustenta.**

---

## 7. O v1 não é um modo para entregar; é o eixo de medição

Reenquadramento que muda o custo do trabalho.

A contribuição acadêmica descrita na especificação é **quantificar o trade-off entre latência de
detecção e robustez de trajetória**, e a pesquisa afirma que essa curva não foi encontrada
publicada. Para desenhar uma curva são necessários pontos. Os pontos são exatamente `look` de 0
a 16 e `nFrame` por preset, que é o que a questão 2 da §8 já manda medir com SNR controlado e
GPE.

Ou seja: **a varredura dos parâmetros do v1 precisa acontecer de qualquer forma, para a
medição.** Empacotá-los como um "modo" na interface é trabalho adicional que entrega pouco,
porque o modo não alcança o alvo de latência em nenhum preset com voz grave (§3).

---

## 8. Risco: o L6 pode virar trabalho perdido

⚠️ **Inferência, não medição.**

Se o destino for o v3, o CMNDF recursivo (L6) resolve um problema que o v3 dissolve por outro
caminho. No v3 os estágios 1 e 2 rodam ao lado do caminho de áudio, então a latência **deles**
sai do orçamento, e o L6 existe para reduzir exatamente essa latência.

O que sobra no v3 não é latência de detecção, é **defasagem da correção**: o `β` aplicado ao
áudio de agora vem do pitch de ~2 períodos atrás. Problema diferente, medido de outro jeito, e o
filtro do Retune Speed (τ = 25 ms) já borra parte dele.

**Isto não está verificado.** Pode ser que o L6 ainda valha no v3 por reduzir a defasagem da
correção, o que melhoraria o erro de ataque. Mas fazer o v2 inteiro *antes* de decidir sobre o v3
corre o risco de pagar por algo que o v3 não precisa.

---

## 9. A decisão em aberto

O objetivo foi fixado em 31/08: **contribuição acadêmica e uso ao vivo, as duas coisas.** Isso
elimina parar no v1 (§3: 37,5 ms com voz grave) e deixa o v2 na fronteira (12,5 ms, dentro da
faixa 7–13 ms onde a coloração começa).

| | Rota A | Rota B |
|---|---|---|
| O que é | v3 como **motor paralelo** selecionável, com o PSOLA mantido como referência; a varredura de `look`/`nFrame` roda offline e vira a curva do texto | v2 primeiro (L6), decidir sobre o v3 depois |
| Latência | 2 ms típico, 6,9 ms pior caso | 12,5 ms com voz grave |
| Linha de base | preservada: os 19 casos do `baseline.sh` continuam medindo o motor antigo, intocado | preservada |
| Contribuição | comparação medida entre **duas** arquiteturas de síntese, mesma base de código, mesmo detector | otimização de uma arquitetura só |
| Risco | erro de ataque não medido; escopo de capítulo | §8: pode virar trabalho perdido |

**Recomendação registrada (não decidida):** rota A. O motor de ponteiro móvel é *mais simples*
que o PSOLA — buffer circular, ponteiro fracionário, interpolador e um teste de distância, sem
marcas de análise, busca por correlação, janelamento, sobreposição-soma ou normalização por
pico. Quatro problemas conhecidos deixam de existir por construção:

| Problema | Onde está registrado | No v3 |
|---|---|---|
| Drift de fase do PSOLA | [histórico](historico-e-decisoes.md) | não há grãos para alinhar |
| `psolaSintetiza()` normaliza por pico | doc técnico §8.3, Achado 3 | não há janela nem normalização |
| Janela de re-síntese cresce sem limite | doc técnico §8.3, Achado 1 | buffer de tamanho fixo |
| Alocação dentro do callback de áudio | doc técnico §8.3, Achado 2 | buffer pré-alocado |

Manter os dois motores é o que a Antares faz com Classic e Modern, e resolve sozinho a objeção
de invalidar a linha de base.

---

## 10. O que continua sem resposta

| Questão | Como responder |
|---|---|
| **O erro de ataque do v3 é aceitável?** O `β` chega ~2 períodos atrasado, e ao vivo isso vira ataque com a altura errada por ~25 ms | só escuta. É a **mesma** escuta que as Etapas 3 a 5 esperam desde o plano anterior. O teste de escuta agora bloqueia dois trabalhos |
| **Qual o artefato do salto em material não periódico?** Consoante, ataque e vibrato rápido quebram a hipótese de periodicidade; a emenda vira modulação de amplitude, não clique | medir. Protótipo descartável do ponteiro + material real |
| **As 37 amostras da Antares são mesmo atraso de interpolador?** | loopback no próprio Auto-Tune (questão 5 da §8, ainda aberta) |
| **12,5 ms bastam com fone fechado?** A literatura citada mediu intra-auricular e caixa, não circum-auricular fechado | se bastar, o v2 encerra o trabalho e o v3 vira trabalho futuro |

---

## 11. Grau de evidência

Declarado como manda o padrão do projeto.

- **§1 e §2** são **fortes**: leitura direta do código (`dsp.h:585`, `autotune_stream.h:82`).
- **§3 e §5** são **fortes**: aritmética sobre a regra que a própria especificação define, e a
  §4 dela já concorda com o resultado.
- **§4** é **inferência fundamentada**: a mecânica do ponteiro (oscilação 0..`T`) segue do
  algoritmo descrito na patente e em van Aerde, mas os números do v3 não foram medidos, porque
  não há implementação.
- **§7 e §8** são **argumento de planejamento**, não resultado.
