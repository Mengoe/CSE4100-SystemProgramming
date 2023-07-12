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
        (*tmptable)->readcnt = 0;
        Sem_init(&((*tmptable)->cnt_mutex), 0, 1);
        Sem_init(&((*tmptable)->mutex), 0, 1);
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
    node->readcnt = 0;
    Sem_init(&(node->cnt_mutex), 0, 1);
    Sem_init(&(node->mutex), 0, 1);

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

    if(prev->ID < ID){
        prev->right = node;
    }

    else{
        prev->left = node;
    }

    return;
}

/* stock manage function */

// stock read function
void getTable(item* cur, char (*buf)[MAXLINE]){
    char tmp[8192] = {'\0'};

    if(cur!=NULL){
        getTable(cur->left, buf);  
        
        
        P(&(cur->cnt_mutex));
        cur->readcnt += 1;
        if(cur->readcnt == 1) P(&(cur->mutex));
        V(&(cur->cnt_mutex));
        
        // C.S
        sprintf(tmp, "%d %d %d\n", cur->ID, cur->cnt, cur->price);
        
        P(&(cur->cnt_mutex));
        cur->readcnt -= 1;
        if(cur->readcnt == 0) V(&(cur->mutex));
        V(&(cur->cnt_mutex));
        
        strcat(*buf, tmp);
        getTable(cur->right, buf);
    }
}

// stock read & write function
int buyStock(int ID, int buyNum, item* stockTable){
    item* snode = findStock(ID, stockTable);

    if(snode == NULL) return 0;
    
    P(&(snode->mutex));

    // C.S
    if(snode->cnt < buyNum){
        V(&(snode->mutex));
        return 0;
    }
    snode->cnt -= buyNum;

    V(&(snode->mutex));

    return 1;
}

// stock read & write function
void sellStock(int ID, int sellNum, item* stockTable){
    item* snode = findStock(ID, stockTable);
    
    if(snode == NULL) return;
    
    P(&(snode->mutex));
    snode->cnt += sellNum;
    V(&(snode->mutex));
    
    return;
}

// stock read function
void updateTable(item* cur){
    char buf[8192] = {'\0'};

    if(cur!=NULL){
        updateTable(cur->left);

        P(&(cur->mutex));
        sprintf(buf, "%d %d %d\n", cur->ID, cur->cnt, cur->price);
        V(&(cur->mutex));
        fprintf(writeFile, "%s", buf);  

        updateTable(cur->right);
    }
}

