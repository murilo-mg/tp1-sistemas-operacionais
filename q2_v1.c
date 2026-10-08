/* Buffer circular com produtores e consumidores, versao 1.
 * gcc -Wall -Wextra -pthread q2_v1.c -o q2_v1
 */
#define VERSAO 1
#define CONSUMIDORES_PADRAO 1
#define NOME_TIPO_0 "produtor"
#define NOME_TIPO_1 "consumidor"
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_THREADS 128
#define MAX_OPERACOES 10000
#define MAX_ITENS 100000
#define MAX_VALOR 1000000
#define MAX_ATRASO 60000
#define MAX_BUFFER 256
#define TAM_TEXTO_BUFFER 18000

static pthread_mutex_t impressao = PTHREAD_MUTEX_INITIALIZER;
static sem_t inicio;
static struct timespec instante_inicial;

typedef struct {
    int id, tipo, operacoes, atraso;
    int *valores;
} Thread;

static void falhar(const char *msg) {
    fprintf(stderr, "Erro: %s\n", msg);
    exit(EXIT_FAILURE);
}

static void checar(int erro, const char *msg) {
    if (erro != 0) {
        fprintf(stderr, "Erro: %s: %s\n", msg, strerror(erro));
        exit(EXIT_FAILURE);
    }
}

static void registrar(const Thread *t, const char *fmt, ...) {
    struct timespec agora;
    va_list ap;
    checar(pthread_mutex_lock(&impressao), "lock impressao");
    if (clock_gettime(CLOCK_MONOTONIC, &agora) != 0) falhar("clock_gettime");
    double tempo = agora.tv_sec - instante_inicial.tv_sec +
                   (agora.tv_nsec - instante_inicial.tv_nsec) / 1e9;
    printf("[%8.3f s] %s %d ", tempo,
           t ? (t->tipo == 0 ? NOME_TIPO_0 : NOME_TIPO_1) : "main", t ? t->id : 0);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    checar(pthread_mutex_unlock(&impressao), "unlock impressao");
}

static void pausar(int ms) {
    unsigned int segundos = (unsigned int)ms / 1000;
    while (segundos != 0) segundos = sleep(segundos);
    struct timespec resto = {0, (long)(ms % 1000) * 1000000L};
    while (nanosleep(&resto, &resto) == -1) {
        if (errno != EINTR) falhar("nanosleep");
    }
}

static void dormir(const Thread *t, int ms, const char *motivo) {
    registrar(t, "dormindo %d ms: %s", ms, motivo);
    pausar(ms);
}

static void inicializar_sem(sem_t *s, unsigned int valor) {
    if (sem_init(s, 0, valor) == -1) falhar("sem_init");
}

static void postar(sem_t *s) {
    if (sem_post(s) == -1) falhar("sem_post");
}

static void aguardar(sem_t *s) {
    while (sem_wait(s) == -1) {
        if (errno != EINTR) falhar("sem_wait");
    }
}

static void destruir_sem(sem_t *s) {
    if (sem_destroy(s) == -1) falhar("sem_destroy");
}

static long long numero(const char *s, long long min, long long max) {
    char *fim;
    errno = 0;
    long long n = strtoll(s, &fim, 10);
    if (errno || *s == '\0' || *fim != '\0' || n < min || n > max)
        falhar("argumento numerico invalido; consulte --ajuda");
    return n;
}

static int lista(const char *s, int *destino, int max, int minval, int maxval) {
    char *copia = strdup(s);
    if (!copia) falhar("memoria para lista");
    int n = 0;
    char *p = copia;
    for (;;) {
        char *virgula = strchr(p, ',');
        if (virgula) *virgula = '\0';
        if (n == max) falhar("lista muito longa");
        destino[n++] = (int)numero(p, minval, maxval);
        if (!virgula) break;
        p = virgula + 1;
    }
    free(copia);
    return n;
}

static uint32_t proximo(uint32_t *estado) {
    *estado = *estado * UINT32_C(1664525) + UINT32_C(1013904223);
    return *estado;
}

