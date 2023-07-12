#include "history.h"

void makeHistory(){
	char line[MAXLINE];
	
	writeHistory = fopen("historyLog.txt", "a");
	if(writeHistory == NULL){
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}

	readHistory = fopen("historyLog.txt", "r");
	if(readHistory == NULL){
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}
	
	historySize = 0;

	while(fgets(line, MAXLINE, readHistory) != NULL){
		historySize++;
		line[strlen(line)-1] = '\0'; // '\n'을 없애줌
		strcpy(saveLastHistory, line);
	}

	return;
}

long long getSize(){
	return historySize;
}

void addHistory(char* cmd){
	
	// 파일에 쓸 때는 %s\n 형태로 쓰므로, '\n'을 제거
	char* trav = cmd;
	while(*trav){
		if(*trav == '\n'){
			*trav = '\0';
			break;
		}
		trav++;
	}

	// leading space 제거
	while(*cmd && *cmd == ' ') cmd++;

	//   !!    <- 와 같이 뒤에 공백 제거
	trav = cmd;
	while(*trav) trav++;
	
	trav--;
	while(*trav && *trav == ' ') trav--;
	trav++;
	*trav = '\0';
	
	// 중복이면 저장 안함
	if(!strcmp(cmd, saveLastHistory)){
		return;
	}

	strcpy(saveLastHistory, cmd);
	historySize++;
	
	fprintf(writeHistory, "%s\n", saveLastHistory);
	fflush(writeHistory); // historyLog에 바로 적용되게 하기 위해 버퍼를 비워줘야 함.

}

char* getRecentHistory(){
	return saveLastHistory;
}

char* getCertainHistory(long long lineNum){
	long long cnt = 1;
	char line[MAXLINE];
	fseek(readHistory, 0L, SEEK_SET);

	while(cnt <= lineNum){
		fgets(line, MAXLINE, readHistory);
		line[strlen(line)-1] = '\0';
		strcpy(saveCertainHistory, line);
		cnt++;
	}


	return saveCertainHistory;
}

void printHistory(){
	long long cnt=1;
	fseek(readHistory, 0L, SEEK_SET);

	char line[MAXLINE];

	// stdout stream으로 출력
	while(fgets(line, MAXLINE, readHistory)){
		dprintf(STDOUT_FILENO, "%lld %s", cnt++, line);
	}

	
	return;
}

void closeFile(){
	if(fclose(writeHistory) == EOF){
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}

	if(fclose(readHistory) == EOF){
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}
}




