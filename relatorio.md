# Trabalho Prático 1 — Sistemas Operacionais

**Universidade Federal do Amazonas (UFAM) — Instituto de Computação**

Disciplina: Sistemas Operacionais · Período letivo: 2026/2

Professor: João Marcos Bastos Cavalcanti

## 1. Identificação da equipe

- Murilo da Mota Gonçalves
- Matheus Sales Rosa
- William Ferreira Pinheiro

## 2. Introdução

Este trabalho implementa seis programas independentes para comparar políticas de acesso a uma conta bancária e a um buffer circular. As versões sincronizadas mantêm os invariantes dos dados. As versões sem exclusão expõem atualizações perdidas, leituras de registros incompletos e falhas na inserção e retirada de itens.

## 3. Decisões gerais de implementação

Os programas usam C11, pthread e semáforos POSIX sem nome, com `sem_init` e `pshared` igual a zero, para Linux. Cada versão é autocontida em um arquivo .c e pode ser compilada sem arquivos auxiliares. O Makefile utiliza gcc -std=c11 -O0 -Wall -Wextra -pthread. A compilação também foi verificada com exatamente gcc -Wall -Wextra -pthread.

O tema da primeira questão é uma conta com saldo, número de operações e movimentação acumulada. Os valores são inteiros em centavos. Depósitos têm delta positivo e saques, negativo. Para estudar concorrência sem regras adicionais de autorização, admite-se saldo negativo. O saldo inicial padrão é 1000 centavos. Em uma observação consistente, saldo = saldo inicial + `movimentado`.

A segunda questão usa um vetor circular. Cada célula guarda um identificador único de item; o valor correspondente fica em uma tabela imutável, preenchida antes de iniciar as threads. Assim, dois itens com valor 17 continuam distinguíveis. A impressão valor#id identifica ambos. O ponto representa célula vazia. A ordem FIFO refere-se à sequência efetiva de inserções protegidas, e não aos identificadores atribuídos antecipadamente aos produtores.

A organização segue os exemplos de pthread da Aula 5: uma função executada pelas threads e a `main` responsável por criar e `aguardar` todas elas. As funções `entrar` e `sair` delimitam as regiões críticas, como na Aula 10. As funções `aguardar` e `postar` encapsulam `sem_wait` e `sem_post`, correspondentes às operações down/P e up/V dos slides, com verificação de erros. A conta bancária também retoma o exemplo apresentado nessa aula.

### 3.1 Parâmetros e atrasos

A entrada é feita por argumentos de linha de comando. `--ajuda` apresenta as opções. As listas de valores são repetidas por thread escritora ou produtora. `--aleatorio` substitui a lista por valores gerados na `main`; `--semente` torna esses valores reproduzíveis. A ordem das threads e os horários continuam dependentes do escalonador.

| Questão | Opções principais | Padrão |
| --- | --- | --- |
| 1 | `--leitores` / `--escritores` | 3 / 2 |
| 1 | `--operacoes-leitor` / `--operacoes-escritor` | 4 / 3 |
| 1 | `--atraso-leitor` / `--atraso-escritor` | 20 / 60 ms |
| 1 | `--saldo` / `--valores` | Saldo de 1000 centavos; valores 100,-40 |
| 2 | `--produtores` / `--consumidores` | 3 produtores; 1 consumidor na v1 e 2 nas v2 e v3 |
| 2 | `--operacoes-produtor` / `--operacoes-consumidor` | 4 / divisão automática do total |
| 2 | `--buffer` | 3 posições |
| 2 | `--atraso-produtor` / `--atraso-consumidor` | 20 / 70 ms |
| 2 | `--valores` | 17,23,5 |
| Ambas | `--janela` / `--semente` | 15 ms / 1 |
| Ambas | `--aleatorio` | Desativado |

`--atrasos-leitores`, `--atrasos-escritores`, `--atrasos-produtores` e `--atrasos-consumidores` permitem um atraso individual por thread. A lista deve ter exatamente um valor por thread do tipo correspondente; ela prevalece sobre o atraso geral. Todos os atrasos e a janela podem ser zero. A função `pausar` usa `sleep` para a parte em segundos e `nanosleep` para o restante em milissegundos, retomando chamadas interrompidas.

