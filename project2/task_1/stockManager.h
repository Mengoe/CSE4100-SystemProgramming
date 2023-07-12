#ifndef __STOCKMANAGER_H__
#define __STOCKMANAGER_H__

#include "csapp.h"
#define MAXSTOCK 100

// stocktable 저장 위한 함수 정의

typedef struct _item{
    int ID;
    int cnt;
    int price;
    struct _item* left;
    struct _item* right;
} item;


/* table manage function */
item* findStock(int ID, item* cur);
void insertStock(int ID, int price, int cnt, item**tmptable); 
item* makeTable(); // program 시작 시 table 구성

/* stock manage function */
void getTable(item* cur, char (*buf)[MAXLINE]);
int buyStock(int ID, int buyNum, item* stocktable);
void sellStock(int ID, int sellNum, item* stocktable);
void updateTable(item* stockTable);

FILE* readFile;
FILE* writeFile;

#endif /* __STOCKMANAGER_H__ */
/* $end csapp.h */