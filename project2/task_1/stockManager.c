#include "stockManager.h"

/* table manage function */

item* makeTable(){
    readFile = fopen("stock.txt", "r");

    char buf[MAXLINE];
    item* tmptable = NULL;

    if(readFile == NULL){
        fprintf(stderr, "fileopen error!\n");
        exit(1);
    }

    while(fgets(buf, MAXLINE, readFile) != NULL){
        int id, cnt, price;
        sscanf(buf, "%d %d %d", &id, &cnt, &price);
        insertStock(id, cnt, price, &tmptable);
    }

    return tmptable;
}

item* findStock(int ID, item* cur){
    if(cur == NULL || cur->ID == ID) return cur;

    if(cur->ID < ID) return findStock(ID, cur->right);

    return findStock(ID, cur->left);
}

void insertStock(int ID, int cnt, int price, item**tmptable){
    if(*tmptable == NULL){
        (*tmptable) = (item*)malloc(sizeof(item));
        (*tmptable)->ID = ID;
        (*tmptable)->price = price;
        (*tmptable)->cnt = cnt;
        (*tmptable)->left = NULL;
        (*tmptable)->right = NULL;
        return;
    }

    item* node = (item*)malloc(sizeof(item));
    item* prev = NULL;
    item* temp = (*tmptable);

    node->ID = ID;
    node->price = price;
    node->cnt = cnt;
    node->left = NULL;
    node->right = NULL;

    while(temp){
        if(ID > temp->ID){
            prev = temp;
            temp = temp->right;
        }

        else if(ID < temp->ID){
            prev = temp;
            temp = temp->left;
        }
    }

    if(prev->ID < ID) prev->right = node;
    else prev->left = node;

    return;
}

/* stock manage function */
void getTable(item* cur, char (*buf)[MAXLINE]){
    char tmp[8192] = {'\0'};

    if(cur!=NULL){
        getTable(cur->left, buf);
        
        sprintf(tmp, "%d %d %d\n", cur->ID, cur->cnt, cur->price);
        strcat(*buf, tmp);
        getTable(cur->right, buf);
    }
}

int buyStock(int ID, int buyNum, item* stockTable){
    item* snode = findStock(ID, stockTable);  

    if(snode == NULL) return 0;
    if(snode->cnt < buyNum) return 0;

    snode->cnt -= buyNum;

    return 1;
}

void sellStock(int ID, int sellNum, item* stockTable){
    item* snode = findStock(ID, stockTable);
    if(snode == NULL) return;
    snode->cnt += sellNum;
    return;
}


void updateTable(item* cur){
    char buf[8192] = {'\0'};

    if(cur!=NULL){
        updateTable(cur->left);
        sprintf(buf, "%d %d %d\n", cur->ID, cur->cnt, cur->price);
        fprintf(writeFile, "%s", buf);    
        updateTable(cur->right);
    }
}