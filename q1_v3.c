/* Conta bancaria com leitores e escritores, versao 3.
 * gcc -Wall -Wextra -pthread q1_v3.c -o q1_v3
 */
#define VERSAO 3
#define NOME_TIPO_0 "leitor"
#define NOME_TIPO_1 "escritor"
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdatomic.h>
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
    int leitores, escritores, op_leitor, op_escritor;
    int atraso_leitor, atraso_escritor, janela;
    int atrasos_l[MAX_THREADS], atrasos_e[MAX_THREADS], nl, ne;
    int valores[MAX_OPERACOES], nv, aleatorio;
    long long saldo;
    uint32_t semente;
} Config;

static Config cfg = {
    .leitores=3, .escritores=2, .op_leitor=4, .op_escritor=3,
    .atraso_leitor=20, .atraso_escritor=60, .janela=15,
    .valores={100,-40}, .nv=2, .saldo=1000, .semente=1
};

static void ajuda(void) {
    puts("Opcoes: --leitores N --escritores N --operacoes-leitor N\n"
         " --operacoes-escritor N --atraso-leitor MS --atraso-escritor MS\n"
         " --atrasos-leitores MS,... --atrasos-escritores MS,...\n"
         " --janela MS --saldo N --valores N,... --aleatorio --semente N\n"
         "Saldo e valores em centavos; negativos representam saques.\n"
         "Limites: 128 threads no total; 10000 operacoes/thread;\n"
         " 100000 atualizacoes no total; atrasos 0..60000 ms.\n"
         "Listas de atrasos precisam ter um valor por thread.\n"
         "Valores ciclicos por escritor; aleatorio gera -100..100 centavos.");
}

static void configurar(int argc, char **argv) {
    for (int i=1; i<argc; i++) {
        const char *k=argv[i];
        if (!strcmp(k,"--ajuda")) { ajuda(); exit(EXIT_SUCCESS); }
        if (!strcmp(k,"--aleatorio")) { cfg.aleatorio=1; continue; }
        if (++i == argc) falhar("opcao sem valor");
        const char *v=argv[i];
        if (!strcmp(k,"--leitores")) cfg.leitores=(int)numero(v,0,MAX_THREADS);
        else if (!strcmp(k,"--escritores")) cfg.escritores=(int)numero(v,0,MAX_THREADS);
        else if (!strcmp(k,"--operacoes-leitor")) cfg.op_leitor=(int)numero(v,0,MAX_OPERACOES);
        else if (!strcmp(k,"--operacoes-escritor")) cfg.op_escritor=(int)numero(v,0,MAX_OPERACOES);
        else if (!strcmp(k,"--atraso-leitor")) cfg.atraso_leitor=(int)numero(v,0,MAX_ATRASO);
        else if (!strcmp(k,"--atraso-escritor")) cfg.atraso_escritor=(int)numero(v,0,MAX_ATRASO);
        else if (!strcmp(k,"--janela")) cfg.janela=(int)numero(v,0,MAX_ATRASO);
        else if (!strcmp(k,"--saldo")) cfg.saldo=numero(v,-1000000000LL,1000000000LL);
        else if (!strcmp(k,"--semente")) cfg.semente=(uint32_t)numero(v,0,UINT32_MAX);
        else if (!strcmp(k,"--valores")) cfg.nv=lista(v,cfg.valores,MAX_OPERACOES,-MAX_VALOR,MAX_VALOR);
        else if (!strcmp(k,"--atrasos-leitores")) cfg.nl=lista(v,cfg.atrasos_l,MAX_THREADS,0,MAX_ATRASO);
        else if (!strcmp(k,"--atrasos-escritores")) cfg.ne=lista(v,cfg.atrasos_e,MAX_THREADS,0,MAX_ATRASO);
        else falhar("opcao desconhecida");
    }
    if (cfg.leitores+cfg.escritores > MAX_THREADS) falhar("excesso de threads");
    if (cfg.escritores*cfg.op_escritor > MAX_ITENS) falhar("excesso de atualizacoes");
    if ((cfg.nl && cfg.nl != cfg.leitores) || (cfg.ne && cfg.ne != cfg.escritores))
        falhar("lista de atrasos deve ter um valor por thread");
}

typedef struct { atomic_llong saldo, movimentado; atomic_int operacoes; } Conta;
static Conta conta;
// Atomicidade de campos isolados nao protege a operacao composta.

static void entrar(const Thread *t) {
    registrar(t,"entrou na regiao critica sem exclusao");
}
static void sair(const Thread *t) { registrar(t,"saiu da regiao critica sem exclusao"); }

static void consultar(const Thread *t) {
    long long saldo = atomic_load_explicit(&conta.saldo,memory_order_relaxed);
    long long mov = atomic_load_explicit(&conta.movimentado,memory_order_relaxed);
    int op = atomic_load_explicit(&conta.operacoes,memory_order_relaxed);
    registrar(t,"consultou: saldo=%lld operacoes=%d movimentado=%lld",saldo,op,mov);
    if (saldo != cfg.saldo + mov)
        registrar(t,"AVISO registro inconsistente: saldo=%lld inicial+movimentado=%lld",saldo,cfg.saldo+mov);
    dormir(t,cfg.janela,"consulta em andamento");
}