O atraso configurado para cada tipo de thread ocorre antes de cada tentativa. A janela é um atraso dentro da operação: uma pausa por consulta e duas por atualização, inserção ou retirada sem exclusão; nas inserções e retiradas sincronizadas há uma pausa. Nas versões corretas, dormir dentro da região protegida preserva os invariantes, mas reduz a vazão e evidencia o bloqueio. Nas versões sem exclusão, as pausas ampliam a oportunidade de interferência.

Foram definidos limites de 128 threads no total, 10000 operações por thread, 100000 atualizações ou itens produzidos, buffer de 1 a 256 posições e atrasos de 0 a 60000 ms. Valores de atualização e de produção ficam entre -1000000 e 1000000. O saldo inicial fica entre -1000000000 e 1000000000. Esses limites evitam entradas excessivas e estouro aritmético nos cálculos usados. O gerador congruente trabalha em `uint32_t`; o estouro modular é definido. Gera deltas de -100 a 100 e itens de 1 a 99.

### 3.2 Instrumentação e término

Cada evento traz tempo relativo obtido com `CLOCK_MONOTONIC`, tipo e identificador da thread. A criação é registrada imediatamente antes de `pthread_create`; em caso de falha, há mensagem de erro e término do processo. O semáforo `inicio` segura as threads até a `main` concluir todas as criações. Ele não controla o acesso aos dados nas versões sem exclusão.

Um mutex exclusivo de impressão evita linhas misturadas. Nenhuma aquisição desse mutex engloba leitura ou atualização da conta ou do buffer nas versões sem exclusão: `registrar` recebe valores já calculados. As cópias exibidas nessas versões não são snapshots transacionais. Nas versões sincronizadas, as linhas do buffer são construídas enquanto o mutex do buffer está adquirido.

`sem_trywait` é usado para identificar indisponibilidade e `registrar` “bloqueada” antes de `sem_wait`. A mensagem informa que a tentativa encontrou o `recurso` indisponível; outra thread pode liberá-lo entre as duas chamadas, de modo que a espera efetiva pode ser breve. Não se inventa bloqueio nas versões sem exclusão. As operações têm cotas finitas. Após `pthread_join` de todas as threads, a `main` verifica os resultados, destrói os semáforos e mutexes inicializados e libera a memória.

### 3.3 Interpretação das versões sem controle

A ausência de controle foi interpretada como ausência de exclusão e de protocolo de sincronização das operações compostas. Acessar variáveis comuns simultaneamente em C causaria data race e comportamento indefinido. Para que as demonstrações tenham significado mesmo com otimização, os campos compartilhados dessas versões são atômicos C11 com `memory_order_relaxed`. Não se usa compare-and-swap, trava do registro, trava do buffer ou semáforo de disponibilidade para tornar a operação composta indivisível.

Uma carga ou gravação atômica isolada não torna atômica a sequência ler, calcular, dormir e gravar. Dois escritores ainda podem calcular a partir do mesmo saldo. No buffer, `atomic_exchange` grava ou remove uma célula e retorna o conteúdo anterior, permitindo detectar sobrescritas. Contadores atômicos de diagnóstico registram repetições sem reservar a operação para uma thread. Essa escolha evita o comportamento indefinido decorrente de acessos concorrentes não atômicos a esses campos, mas mantém as falhas lógicas das operações compostas.

## 4. Questão 1 — Leitores e escritores

Os exemplos seguintes foram obtidos de execuções reais em Linux x86_64, com GCC 13.3.0. As linhas foram selecionadas em ordem cronológica; eventos intermediários foram omitidos. Uma nova execução pode produzir outra ordem e outros horários.

### 4.1 Versão 1 — Sem preferência explícita

O semáforo `atendimento` é uma passagem comum para leitores e escritores. O primeiro leitor adquire `recurso`; outros leitores podem consultar juntos, e o último libera `recurso`. Um escritor adquire `recurso` sozinho. O semáforo `contagem` protege o número de leitores. A passagem fica adquirida enquanto um escritor espera `recurso`, impedindo novos leitores de prolongarem esse lote indefinidamente.

Nenhum tipo recebe prioridade por regra. A política não garante FIFO estrito nem justiça absoluta: semáforos POSIX não prometem ordem universal de despertar. “Sem preferência” foi interpretado como ausência de favorecimento explícito de um tipo, mantendo a exclusão exigida para leitores e escritores. Não houve leitura suja nas execuções desta versão.

