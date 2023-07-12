// Shell history function header file

#ifndef __HISTORY_H__
# define __HISTORY_H__

#define MAXLINE 8192
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
long long historySize;
char saveLastHistory[MAXLINE];
char saveCertainHistory[MAXLINE];
FILE* readHistory;
FILE* writeHistory;


long long getSize();
void makeHistory(); // historyLog를 읽어 마지막 문자와 Log의 크기를 저장
void addHistory(char*cmd); // 해당 커맨드를 historyList에 추가
char* getRecentHistory(); // 마지막 History return
char* getCertainHistory(long long lineNum); // lineNun에 해당하는  History return
void printHistory();
void closeFile();

#endif
