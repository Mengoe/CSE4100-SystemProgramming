#include "csapp.h"
#include <sys/times.h>
#include "stockManager.h"

#define NTHREADS 100
#define SHOW 0
#define BUY 1
#define SELL 2
#define EXIT 3

item* stockTable = NULL;
long long byte_cnt;
sem_t s_for_byte_cnt;  

typedef struct{
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

sbuf_t sbuf;

/*  functions for sbuf */
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

/* functions for parsing cmd */
int parseCommand(char*buf);
int getID(char* buf);
int getStockNum(char *buf);

/*  functions for stock management */
void buy(int connfd, int ID, int buynum);
void sell(int connfd, int ID, int sellnum);
void show(int connfd);

/* functions for thread and process request */
void* thread(void* vargp);
void processingCommand(int connfd);
static void init_byte_cnt(void);

/* signalHandler for updating stock.txt */
void sigint_handler(int sig){
    writeFile = fopen("stock.txt", "w");
    updateTable(stockTable);
    exit(0);
}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }

    stockTable = makeTable();
    listenfd = Open_listenfd(argv[1]);
    Signal(SIGINT, sigint_handler);
    sbuf_init(&sbuf, FD_SETSIZE);

    // prethreading
    for(int i=0; i<NTHREADS; i++) Pthread_create(&tid, NULL, thread, NULL);

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
    }
 
    exit(0);
}
/* $end servermain */

/* thread related functions start */
void* thread(void* vargp){
    Pthread_detach(pthread_self());
    
    while(1){
        int connfd = sbuf_remove(&sbuf);
        processingCommand(connfd);
        Close(connfd);
    }
}

static void init_byte_cnt(void){
    Sem_init(&s_for_byte_cnt, 0, 1);
    byte_cnt = 0;
}

void processingCommand(int connfd){
    int n;
    int cmdtype;
    char buf[MAXLINE];
    int exitFlag = 0;
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    Pthread_once(&once, init_byte_cnt);
    Rio_readinitb(&rio, connfd);

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        P(&s_for_byte_cnt);
        byte_cnt += n;
        printf("thread %d received %d (%lld total) bytes on fd %d\n", (int)pthread_self(), n, byte_cnt, connfd);
        V(&s_for_byte_cnt);

        cmdtype = parseCommand(buf);

        switch(cmdtype){
            case BUY : buy(connfd, getID(buf), getStockNum(buf));
                       break;
                    
            case SELL : sell(connfd, getID(buf), getStockNum(buf));
                        break;
                    
            case SHOW : show(connfd);
                        break;

            case EXIT : memset(buf, '\0', MAXLINE * sizeof(char));
                        Rio_writen(connfd, buf, MAXLINE);
                        exitFlag = 1;
                        break;
        }

        if(exitFlag) break;
    }
}

/* thread related functions end */


/* command parsing functions start */
// all functions are thread-safe because they reference no shared-variables

int parseCommand(char *buf){
    int argc=0;
    char *argv[5];
    char *delim;

    buf[strlen(buf)-1] = ' ';
    while(*buf && (*buf == ' ')) buf++;

    while((delim = strchr(buf, ' '))){
        argv[argc++] = buf;
        *delim = '\0';

        buf = delim+1;
        while(*buf && (*buf == ' ')) buf++;
    }

    if(!strcmp(argv[0], "show")) return SHOW;
    else if(!strcmp(argv[0], "buy")) return BUY;
    else if(!strcmp(argv[0], "sell")) return SELL;
    else if(!strcmp(argv[0], "exit")) return EXIT;
}

int getID(char* buf){
    int argc=0;
    char *argv[5];
    char *delim;
    
    buf[strlen(buf)-1] = ' ';
    while(*buf && (*buf == ' ')) buf++;

    while((delim = strchr(buf, ' '))){
        argv[argc++] = buf;
        *delim = '\0';

        buf = delim+1;
        while(*buf && (*buf == ' ')) buf++;
    }

    return atoi(argv[1]);
}

int getStockNum(char* buf){
    int argc=0;
    char *argv[5];
    char *delim;
    buf[strlen(buf)-1] = ' ';
    while(*buf && (*buf == ' ')) buf++;

    while((delim = strchr(buf, ' '))){
        argv[argc++] = buf;
        *delim = '\0';

        buf = delim+1;
        while(*buf && (*buf == ' ')) buf++;
    }

    return atoi(argv[2]);
}

/* command parsing function end */



/* stock management functions start */

void buy(int connfd, int ID, int buynum){
    char buf[MAXLINE] = {'\0'};

    if(buyStock(ID, buynum, stockTable) == 0){
        strcpy(buf, "Not enough left stock\n");
        int idx = strlen(buf);

        for(int i=strlen(buf); i<MAXLINE; i++) buf[i] = '.';
        buf[idx] = '\0';
        Rio_writen(connfd, buf, MAXLINE);
        return;
    }

    strcpy(buf, "[buy] success\n");
    int idx = strlen(buf);
    for(int i=strlen(buf); i<MAXLINE; i++) buf[i] = '.';
    buf[idx] = '\0';
    Rio_writen(connfd, buf, MAXLINE); 

}

void sell(int connfd, int ID, int sellnum){
    sellStock(ID, sellnum, stockTable);
    char buf[MAXLINE];

    strcpy(buf, "[sell] success\n");
    int idx = strlen(buf);
    
    for(int i=idx; i<MAXLINE; i++) buf[i] = '.';
    buf[idx] = '\0';

    Rio_writen(connfd, buf, MAXLINE);
}

void show(int connfd){
    char buf[MAXLINE] = {'\0'};
    getTable(stockTable, &buf);

    int idx = strlen(buf);
    for(int i=idx; i<MAXLINE; i++) buf[i] = '.';
    
    buf[idx] = '\0';
    Rio_writen(connfd, buf, MAXLINE);
}

/* stock management functions end */

/* sbuf related functions start */
void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots);
    P(&sp->mutex);
   
    sp->buf[(++sp->rear)%(sp->n)] = item;
    
    V(&sp->mutex);
    V(&sp->items);
}

int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items);
    P(&sp->mutex);
   
    item = sp->buf[(++sp->front)%(sp->n)];
   
    V(&sp->mutex);
    V(&sp->slots);

    return item;
}
/* sbuf related functions end */