#### Trecho relevante

```c
static void entrar(const Thread *t) {
    esperar_log(&atendimento,t,"atendimento ocupado");
    if (t->tipo == 0) {
        esperar_log(&contagem,t,"contagem ocupada");
        if (leitores_ativos == 0) esperar_log(&recurso,t,"escritor ativo");
        leitores_ativos++;
        postar(&contagem);
    } else esperar_log(&recurso,t,"registro em uso");
    postar(&atendimento);
    registrar(t,"entrou na regiao critica");
}
```

#### Execução real

```bash
./q1_v1 --leitores 2 --escritores 2 --operacoes-leitor 3 --operacoes-escritor 2 \
  --atraso-leitor 5 --atraso-escritor 8 --janela 12 --valores 100,-40
```

```text
[   0.000 s] leitor 1 criada; cota=3 atraso=5 ms
[   0.005 s] leitor 1 entrou na regiao critica
[   0.005 s] leitor 1 consultou: saldo=1000 operacoes=0 movimentado=0
[   0.008 s] escritor 1 tentando entrar; operacao=1
[   0.008 s] escritor 1 bloqueada: atendimento ocupado; dormindo no semaforo
[   0.017 s] leitor 1 saiu da regiao critica
[   0.042 s] escritor 1 entrou na regiao critica
[   0.066 s] escritor 1 atualizou: saldo=1200 operacoes=2 movimentado=200
[   0.066 s] escritor 1 saiu da regiao critica
[   0.127 s] escritor 1 finalizada
[   0.139 s] main 0 RESULTADO esperado: saldo=1120 operacoes=4
[   0.139 s] main 0 RESULTADO obtido: saldo=1120 operacoes=4 movimentado=120
[   0.139 s] main 0 DIVERGENCIA=NAO
```

#### Análise

As consultas podem se sobrepor, mas a atualização começa depois da saída do lote de leitores. Dois escritores aplicaram 100 e -40, totalizando 120 centavos. O saldo obtido foi 1120 e o contador terminou em quatro operações, iguais ao cálculo sequencial.

No enunciado, “pode ocorrer leitura suja” descreve uma possibilidade de observar uma atualização ainda incompleta. Ela ocorreria se o leitor consultasse enquanto um escritor já alterou saldo, mas ainda não atualizou os demais campos. A falta de prioridade, por si só, não causa leitura suja. O protocolo desta versão exclui esse cenário; ele é observado na versão 3. O programa não modela transações com rollback, portanto o termo é usado no sentido didático de observação intermediária.

### 4.2 Versão 2 — Preferência dos escritores

Um mutex `estado` protege a decisão de admissão e os contadores de threads ativas e esperando. Cada thread possui um semáforo `liberacao`, inicialmente zero. As filas `fila_leitores` e `fila_escritores` guardam ponteiros para as threads esperando. Um leitor novo é admitido somente quando `escritor_ativo` e `escritores_esperando` são zero. Um escritor espera quando há leitores ativos ou outro escritor.

Quando o último usuário sai, o protocolo reserva o próximo acesso ainda sob `estado`. Havendo escritores em espera, o protocolo retira o primeiro da fila, reserva seu acesso e sinaliza seu semáforo individual. Quando não há escritores em espera, reserva o acesso do lote de leitores e sinaliza seus semáforos individuais. A reserva e o semáforo individual impedem que uma thread recém-chegada consuma a liberação de outra. A fila de escritores é circular; o lote de leitores é armazenado em vetor. Uma thread reservada já conta como ativa, mesmo antes de voltar a executar; leitores já admitidos podem concluir enquanto um escritor espera.

Os semáforos fazem a espera e o despertar; o mutex simplifica a atualização conjunta do `estado` de admissão. Ele não fica adquirido durante consultas ou atualizações, permitindo consultas simultâneas. Uma sucessão ilimitada de escritores poderia causar inanição de leitores. Neste programa, as cotas dos escritores são finitas, de modo que a fila de leitores acaba sendo liberada, supondo progresso do escalonador.

#### Trecho relevante