static void *alocar(size_t n, size_t tamanho) {
    void *p = calloc(n ? n : 1, tamanho);
    if (!p) falhar("memoria insuficiente");
    return p;
}

typedef struct {
    int produtores, consumidores, op_produtor, op_consumidor, capacidade;
    int atraso_produtor, atraso_consumidor, janela;
    int atrasos_p[MAX_THREADS], atrasos_c[MAX_THREADS], np, nc;
    int valores[MAX_OPERACOES], nv, aleatorio;
    uint32_t semente;
} Config;

static Config cfg = {
    .produtores=3, .consumidores=CONSUMIDORES_PADRAO, .op_produtor=4,
    .op_consumidor=-1, .capacidade=3,
    .atraso_produtor=20, .atraso_consumidor=70, .janela=15,
    .valores={17,23,5}, .nv=3, .semente=1
};

static void ajuda(void) {
    puts("Opcoes: --produtores N --consumidores N --operacoes-produtor N\n"
         " --operacoes-consumidor N --buffer N --atraso-produtor MS\n"
         " --atraso-consumidor MS --atrasos-produtores MS,...\n"
         " --atrasos-consumidores MS,... --janela MS --valores N,...\n"
         " --aleatorio --semente N\n"
         "Sem cota explicita, consumidores dividem o total produzido.\n"
         "A cota explicita deve equilibrar os totais.\n"
         "Versao 1 exige exatamente um consumidor.\n"
         "Limites: 128 threads no total; 10000 operacoes/thread;\n"
         " 100000 itens; buffer 1..256; atrasos 0..60000 ms.\n"
         "Listas de atrasos precisam ter um valor por thread.\n"
         "Valores ciclicos por produtor; aleatorio gera 1..99.");
}

static void configurar(int argc, char **argv) {
    for (int i=1; i<argc; i++) {
        const char *k=argv[i];
        if (!strcmp(k,"--ajuda")) { ajuda(); exit(EXIT_SUCCESS); }
        if (!strcmp(k,"--aleatorio")) { cfg.aleatorio=1; continue; }
        if (++i == argc) falhar("opcao sem valor");
        const char *v=argv[i];
        if (!strcmp(k,"--produtores")) cfg.produtores=(int)numero(v,1,MAX_THREADS);
        else if (!strcmp(k,"--consumidores")) cfg.consumidores=(int)numero(v,1,MAX_THREADS);
        else if (!strcmp(k,"--operacoes-produtor")) cfg.op_produtor=(int)numero(v,0,MAX_OPERACOES);
        else if (!strcmp(k,"--operacoes-consumidor")) cfg.op_consumidor=(int)numero(v,0,MAX_OPERACOES);
        else if (!strcmp(k,"--buffer")) cfg.capacidade=(int)numero(v,1,MAX_BUFFER);
        else if (!strcmp(k,"--atraso-produtor")) cfg.atraso_produtor=(int)numero(v,0,MAX_ATRASO);
        else if (!strcmp(k,"--atraso-consumidor")) cfg.atraso_consumidor=(int)numero(v,0,MAX_ATRASO);
        else if (!strcmp(k,"--janela")) cfg.janela=(int)numero(v,0,MAX_ATRASO);
        else if (!strcmp(k,"--semente")) cfg.semente=(uint32_t)numero(v,0,UINT32_MAX);
        else if (!strcmp(k,"--valores")) cfg.nv=lista(v,cfg.valores,MAX_OPERACOES,-MAX_VALOR,MAX_VALOR);
        else if (!strcmp(k,"--atrasos-produtores")) cfg.np=lista(v,cfg.atrasos_p,MAX_THREADS,0,MAX_ATRASO);
        else if (!strcmp(k,"--atrasos-consumidores")) cfg.nc=lista(v,cfg.atrasos_c,MAX_THREADS,0,MAX_ATRASO);
        else falhar("opcao desconhecida");
    }
    if (cfg.produtores+cfg.consumidores > MAX_THREADS) falhar("excesso de threads");
    if (cfg.consumidores != 1) falhar("versao 1 exige um consumidor");
    if (cfg.produtores*cfg.op_produtor > MAX_ITENS) falhar("excesso de itens");
    if (cfg.op_consumidor == -1 &&
        (cfg.produtores*cfg.op_produtor+cfg.consumidores-1)/cfg.consumidores > MAX_OPERACOES)
        falhar("cota automatica por consumidor excede 10000");
    if (cfg.op_consumidor >= 0 &&
        cfg.consumidores*cfg.op_consumidor != cfg.produtores*cfg.op_produtor)
        falhar("cotas desequilibradas causariam espera sem fim");
    if ((cfg.np && cfg.np != cfg.produtores) || (cfg.nc && cfg.nc != cfg.consumidores))
        falhar("lista de atrasos deve ter um valor por thread");
}

