/* 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#include "stockManager.h"
#include <sys/times.h>

#define SHOW 0
#define BUY 1
#define SELL 2
#define EXIT 3

typedef struct{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

/* variables for server's data structure and load */
item* stockTable = NULL;
long long byte_cnt = 0;

/*  functions for process request */
void init_pool(int listenfd, pool*p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);

/* functions for parsing cmd */
int parseCommand(char*buf);
int getID(char* buf);
int getStockNum(char *buf);

/*  functions for stock management */
void buy(int connfd, int ID, int buynum);
void sell(int connfd, int ID, int sellnum);
void show(int connfd);


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
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage   
    static pool pool;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

    stockTable = makeTable();
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    Signal(SIGINT, sigint_handler);
    
    
    while (1) {
        pool.ready_set = pool.read_set;

        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
        if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            printf("Connection made\n");
            add_client(connfd, &pool);
        }
    
        check_clients(&pool);   
    }

    
    exit(0);
}
/* $end serverimain */


/* functions for process request */

void init_pool(int listenfd, pool *p){
    int i;
    p->maxi = -1;
    for(i=0; i<FD_SETSIZE; i++) p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p){
    int i;
    p->nready--;

    for(i=0; i<FD_SETSIZE; i++){
        if(p->clientfd[i] < 0){
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            FD_SET(connfd, &p->read_set);

            if(connfd > p->maxfd) p->maxfd = connfd;

            if(i > p->maxi) p->maxi = i;

            break;
        }
    }

    if(i == FD_SETSIZE) app_error("add_client error : Too many clients");
}


void check_clients(pool *p){
    int i, connfd, n;
    rio_t rio;

    for(i=0; (i<=p->maxi) && (p->nready > 0); i++){
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))){
            p->nready--;
            char buf[MAXLINE];
            if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                byte_cnt += n;
                printf("Server received %d (%lld total) bytes on fd %d\n", n, byte_cnt, connfd);
                
                int cmdtype = parseCommand(buf);
                
                switch(cmdtype){
                    case BUY  : buy(connfd, getID(buf), getStockNum(buf));
                                break;

                    case SELL : sell(connfd, getID(buf), getStockNum(buf));
                                break;

                    case SHOW : show(connfd);
                                break;

                    case EXIT : memset(buf, '\0', MAXLINE * sizeof(char));
                                Rio_writen(connfd, buf, MAXLINE);
                                Close(connfd);
                                FD_CLR(connfd, &p->read_set);
                                p->clientfd[i] = -1;
                                break;
							
                    default : break;
                }
            }

            else{
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}

/* command parsing function start */
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
