# TP1 — Sistemas Operacionais

Trabalho prático da disciplina de Sistemas Operacionais da Universidade Federal do Amazonas (UFAM), período 2026/2. O projeto reúne seis programas em C para estudar concorrência com threads: três versões do problema de leitores e escritores e três do problema de produtores e consumidores.

As execuções mostram a criação e o término das threads, as tentativas de acesso, os bloqueios e as alterações nos dados compartilhados. A comparação entre versões permite observar o efeito da exclusão mútua, da preferência de acesso e da ausência de proteção das operações compostas.

## Versões implementadas

| Programa | Problema | Comportamento |
| --- | --- | --- |
| [`q1_v1.c`](q1_v1.c) | Leitores e escritores | Sem preferência explícita entre os tipos; leitores podem consultar juntos e escritores têm acesso exclusivo. |
| [`q1_v2.c`](q1_v2.c) | Leitores e escritores | Preferência dos escritores na admissão; novos leitores aguardam enquanto houver escritor ativo ou em espera. |
| [`q1_v3.c`](q1_v3.c) | Leitores e escritores | Sem exclusão das operações da conta; pode apresentar atualizações perdidas e leituras inconsistentes. |
| [`q2_v1.c`](q2_v1.c) | Produtores e consumidores | Vários produtores e exatamente um consumidor, com semáforos de disponibilidade e mutex do buffer. |
| [`q2_v2.c`](q2_v2.c) | Produtores e consumidores | Vários produtores e consumidores, mantendo a retirada dos itens em ordem FIFO. |
| [`q2_v3.c`](q2_v3.c) | Produtores e consumidores | Sem proteção das operações do buffer; registra sobrescritas, consumo repetido, posições vazias e interferências nos índices. |

Na questão 1, o recurso compartilhado é uma conta bancária com saldo, quantidade de operações e movimentação acumulada. Os valores são inteiros em centavos: valores positivos representam depósitos e negativos, saques. O saldo inicial padrão é de 1000 centavos, e a simulação admite saldo negativo.

Na questão 2, os dados passam por um buffer circular. Cada item tem um identificador próprio, permitindo distinguir itens diferentes mesmo quando seus valores são iguais. A ordem FIFO acompanha as inserções efetivamente realizadas, e não a ordem numérica desses identificadores.

As versões 3 usam operações atômicas C11 nos campos individuais, mas não tornam indivisível a sequência completa de leitura, cálculo e atualização. O semáforo de início e o mutex de impressão organizam a execução e os logs; eles não protegem a conta ou o buffer nessas versões.

## Requisitos

- Linux, ou um ambiente Linux como o WSL.
- GCC com suporte a C11 e às bibliotecas POSIX de threads e semáforos.
- GNU Make.
- Git, para clonar o repositório.

## Compilação

```bash
git clone https://github.com/murilo-mg/tp1-sistemas-operacionais.git
cd tp1-sistemas-operacionais
make
```

O [`Makefile`](Makefile) compila os seis programas com `-std=c11 -O0 -Wall -Wextra -pthread`. Cada arquivo também pode ser compilado separadamente:

```bash
gcc -std=c11 -O0 -Wall -Wextra -pthread q1_v1.c -o q1_v1
```

Para compilar apenas uma versão com Make:

```bash
make q2_v2
```

Para remover os executáveis:

```bash
make clean
```

## Exemplos de execução

### Questão 1 — Leitores e escritores

Sem preferência explícita:

```bash
./q1_v1 --leitores 2 --escritores 2 \
  --operacoes-leitor 3 --operacoes-escritor 2 \
  --atraso-leitor 5 --atraso-escritor 8 \
  --janela 12 --valores 100,-40
```

Com preferência dos escritores:

```bash
./q1_v2 --leitores 2 --escritores 2 \
  --operacoes-leitor 3 --operacoes-escritor 2 \
  --atraso-leitor 5 --atraso-escritor 8 \
  --janela 12 --valores 100,-40
```

Nos dois exemplos, cada escritor realiza um depósito de 100 e um saque de 40 centavos. O resultado esperado é saldo de **1120 centavos**, quatro operações e `DIVERGENCIA=NAO`.

Sem exclusão das operações:

```bash
./q1_v3 --leitores 2 --escritores 3 \
  --operacoes-leitor 15 --operacoes-escritor 3 \
  --atraso-leitor 2 --atraso-escritor 0 \
  --janela 12 --valores 100
```