static int *valor_item;
static int total_consumidos;

static sem_t vazias, cheias;
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
static int *buffer, *itens_consumidos;
static int inserir, retirar, ocupados, proxima_ordem;
static int *ordem;

static void esperar_vaga(sem_t *s, const Thread *t, const char *motivo) {
    while (sem_trywait(s) == -1) {
        if (errno == EINTR) continue;
        if (errno != EAGAIN) falhar("sem_trywait");
        registrar(t,"bloqueada: %s; dormindo no semaforo",motivo);
        aguardar(s);
        return;
    }
}

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

static void exibir(const Thread *t) {
    char texto[TAM_TEXTO_BUFFER];
    size_t usados=0;
    for (int i=0; i<cfg.capacidade; i++) {
        int id=buffer[i];
        int r=snprintf(texto+usados,sizeof(texto)-usados,
                       id ? "%s%d:%d#%d" : "%s%d:.",i ? " | " : "",i,id ? valor_item[id] : 0,id);
        if (r < 0 || (size_t)r >= sizeof(texto)-usados) falhar("linha do buffer muito longa");
        usados+=(size_t)r;
    }
    registrar(t,"BUFFER [%s] inserir=%d retirar=%d ocupados=%d",texto,inserir,retirar,ocupados);
}

static void produzir(const Thread *t, int item) {
    if (buffer[inserir] != 0 || ocupados >= cfg.capacidade) falhar("invariante de insercao");
    dormir(t,cfg.janela,"dentro da insercao");
    int pos=inserir;
    buffer[pos]=item;
    ordem[item]=++proxima_ordem;
    inserir=(inserir+1)%cfg.capacidade;
    ocupados++;
    registrar(t,"produzindo %d item=%d posicao=%d ordem=%d",valor_item[item],item,pos,ordem[item]);
    exibir(t);
}

static void consumir(const Thread *t) {
    int pos=retirar, item=buffer[pos];
    if (!item || ocupados <= 0) falhar("invariante de retirada");
    dormir(t,cfg.janela,"dentro da retirada");
    if (++itens_consumidos[item] != 1 || ordem[item] != total_consumidos+1)
        falhar("item repetido ou fora da ordem FIFO");
    total_consumidos++;
    buffer[pos]=0;
    retirar=(retirar+1)%cfg.capacidade;
    ocupados--;
    registrar(t,"consumindo %d item=%d posicao=%d ordem=%d",valor_item[item],item,pos,ordem[item]);
    exibir(t);
}

static void sair(const Thread *t) {
    registrar(t,"saiu da regiao critica");
    checar(pthread_mutex_unlock(&buffer_mutex),"unlock buffer");
    postar(t->tipo == 0 ? &cheias : &vazias);
}

static void iniciar_controle(int total) {
    buffer=alocar((size_t)cfg.capacidade,sizeof(*buffer));
    itens_consumidos=alocar((size_t)total+1,sizeof(*itens_consumidos));
    ordem=alocar((size_t)total+1,sizeof(*ordem));
    inicializar_sem(&vazias,(unsigned int)cfg.capacidade);
    inicializar_sem(&cheias,0);
}