```c
if (t->tipo == 0) {
    if (escritor_ativo || escritores_esperando) {
        fila_leitores[leitores_esperando++]=t;
        bloquear=1;
        registrar(t,"bloqueada: escritor ativo ou esperando; dormindo no semaforo");
    } else leitores_ativos++;
} else {
    if (escritor_ativo || leitores_ativos) {
        fila_escritores[fim_escritores]=t;
        fim_escritores=(fim_escritores+1)%MAX_THREADS;
        escritores_esperando++;
        bloquear=1;
        registrar(t,"bloqueada: registro em uso; escritores esperando=%d; dormindo no semaforo",
                  escritores_esperando);
    } else escritor_ativo=1;
}
```

#### Execução real

```bash
./q1_v2 --leitores 2 --escritores 2 --operacoes-leitor 3 --operacoes-escritor 2 \
  --atraso-leitor 5 --atraso-escritor 8 --janela 12 --valores 100,-40
```

```text
[   0.008 s] escritor 1 bloqueada: registro em uso; escritores esperando=1; dormindo no semaforo
[   0.008 s] escritor 2 bloqueada: registro em uso; escritores esperando=2; dormindo no semaforo
[   0.017 s] leitor 1 saiu da regiao critica
[   0.017 s] leitor 2 reserva acesso para escritor; esperando_escritores=1
[   0.017 s] escritor 1 entrou na regiao critica
[   0.022 s] leitor 1 bloqueada: escritor ativo ou esperando; dormindo no semaforo
[   0.042 s] escritor 1 saiu da regiao critica
[   0.115 s] escritor 2 reserva acesso para 2 leitores; esperando_escritores=0
[   0.132 s] leitor 1 entrou na regiao critica
[   0.144 s] main 0 RESULTADO esperado: saldo=1120 operacoes=4
[   0.144 s] main 0 RESULTADO obtido: saldo=1120 operacoes=4 movimentado=120
[   0.144 s] main 0 DIVERGENCIA=NAO
```

#### Análise

Os escritores ficaram esperando enquanto as consultas iniciais terminavam. Quando o último leitor saiu, o acesso foi reservado para um escritor. A nova tentativa do leitor 1 foi bloqueada. O lote de dois leitores foi liberado após a conclusão dos escritores. O resultado final foi saldo 1120 e quatro operações, sem inconsistência do registro. A preferência refere-se à admissão; ela não interrompe leitores já admitidos. Escritores bloqueados são escolhidos na ordem de registro na fila, que pode diferir da ordem das tentativas impressas antes da admissão.

### 4.3 Versão 3 — Sem exclusão das operações

`entrar` e `sair` apenas registram eventos. Cada escritor lê saldo, `movimentado` e `operacoes`, dorme, grava o novo saldo, dorme novamente e grava os demais campos. Não há trava da conta. O leitor verifica saldo = saldo inicial + `movimentado` e avisa quando os valores observados violam essa relação.

Antes de criar threads, a `main` calcula sequencialmente o saldo esperado e a quantidade de atualizações a partir dos valores efetivamente atribuídos. Após os joins, compara todos os campos com essa referência. Uma execução que não apresenta divergência é informada como tal; os atrasos aumentam a chance de conflito, sem garantir um escalonamento específico. Só leitores não alteram o `estado`: os problemas dependem de atualizações escritoras.

#### Trecho relevante

```c
dormir(t,cfg.janela,"entre leitura e modificacao");
atomic_store_explicit(&conta.saldo,saldo+valor,memory_order_relaxed);
registrar(t,"%s %lld centavos; saldo parcial=%lld",valor >= 0 ? "depositou" : "sacou",
          valor >= 0 ? (long long)valor : -(long long)valor,saldo+valor);
dormir(t,cfg.janela,"entre campos do registro");
atomic_store_explicit(&conta.movimentado,mov+valor,memory_order_relaxed);
atomic_store_explicit(&conta.operacoes,op+1,memory_order_relaxed);
```

#### Execução real

```bash
./q1_v3 --leitores 2 --escritores 3 --operacoes-leitor 15 --operacoes-escritor 3 \
  --atraso-leitor 2 --atraso-escritor 0 --janela 12 --valores 100
```