static void atualizar(const Thread *t, int valor) {
    long long saldo = atomic_load_explicit(&conta.saldo,memory_order_relaxed);
    long long mov = atomic_load_explicit(&conta.movimentado,memory_order_relaxed);
    int op = atomic_load_explicit(&conta.operacoes,memory_order_relaxed);
    registrar(t,"leu antes de atualizar: saldo=%lld operacoes=%d delta=%d",saldo,op,valor);
    dormir(t,cfg.janela,"entre leitura e modificacao");
    atomic_store_explicit(&conta.saldo,saldo+valor,memory_order_relaxed);
    registrar(t,"%s %lld centavos; saldo parcial=%lld",valor >= 0 ? "depositou" : "sacou",
              valor >= 0 ? (long long)valor : -(long long)valor,saldo+valor);
    dormir(t,cfg.janela,"entre campos do registro");
    atomic_store_explicit(&conta.movimentado,mov+valor,memory_order_relaxed);
    atomic_store_explicit(&conta.operacoes,op+1,memory_order_relaxed);
    registrar(t,"atualizou: saldo=%lld operacoes=%d movimentado=%lld",
              (long long)atomic_load_explicit(&conta.saldo,memory_order_relaxed),(int)atomic_load_explicit(&conta.operacoes,memory_order_relaxed),(long long)atomic_load_explicit(&conta.movimentado,memory_order_relaxed));
}

static void *executar(void *arg) {
    Thread *t=arg;
    aguardar(&inicio);
    for (int i=0; i<t->operacoes; i++) {
        dormir(t,t->atraso,"intervalo antes da operacao");
        registrar(t,"tentando entrar; operacao=%d",i+1);
        entrar(t);
        if (t->tipo == 0) consultar(t);
        else atualizar(t,t->valores[i]);
        sair(t);
    }
    registrar(t,"finalizada");
    return NULL;
}

int main(int argc, char **argv) {
    configurar(argc,argv);
    if (clock_gettime(CLOCK_MONOTONIC,&instante_inicial) != 0) falhar("clock_gettime");
    inicializar_sem(&inicio,0);
    atomic_init(&conta.saldo,cfg.saldo);
    atomic_init(&conta.movimentado,0);
    atomic_init(&conta.operacoes,0);
    int n=cfg.leitores+cfg.escritores;
    Thread *dados_threads=alocar((size_t)n,sizeof(*dados_threads));
    pthread_t *threads=alocar((size_t)n,sizeof(*threads));
    long long esperado=cfg.saldo;
    uint32_t estado_aleatorio=cfg.semente;
    for (int i=0; i<n; i++) {
        Thread *t=&dados_threads[i];
        t->tipo=i < cfg.leitores ? 0 : 1;
        t->id=t->tipo == 0 ? i+1 : i-cfg.leitores+1;
        t->operacoes=t->tipo == 0 ? cfg.op_leitor : cfg.op_escritor;
        t->atraso=t->tipo == 0 ? (cfg.nl ? cfg.atrasos_l[t->id-1] : cfg.atraso_leitor) :
                               (cfg.ne ? cfg.atrasos_e[t->id-1] : cfg.atraso_escritor);
        if (t->tipo == 1) {
            t->valores=alocar((size_t)t->operacoes,sizeof(int));
            for (int j=0; j<t->operacoes; j++) {
                t->valores[j]=cfg.aleatorio ? (int)(proximo(&estado_aleatorio)%201)-100 : cfg.valores[j%cfg.nv];
                esperado+=t->valores[j];
                registrar(NULL,"plano escritor=%d operacao=%d delta=%d",t->id,j+1,t->valores[j]);
            }
        }
    }
    registrar(NULL,"versao=%d saldo inicial=%lld semente=%u aleatorio=%d",VERSAO,cfg.saldo,cfg.semente,cfg.aleatorio);
    for (int i=0; i<n; i++) {
        registrar(&dados_threads[i],"criada; cota=%d atraso=%d ms",dados_threads[i].operacoes,dados_threads[i].atraso);
        checar(pthread_create(&threads[i],NULL,executar,&dados_threads[i]),"pthread_create");
    }
    for (int i=0; i<n; i++) postar(&inicio);
    for (int i=0; i<n; i++) checar(pthread_join(threads[i],NULL),"pthread_join");
    long long obtido=atomic_load_explicit(&conta.saldo,memory_order_relaxed);
    int ops=atomic_load_explicit(&conta.operacoes,memory_order_relaxed);
    int total=cfg.escritores*cfg.op_escritor;
    registrar(NULL,"RESULTADO esperado: saldo=%lld operacoes=%d",esperado,total);
    registrar(NULL,"RESULTADO obtido: saldo=%lld operacoes=%d movimentado=%lld",obtido,ops,(long long)atomic_load_explicit(&conta.movimentado,memory_order_relaxed));
    int divergencia=obtido != esperado || ops != total || atomic_load_explicit(&conta.movimentado,memory_order_relaxed) != esperado-cfg.saldo;
    registrar(NULL,"DIVERGENCIA=%s",divergencia ? "SIM" : "NAO");
    for (int i=0; i<n; i++) free(dados_threads[i].valores);
    free(dados_threads); free(threads);
    destruir_sem(&inicio);
    checar(pthread_mutex_destroy(&impressao),"destroy impressao");
    return EXIT_SUCCESS;
}