static void encerrar_controle(int total) {
    int faltantes=0;
    for (int i=1; i<=total; i++) if (itens_consumidos[i] != 1) faltantes++;
    registrar(NULL,"RESULTADO produzidos=%d consumidos=%d faltantes=%d ocupados=%d FIFO=OK",
              proxima_ordem,total_consumidos,faltantes,ocupados);
    if (faltantes || ocupados || proxima_ordem != total || total_consumidos != total)
        falhar("resultado final inconsistente");
    destruir_sem(&vazias); destruir_sem(&cheias);
    checar(pthread_mutex_destroy(&buffer_mutex),"destroy buffer");
    free(buffer); free(itens_consumidos); free(ordem);
}

static void *executar(void *arg) {
    Thread *t=arg;
    aguardar(&inicio);
    for (int i=0; i<t->operacoes; i++) {
        dormir(t,t->atraso,"intervalo antes da operacao");
        registrar(t,"tentando entrar; operacao=%d",i+1);
        entrar(t);
        if (t->tipo == 0) produzir(t,t->valores[i]);
        else consumir(t);
        sair(t);
    }
    registrar(t,"finalizada");
    return NULL;
}

int main(int argc, char **argv) {
    configurar(argc,argv);
    if (clock_gettime(CLOCK_MONOTONIC,&instante_inicial) != 0) falhar("clock_gettime");
    int total=cfg.produtores*cfg.op_produtor;
    valor_item=alocar((size_t)total+1,sizeof(*valor_item));
    inicializar_sem(&inicio,0);
    iniciar_controle(total);
    int n=cfg.produtores+cfg.consumidores;
    Thread *dados_threads=alocar((size_t)n,sizeof(*dados_threads));
    pthread_t *threads=alocar((size_t)n,sizeof(*threads));
    int item=0;
    uint32_t estado_aleatorio=cfg.semente;
    for (int i=0; i<n; i++) {
        Thread *t=&dados_threads[i];
        t->tipo=i < cfg.produtores ? 0 : 1;
        t->id=t->tipo == 0 ? i+1 : i-cfg.produtores+1;
        t->atraso=t->tipo == 0 ? (cfg.np ? cfg.atrasos_p[t->id-1] : cfg.atraso_produtor) :
                               (cfg.nc ? cfg.atrasos_c[t->id-1] : cfg.atraso_consumidor);
        t->operacoes=t->tipo == 0 ? cfg.op_produtor :
                      (cfg.op_consumidor >= 0 ? cfg.op_consumidor :
                       total/cfg.consumidores+(t->id <= total%cfg.consumidores));
        if (t->tipo == 0) {
            t->valores=alocar((size_t)t->operacoes,sizeof(int));
            for (int j=0; j<t->operacoes; j++) {
                t->valores[j]=++item;
                valor_item[item]=cfg.aleatorio ? (int)(proximo(&estado_aleatorio)%99)+1 : cfg.valores[j%cfg.nv];
                registrar(NULL,"plano produtor=%d operacao=%d item=%d valor=%d",t->id,j+1,item,valor_item[item]);
            }
        }
    }
    registrar(NULL,"versao=%d buffer=%d total=%d semente=%u aleatorio=%d",VERSAO,cfg.capacidade,total,cfg.semente,cfg.aleatorio);
    for (int i=0; i<n; i++) {
        registrar(&dados_threads[i],"criada; cota=%d atraso=%d ms",dados_threads[i].operacoes,dados_threads[i].atraso);
        checar(pthread_create(&threads[i],NULL,executar,&dados_threads[i]),"pthread_create");
    }
    for (int i=0; i<n; i++) postar(&inicio);
    for (int i=0; i<n; i++) checar(pthread_join(threads[i],NULL),"pthread_join");
    encerrar_controle(total);
    for (int i=0; i<n; i++) free(dados_threads[i].valores);
    free(dados_threads); free(threads); free(valor_item);
    destruir_sem(&inicio);
    checar(pthread_mutex_destroy(&impressao),"destroy impressao");
    return EXIT_SUCCESS;
}