```text
[   0.000 s] escritor 2 leu antes de atualizar: saldo=1000 operacoes=0 delta=100
[   0.000 s] escritor 1 entrou na regiao critica sem exclusao
[   0.000 s] escritor 1 leu antes de atualizar: saldo=1000 operacoes=0 delta=100
[   0.000 s] escritor 3 leu antes de atualizar: saldo=1000 operacoes=0 delta=100
[   0.013 s] escritor 1 depositou 100 centavos; saldo parcial=1100
[   0.017 s] leitor 2 AVISO registro inconsistente: saldo=1100 inicial+movimentado=1000
[   0.025 s] escritor 1 atualizou: saldo=1100 operacoes=1 movimentado=100
[   0.025 s] escritor 1 saiu da regiao critica sem exclusao
[   0.214 s] main 0 RESULTADO esperado: saldo=1900 operacoes=9
[   0.214 s] main 0 RESULTADO obtido: saldo=1300 operacoes=3 movimentado=300
[   0.214 s] main 0 DIVERGENCIA=SIM
```

#### Análise

Três escritores leram o mesmo saldo inicial 1000 e fizeram depósitos de 100. Suas gravações substituíram umas às outras. Nove depósitos deveriam produzir 1900 e nove operações; foram obtidos 1300 e três operações. Houve perda de 600 centavos no resultado em relação à referência sequencial. Isso é atualização perdida, mesmo que o registro final volte a ser internamente coerente.

Durante a execução, um leitor observou saldo 1100 enquanto saldo inicial + `movimentado` ainda era 1000. Esse aviso documenta uma observação inconsistente dos campos durante uma atualização. Os valores foram realmente registrados; não se deduz que todo leitor sempre veja inconsistência.

## 5. Questão 2 — Produtores e consumidores

Nas versões 1 e 2, `vazias` começa com a capacidade do buffer e `cheias` com zero. O produtor reserva uma posição vazia antes de adquirir `buffer_mutex`; o consumidor reserva um item cheio antes do mesmo mutex. Ao terminar, libera o mutex e sinaliza o semáforo oposto. O índice seguinte é calculado por módulo da capacidade. O mutex protege a operação conjunta no vetor, nos índices, no contador de ocupação e na numeração FIFO.

Adotaram-se cotas fixas, porque a quantidade de itens já é conhecida. Sem `--operacoes-consumidor`, cada consumidor recebe total / consumidores operações, e os primeiros total % consumidores recebem uma operação adicional. A soma é exatamente a quantidade produzida, inclusive quando alguns consumidores recebem zero. Uma cota explícita desequilibrada é rejeitada antes de criar threads nas versões sincronizadas. Não se usam poison pills, evitando valores sentinela e mantendo todos os valores disponíveis para dados.

Nas versões corretas, ninguém espera `vazias` ou `cheias` enquanto segura `buffer_mutex`. A seção protegida é finita; ao concluí-la, sinaliza-se o `recurso` necessário ao tipo oposto. Isso evita um ciclo de espera que cause deadlock. O algoritmo pressupõe que as threads prontas para executar sejam escalonadas. Erros fatais de criação ou de operações POSIX encerram o processo com diagnóstico, em vez de continuar com cotas impossíveis.

### 5.1 Versão 1 — Vários produtores e um consumidor

A versão exige exatamente um consumidor; `--consumidores` diferente de 1 é rejeitado. Os produtores disputam o mutex após reservar uma posição. O consumidor retira todos os itens inseridos, na ordem efetiva de produção. A exclusão protege também a exibição completa do buffer após cada operação.

#### Trecho relevante

```c
static void entrar(const Thread *t) {
    esperar_vaga(t->tipo == 0 ? &vazias : &cheias,t,
                 t->tipo == 0 ? "buffer cheio" : "buffer vazio");
    int r=pthread_mutex_trylock(&buffer_mutex);
    if (r == EBUSY) {
        registrar(t,"bloqueada: mutex do buffer; dormindo no mutex");
        checar(pthread_mutex_lock(&buffer_mutex),"lock buffer");
    } else checar(r,"trylock buffer");
    registrar(t,"entrou na regiao critica");
}
```

#### Execução real

```bash
./q2_v1 --produtores 3 --consumidores 1 --buffer 2 --operacoes-produtor 3 \
  --atraso-produtor 1 --atraso-consumidor 15 --janela 3
```