O cálculo sequencial prevê saldo de 1900 centavos e nove operações. A execução concorrente pode obter um resultado diferente e emitir avisos de registro inconsistente.

### Questão 2 — Produtores e consumidores

Vários produtores e um consumidor:

```bash
./q2_v1 --produtores 3 --consumidores 1 \
  --buffer 2 --operacoes-produtor 3 \
  --atraso-produtor 1 --atraso-consumidor 15 --janela 3
```

Vários produtores e consumidores:

```bash
./q2_v2 --produtores 3 --consumidores 2 \
  --buffer 2 --operacoes-produtor 3 \
  --atraso-produtor 1 --atraso-consumidor 15 --janela 3
```

Esses exemplos produzem nove itens. Nas versões sincronizadas, todos devem ser consumidos uma única vez, com `faltantes=0`, `ocupados=0` e `FIFO=OK`.

Sem proteção das operações do buffer:

```bash
./q2_v3 --produtores 3 --consumidores 2 \
  --buffer 2 --operacoes-produtor 6 \
  --atraso-produtor 0 --atraso-consumidor 5 --janela 12
```

Essa versão tenta produzir 18 itens e registra os conflitos encontrados durante as operações. As tentativas de consumo de posições vazias também contam na cota do consumidor, permitindo encerrar a demonstração mesmo quando itens são perdidos.

## Parâmetros

Todos os programas aceitam `--ajuda` e podem ser executados sem argumentos para usar a configuração padrão:

```bash
./q1_v1 --ajuda
./q2_v2 --ajuda
```

| Grupo | Opções | Finalidade |
| --- | --- | --- |
| Leitores e escritores | `--leitores`, `--escritores` | Quantidade de threads de cada tipo. |
| Leitores e escritores | `--operacoes-leitor`, `--operacoes-escritor` | Quantidade de consultas ou atualizações por thread. |
| Leitores e escritores | `--saldo`, `--valores` | Saldo inicial e lista de depósitos ou saques, em centavos. |
| Produtores e consumidores | `--produtores`, `--consumidores` | Quantidade de threads de cada tipo. |
| Produtores e consumidores | `--buffer`, `--operacoes-produtor` | Capacidade do buffer e quantidade de itens por produtor. |
| Produtores e consumidores | `--operacoes-consumidor` | Cota por consumidor; quando omitida, o total produzido é dividido entre os consumidores. |
| Todas as versões | `--atraso-leitor`, `--atraso-escritor`, `--atraso-produtor`, `--atraso-consumidor` | Atraso antes de cada tentativa, conforme o tipo da thread. |
| Todas as versões | `--atrasos-leitores`, `--atrasos-escritores`, `--atrasos-produtores`, `--atrasos-consumidores` | Lista de atrasos individuais, com um valor por thread do tipo correspondente. |
| Todas as versões | `--janela` | Pausa dentro da operação para tornar as interações mais visíveis. |
| Todas as versões | `--valores`, `--aleatorio`, `--semente` | Lista de valores ou geração pseudoaleatória com semente definida. |

Cada programa aceita apenas as opções correspondentes à sua questão. Os atrasos e a janela são expressos em milissegundos. A lista de valores se repete para cada escritor ou produtor. Nas versões sincronizadas do buffer, uma cota explícita de consumo que não equilibre o total produzido é rejeitada.

## Como interpretar a saída

Os logs incluem tempo relativo, tipo e identificador da thread, operação e estado dos dados. No buffer, `17#1` representa o item de identificador 1 e valor 17; `.` indica uma célula vazia.

As mensagens de bloqueio indicam uma tentativa que encontrou o recurso indisponível. Os resumos finais comparam o resultado da conta com a referência sequencial ou apresentam as contagens e a ordem de consumo dos itens.

A ordem das threads e os horários variam conforme o escalonador. Uma semente fixa reproduz os valores pseudoaleatórios, mas não a sequência de execução. Nas versões sem exclusão, as falhas demonstradas podem variar entre execuções; aumentar os atrasos pode facilitar sua observação, sem garantir um resultado específico.

## Relatório

O [relatório técnico em Markdown](relatorio.md) detalha as decisões de implementação, os protocolos de sincronização, os trechos de código, os exemplos de execução e as referências utilizadas.

## Equipe

- Murilo da Mota Gonçalves
- Matheus Sales Rosa
- William Ferreira Pinheiro

**Disciplina:** Sistemas Operacionais — UFAM, 2026/2.  
**Professor:** João Marcos Bastos Cavalcanti.
