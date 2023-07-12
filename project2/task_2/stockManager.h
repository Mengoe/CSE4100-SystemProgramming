#ifndef __STOCKMANAGER_H__
#define __STOCKMANAGER_H__

#include "csapp.h"

#define MAXSTOCK 100000

// stocktable 저장 위한 함수 정의

typedef struct _item{
    int ID;
    int cnt;
    int price;
    struct _item* left;
    struct _item* right;
    sem_t mutex; /* mutex for node */
    sem_t cnt_mutex; /* mutex for resolving reader-writers problem */
    int readcnt;
} item;


/* table manage function */
item* findStock(int ID, item* cur); // table에서 ID 에 해당하는 stock 을 찾아서, struct를 return
void insertStock(int ID, int price, int cnt, item**tmptable); // table에 새로운 주식 삽입
item* makeTable(); // program 시작 시 table 구성
void printTable(item* cur); // table 확인용

/* stock manage function */
void getTable(item* cur, char (*buf)[MAXLINE]);
int buyStock(int ID, int buyNum, item* stocktable);
void sellStock(int ID, int sellNum, item* stocktable);
void updateTable(item* stockTable);

FILE* readFile;
FILE* writeFile;
int readcnt;
sem_t s_for_read_cnt; // for readcnt
#endif /* __STOCKMANAGER_H__ */
/* $end csapp.h */