```text
[   0.001 s] produtor 1 tentando entrar; operacao=1
[   0.001 s] produtor 1 entrou na regiao critica
[   0.001 s] produtor 2 bloqueada: buffer cheio; dormindo no semaforo
[   0.004 s] produtor 1 produzindo 17 item=1 posicao=0 ordem=1
[   0.004 s] produtor 1 BUFFER [0:17#1 | 1:.] inserir=1 retirar=0 ocupados=1
[   0.004 s] produtor 1 saiu da regiao critica
[   0.008 s] produtor 3 BUFFER [0:17#1 | 1:17#7] inserir=0 retirar=0 ocupados=2
[   0.018 s] consumidor 1 consumindo 17 item=1 posicao=0 ordem=1
[   0.095 s] produtor 1 finalizada
[   0.164 s] main 0 RESULTADO produzidos=9 consumidos=9 faltantes=0 ocupados=0 FIFO=OK
```

Uma execução complementar registra o bloqueio por buffer vazio:

```bash
./q2_v1 --produtores 2 --consumidores 1 --buffer 2 --operacoes-produtor 2 \
  --atraso-produtor 15 --atraso-consumidor 1 --janela 3
```

```text
[   0.001 s] consumidor 1 bloqueada: buffer vazio; dormindo no semaforo
```

#### Análise

A inserção mostra valor, identificador e posição. Quando as duas vagas ficaram reservadas ou ocupadas, um produtor encontrou `vazias` indisponível e dormiu. Uma reserva retira a vaga do semáforo antes da gravação no vetor; o aviso de buffer cheio indica falta de vagas disponíveis, mesmo que haja inserções em andamento. Depois de uma retirada, a posição foi liberada. Foram produzidos e consumidos nove itens, todos uma única vez, na ordem FIFO, com buffer vazio ao final. No teste complementar, o consumidor chegou antes dos produtores e esperou um item.

### 5.2 Versão 2 — Vários produtores e vários consumidores

O mesmo protocolo permite consumidores concorrentes. A reserva em `cheias` evita retirar de um buffer vazio, e o mutex impede que dois consumidores retirem a mesma posição. O teste conta cada identificador consumido e compara sua ordem de inserção à próxima ordem esperada. A verificação ocorre na região protegida, antes de atualizar os índices.

#### Trecho relevante

```c
static void entrar(const Thread *t) {
    esperar_vaga(t->tipo == 0 ? &vazias : &cheias,t,
                 t->tipo == 0 ? "buffer cheio" : "buffer vazio");
    int r=pthread_mutex_trylock(&buffer_mutex);
    if (r == EBUSY) {
        registrar(t,"bloqueada: mutex do buffer; dormindo no mutex");
        checar(pthread_mutex_lock(&buffer_mutex),"lock buffer");
    } else checar(r,"trylock buffer");
    registrar(t,"entrou na regiao critica");
}
```

#### Execução real

```bash
./q2_v2 --produtores 3 --consumidores 2 --buffer 2 --operacoes-produtor 3 \
  --atraso-produtor 1 --atraso-consumidor 15 --janela 3
```

```text
[   0.001 s] produtor 1 tentando entrar; operacao=1
[   0.001 s] produtor 1 entrou na regiao critica
[   0.002 s] produtor 2 bloqueada: buffer cheio; dormindo no semaforo
[   0.004 s] produtor 1 produzindo 17 item=1 posicao=0 ordem=1
[   0.005 s] produtor 1 BUFFER [0:17#1 | 1:.] inserir=1 retirar=0 ocupados=1
[   0.005 s] produtor 1 saiu da regiao critica
[   0.008 s] produtor 3 BUFFER [0:17#1 | 1:17#7] inserir=0 retirar=0 ocupados=2
[   0.020 s] consumidor 2 consumindo 17 item=1 posicao=0 ordem=1
[   0.061 s] produtor 1 finalizada
[   0.100 s] main 0 RESULTADO produzidos=9 consumidos=9 faltantes=0 ocupados=0 FIFO=OK
```

Uma execução complementar registra o bloqueio por buffer vazio:

```bash
./q2_v2 --produtores 2 --consumidores 2 --buffer 2 --operacoes-produtor 2 \
  --atraso-produtor 15 --atraso-consumidor 1 --janela 3
```

```text
[   0.001 s] consumidor 1 bloqueada: buffer vazio; dormindo no semaforo
```

#### Análise

Os dois consumidores fizeram retiradas, mas cada item foi atribuído a apenas um deles. Nove itens foram divididos em cotas de cinco e quatro retiradas. O resultado registra faltantes=0, ocupados=0 e FIFO=OK. A numeração de ordem é única e segue a ordem de inserção protegida; os IDs podem aparecer fora da ordem numérica sem violar FIFO. O teste complementar também mostrou consumidores bloqueados por falta de itens.

### 5.3 Versão 3 — Sem exclusão das operações

Não existem `vazias`, `cheias` nem `buffer_mutex` nesta versão. Produtores e consumidores fazem suas tentativas sem esperar a disponibilidade. Cada operação lê índices e ocupação, dorme e altera a célula e os metadados em etapas separadas. Uma tentativa de retirada de uma posição vazia consome uma operação da cota, sem laço de espera. Isso permite finalizar a demonstração mesmo quando itens são perdidos, desde que as threads continuem sendo escalonadas.

A gravação de uma célula ocupada registra item sobrescrito no momento do `atomic_exchange`. Dois consumidores que carregam o mesmo ID antes de dormir podem ambos declarar consumo: o contador por ID detecta a segunda declaração. Se a célula lida é zero, há aviso imediato de consumo de posição vazia. A troca de índice com conteúdo anterior diferente do índice inicialmente lido detecta interferência durante a operação. O snapshot também verifica ocupação, células preenchidas e a relação circular entre os índices.

Os avisos de snapshot são observacionais: como sua leitura é feita campo a campo sem exclusão, podem mostrar estados transitórios. Eles não afirmam que um snapshot coerente existiu em um instante único. Avisos de sobrescrita e de repetição têm evidência direta na célula anterior e no contador por ID. A perda de atualizações dos índices e de ocupação também pode existir mesmo quando a observação final parece válida.

#### Trecho relevante

```c
int pos=atomic_load_explicit(&inserir,memory_order_relaxed);
int n=atomic_load_explicit(&ocupados,memory_order_relaxed);
dormir(t,cfg.janela,"entre leitura e escrita da insercao sem exclusao");
int anterior=atomic_exchange_explicit(&buffer[pos],item,memory_order_relaxed);
if (anterior != 0) aviso(t,"item sobrescrito",anterior,item);
dormir(t,cfg.janela,"entre buffer e indice de insercao");
int indice_anterior=atomic_exchange_explicit(&inserir,(pos+1)%cfg.capacidade,memory_order_relaxed);
if (indice_anterior != pos) aviso(t,"indices inconsistentes na insercao",indice_anterior,pos);
atomic_store_explicit(&ocupados,n+1,memory_order_relaxed);
```

#### Execução real

```bash
./q2_v3 --produtores 3 --consumidores 2 --buffer 2 --operacoes-produtor 6 \
  --atraso-produtor 0 --atraso-consumidor 5 --janela 12
```

```text
[   0.000 s] produtor 1 entrou na regiao critica sem exclusao
[   0.005 s] consumidor 1 AVISO consumo de posicao vazia: observado=0 referencia=0
[   0.012 s] produtor 3 AVISO item sobrescrito: observado=1 referencia=13
[   0.024 s] produtor 1 BUFFER [0:. | 1:.] inserir=1 retirar=0 ocupados=1 (copia nao transacional)
[   0.024 s] produtor 1 AVISO indices inconsistentes na observacao: observado=1 referencia=0
[   0.024 s] produtor 1 saiu da regiao critica sem exclusao
[   0.076 s] consumidor 2 AVISO item consumido duas vezes: observado=15 referencia=2
[   0.265 s] main 0 RESULTADO produzidos=18 consumos_validos=8 nao_consumidos=14 ids_duplicados=4 presentes=0 ocupados=2 avisos=87
```

#### Análise

A execução produziu 18 itens, mas teve somente oito declarações de consumo de IDs não vazios. Quatorze IDs nunca foram consumidos e quatro IDs tiveram consumo repetido; portanto apenas quatro IDs distintos foram consumidos. O resultado soma 87 avisos, que podem descrever aspectos diferentes de um mesmo conflito. Não se interpreta esse total como 87 itens perdidos.

Os quatro tipos de anomalia solicitados aparecem nos registros reais: sobrescrita de uma célula ocupada, consumo repetido do mesmo ID, tentativa de consumo vazio e índice alterado por outra operação. O `estado` final não restaura os itens perdidos nem comprova execução correta. A `main` faz join de todas as threads e destrói somente o semáforo `inicio`, pois semáforos de disponibilidade não foram criados nesta versão.

## 6. Como compilar e executar

No Linux, é necessário ter GCC, Make e os arquivos de desenvolvimento da biblioteca C, que incluem o suporte a pthread. Dentro da pasta do projeto, execute:

```bash
make
./q1_v1 --ajuda
./q2_v2 --ajuda
```

Para remover os executáveis após as execuções:

```bash
make clean
```

Compilação independente de uma versão:

```bash
gcc -Wall -Wextra -pthread q1_v1.c -o q1_v1
```

Substitua `q1_v1` por `q1_v2`, `q1_v3`, `q2_v1`, `q2_v2` ou `q2_v3` para compilar as demais versões. Todos os comandos usados nos exemplos estão nas subseções anteriores. Para incluir valores negativos, use `--valores 100,-40`. Para variar o ritmo individual de dois escritores, use `--atrasos-escritores 0,80`. Para reproduzir os valores aleatórios, use `--aleatorio --semente 37`; a ordem de execução das threads continua dependente do escalonador.

A entrega contém seis arquivos-fonte em C, correspondentes às três versões de cada questão, um `Makefile` e este relatório. Cada programa pode ser compilado e executado independentemente. O comando `make` gera os executáveis e `make clean` remove os arquivos compilados. O relatório apresenta as decisões de implementação, comandos de exemplo e trechos selecionados das saídas.

### 6.1 Verificação realizada

A verificação registrada para a implementação incluiu a compilação dos seis programas com `gcc -Wall -Wextra -pthread`, sem mensagens de erro. Também foram feitas compilações com otimização e UBSan, usando as opções `-std=c11 -O2 -Wall -Wextra -Werror -pthread -fsanitize=undefined -fno-sanitize-recover=all`, seguidas de execuções padrão sem diagnóstico do sanitizador. O ambiente registrado foi Linux 6.18.44 x86_64 e GCC Ubuntu 13.3.0.

O registro de verificação informa 91 execuções conferidas por um script separado, com limite de 15 segundos por processo. Nos casos sincronizados, o teste reconstruiu entradas e saídas das regiões críticas, validou o saldo, os identificadores, a sequência FIFO e a relação inserir = (retirar + ocupados) módulo capacidade. Incluiu buffer de tamanho 1, vários consumidores, consumidores com cota zero, ausência de leitores ou escritores, valores aleatórios e atrasos individuais. Entradas inválidas e cotas desequilibradas foram rejeitadas.

As execuções demonstrativas das versões sem exclusão mostraram divergência na conta e os quatro tipos de anomalia no buffer. Essas execuções complementam a análise dos protocolos; testes finitos e UBSan não provam ausência universal de falhas de concorrência. A terminação das versões corretas segue também das cotas equilibradas e da ausência de espera por disponibilidade enquanto se segura o mutex do buffer.

## 7. Conclusão

Semáforos coordenam a disponibilidade e o despertar; a exclusão protege a atualização conjunta dos dados. A conta sem preferência explícita e a conta com prioridade de escritores produziram o saldo sequencial esperado. A prioridade alterou a admissão e pode causar inanição de leitores sob demanda infinita de escrita. As duas versões corretas do buffer consumiram cada item uma única vez e em ordem FIFO.

As versões sem exclusão mostraram que atomicidade de campos não substitui proteção das operações compostas. A referência sequencial revelou atualização perdida, e os diagnósticos por ID distinguiram duplicação de consumo de repetição legítima de valores. As cotas finitas permitiram concluir as demonstrações e liberar os recursos após os joins.

## 8. Referências

- **Slides da disciplina:** João Marcos Bastos Cavalcanti. *Aula 5 — Threads* e *Aula 10 — Concorrência entre Processos*. Sistemas Operacionais, UFAM.
- **Semáforos e concorrência:** Remzi H. Arpaci-Dusseau e Andrea C. Arpaci-Dusseau. *Operating Systems: Three Easy Pieces*, capítulo 31: [Semaphores](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-sema.pdf).
- **Documentação POSIX:** Linux man-pages: [sem_wait(3)](https://man7.org/linux/man-pages/man3/sem_wait.3.html) e [pthread_mutex_lock(3p)](https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html).
- **Assistente de IA:** ChatGPT/Codex (OpenAI), utilizado como apoio no desenvolvimento do código e na redação e revisão do relatório.

