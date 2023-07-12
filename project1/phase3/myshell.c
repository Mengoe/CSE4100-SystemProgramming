/* $begin shellmain */
#include "csapp.h"
#include "history.h"
#include <errno.h>
#define MAXARGS   128
#define MAXJOBS 100000
#define BG 0 //  running in background
#define FG 1 //  running in foreground
#define ST 2 //  stopped in background
#define TT 3 //  terminated

volatile sig_atomic_t hpid;
volatile pid_t foregroundGroup; // foregroundGroup의 pgid를 저장하고 있음. shell인 경우 항상 0

/* pipe 관련 변수 */
int saveStdin;
int isPipe = 0; // pipe 명령어인지 아닌지 구분

struct job{
	int sequence; // 해당 job 번호
	pid_t pid; // 해당 jop의 pid
	int state; // 해당 job의 state.  0(bg) : running, 1 : foreground, 2 : stopped, 3 : terminated
	char cmdline[MAXLINE]; 
};


/* parsing 관련 함수들 */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv, char*cmdline); 
char* handleQuoatation(char*st); // parsing에서 따옴표 처리해주는 부분
void handleAllQuoatation(char**argv); // argv를 받아서 따옴표에 대한 모든 예외처리 해주는 함수

/* history 관련 함수들 */
long long getCmdNum(char* cmd);
void findRecentHistory(char* tmp, char *cmdline);
void findCertainHistory(char *tmp, char* cmdline, long long num);


/* pipeline 관련 함수들*/
void treatPipeLine(char** argv, int readableEnd, int idx, sigset_t prev_one, int bg, char* cmdline);
int replaceMark(char**argv);

int parseSubArgv(char ** subArgv, char * tmpCmd);

/* signal 관련 함수들 */
void child_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);


/*job 관련 함수들*/
struct job jobList[MAXJOBS];
int jobSize = 0;
int sequence = 1;
void addjob(pid_t pid, char* cmdline, int state); // add job to jobList
void deletejob(pid_t pid); // delete job from jobList
void mybg(char** argv); // 해당 jobnumber에 해당하는 job을 running 상태로 바꿔줌.
void myfg(char** argv); // 해당 jobnumber에 해당하는 job을 foreground로 바꿔줌
void mykill(char** argv);
void printJobs(char** argv);         // jobList에 있는 모든 job들을 출력
struct job* getJob(pid_t pid); // 해당 pid에 해당하는 job을 return 해줌
struct job* getBgJob(int jobNumber); // 해당 jobnumber에 해당하는 job을 return 해줌
int getJobNumberFromQuery(char** argv, char*cmdline);

int main() 
{
	char cmdline[MAXLINE]; /* Command line */
	foregroundGroup = 0;
	
	
	Signal(SIGCHLD, child_handler);
	Signal(SIGINT, sigint_handler);
	Signal(SIGTSTP, sigtstp_handler);
	
	makeHistory();
	
    while (1) {
	/* Read */
		    
		Sio_puts("CSE4100-MP-P1> ");           
		fgets(cmdline, MAXLINE, stdin); 

		if (feof(stdin)){
			break;
		}

		/* Evaluate */
		eval(cmdline);
    }

	closeFile();
	
	exit(0); 
}

void child_handler(int sig){
	int olderrno = errno;

	sigset_t mask_all, prev_all;
	sigfillset(&mask_all);
	pid_t pid;
	int status;
	//Sio_puts("chld handler\n");

	while((pid = waitpid(-1, &status, WNOHANG)) > 0){
		// 자식 프로세스가 정상적으로 종료
		if(WIFEXITED(status)){
			//Sio_puts("please");
			hpid = pid;
			Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
			deletejob(hpid); // 종료됐으면 status TT로 바꿔줌
			Sigprocmask(SIG_SETMASK, &prev_all, NULL);
		}
	}

	errno = olderrno;
}

void sigtstp_handler(int sig){
	hpid = foregroundGroup;
	sigset_t mask_all, prev_all;
	sigfillset(&mask_all);
	
	if(foregroundGroup!=0){
		Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
		struct job *newBg = getJob(foregroundGroup);
		// background로 들어간 적이 없던 job 인 경우
		if(newBg->sequence == 0){
			newBg->state = TT; // 백그라운드 job으로 바뀐 기존의 job의 상태를 TT로 바꿔줌
			addjob(foregroundGroup, newBg->cmdline, ST);
			Sio_puts("\n");
			Sio_puts("[");
			Sio_putl(sequence-1);
			Sio_puts("]   Stopped   ");
			Sio_puts(newBg->cmdline);
			Sio_puts("\n");
		}

		// background에 들어간 적 있는 job인 경우
		else{
			newBg->state = ST; // 백그라운드 job으로 바뀐 기존의 job의 상태를 ST로 바꿔줌
			Sio_puts("\n");
			Sio_puts("[");
			Sio_putl(newBg->sequence);
			Sio_puts("]   Stopped   ");
			Sio_puts(newBg->cmdline);
			Sio_puts("\n");
		}

		Sigprocmask(SIG_SETMASK, &prev_all, NULL);
		Kill(-foregroundGroup, sig);
	}

	Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
	foregroundGroup = 0;
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	return;
}

void sigint_handler(int sig){
	
	hpid = foregroundGroup;
	sigset_t mask_all, prev_all;
	sigfillset(&mask_all);

	if(foregroundGroup!=0) Kill(-foregroundGroup, sig);

	if(foregroundGroup!=0) Sio_puts("\n");
	Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
	foregroundGroup = 0;
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	return;
}
/* $end shellmain */


/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
	
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 

	// argv에 대해 따옴표 처리하는 함수 만들어줘야함. echo he'll'o
    if (argv[0] == NULL)  
		return;   /* Ignore empty lines */
    
	// 따옴표 처리
	handleAllQuoatation(argv);

	/* 파이프 명령어 처리 */
	if(isPipe){
		sigset_t mask_one, prev_one;
		sigset_t mask_all, prev_all;

		Sigemptyset(&mask_one);
		Sigaddset(&mask_one, SIGCHLD);
		Sigfillset(&mask_all);
		saveStdin = dup(STDIN_FILENO);

		// 파이프에서 !! 어떻게 할지 고민
		if(!replaceMark(argv))
			addHistory(cmdline);
		
		Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
	
		treatPipeLine(argv, STDIN_FILENO, 0, prev_one, bg, cmdline); 

		Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
		foregroundGroup = 0;
		Sigprocmask(SIG_SETMASK, &prev_one, NULL); // sigchld unblock
		
		dup2(saveStdin, STDIN_FILENO);  // stdin이 사라져서 문제가 생겼던거
		return;
	}

	/* 파이프가 아닌 일반 명령어 처리 */
	else if (!builtin_command(argv, cmdline)) {
        
		addHistory(cmdline);

		sigset_t mask_one, prev_one;
		Sigemptyset(&mask_one);
		Sigaddset(&mask_one, SIGCHLD);
		Sigprocmask(SIG_BLOCK, &mask_one, &prev_one); // SIGCHLD Block
		
		if((pid = Fork()) == 0){
			// ctrl+c, ctrl+z unblock 필요
			Sigprocmask(SIG_SETMASK, &prev_one, NULL);
			Signal(SIGCHLD, SIG_DFL);
			Signal(SIGTSTP, SIG_DFL);
			Signal(SIGINT, SIG_DFL);
			if(strcmp(argv[0], "less"))
				Setpgid(0, 0);

			
			if(execvp(argv[0], argv) < 0){
				fprintf(stderr, "%s: command not found\n", argv[0]);
				exit(0);
			}
		}

		/* addjob 함수에서 필요 
			a.out     -al &
			a.out -al
			등은 job을 출력했을 시 간격이 똑같아야 함.
		*/

		char concateArray[MAXLINE] = {'\0'};
		for(int i=0; argv[i]; i++){
			strcat(concateArray, argv[i]);
			strcat(concateArray, " ");
		}

	/* Parent waits for foreground job to terminate */
		if(!bg){
			sigset_t mask_all, prev_all;
			sigfillset(&mask_all);
			int status;
			Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
			addjob(pid, concateArray, FG);
			foregroundGroup = pid;
			Sigprocmask(SIG_SETMASK, &prev_all, NULL);

			hpid = 0;
			while(!hpid) 
				Sigsuspend(&prev_one);
			
			
			Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
			foregroundGroup = 0;
			Sigprocmask(SIG_SETMASK, &prev_all, NULL);

			Sigprocmask(SIG_SETMASK, &prev_one, NULL);
			
		}

		else{
			sigset_t mask_all;
			sigfillset(&mask_all);

			Sigprocmask(SIG_BLOCK, &mask_all, NULL);
			addjob(pid, concateArray, BG);
			Sigprocmask(SIG_SETMASK, &prev_one, NULL);

			return;
		}
	}
	
	
	
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv, char*cmdline) 
{
	if(!strcmp(argv[0], "exit")) exit(0);

	// history 나열
	if(!strcmp(argv[0], "history")){
		if(argv[1] == NULL){
			addHistory(argv[0]);
			printHistory();
		}

		else{
			int cnt=1; 
			Sio_puts("myshell: ");
			char* cur = argv[cnt++];
			while(cur!=NULL){
				
				Sio_puts(cur);
				Sio_puts(": ");
				cur = argv[cnt++];
			}
			Sio_puts("event not found");
			Sio_puts("\n");
		}

		return 1;
	}
		
	else if(argv[0][0] == '!'){

		if(strlen(argv[0])==1) return 0;

		// '!!' 함수 구현
		if(argv[0][1] == '!'){
			// * 길이 고려
			char tmp[MAXLINE] = {'\0'};
			findRecentHistory(tmp, cmdline);
			if(tmp[0]  == '\0') return 1;
			Sio_puts(tmp);
			eval(tmp); // 새롭게 만들어진 명령어 다시 수행
			addHistory(tmp);
		}

		// !# 구현
		else if('0' <= argv[0][1] && argv[0][1] <= '9'){
			long long num = getCmdNum(argv[0]);
			
			if(num == 0) return 0;

			char tmp[MAXLINE] = {'\0'};
			findCertainHistory(tmp, cmdline, num);
			Sio_puts(tmp);
			eval(tmp);
			addHistory(tmp);
		}

		// !sf 등등 구현하지 않는 경우 command not found 출력.
		else return 0;

		return 1;
	}

	// built_in cd 구현
	else if(!strcmp(argv[0], "cd")){
		addHistory(cmdline);
		// 홈 디렉토리로 이동
		if(argv[1] == NULL){
			// 수정 필요
			chdir(getenv("HOME"));
		}

		else{
			if((chdir(argv[1]) == -1)){
				printf("myshell: cd: %s: No such file or directory\n", argv[1]);
				return 1;
			}
		}

		return 1;
	}

	else if (!strcmp(argv[0], "&")){    /* Ignore singleton & */
		addHistory(argv[0]);
		return 1;
	}

	else if(!strcmp(argv[0], "jobs")){
		printJobs(argv);
		addHistory(argv[0]);
		return 1;
	}

	else if(!strcmp(argv[0], "bg")){
		mybg(argv);
		addHistory(cmdline);
		return 1;
	}

	else if(!strcmp(argv[0], "fg")){
		myfg(argv);
		addHistory(cmdline);
		return 1;
	}

	else if(!strcmp(argv[0], "kill")){
		mykill(argv);
		addHistory(cmdline);
		return 1;
	}
	
    return 0;                     /* Not a builtin command */
}
/* $end eval */


/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int bg;              /* Background job? */
	isPipe = 0;			 /* PipeLine command? */
    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */

    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;

	int argc=0;            /* Number of args */

    /* Build the argv list */
    while ((delim = strchr(buf, ' '))) {
		char* quot;

		if(*buf == '\"' || *buf == '\''){
			quot = handleQuoatation(buf);
			if(quot != NULL && (*quot == '\0' || *quot == ' ')){
				argv[argc++] = buf;
				// " 다음이 문자의 끝인경우
				if(*quot == '\0') break;

				else{
					*quot = '\0';
					buf = quot+1;
					while(*buf && (*buf == ' ')) buf++;
					continue;
				}
			}
		}

		argv[argc++] = buf;
		*delim = '\0';
		// pipe 명령어인지 아닌지 파악
		if(argc != 1 && !strcmp(argv[argc-1], "|")) isPipe = 1;

		buf = delim + 1;
		while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    
	argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
		return 1;

    /* Should the job run in the background? */
	// sort & 와 같이, & 가 space 로 구분돼 있는 경우
    if ((bg = (*argv[argc-1] == '&')) != 0){
		if(argc != 1)
		argv[--argc] = NULL;
	}
	
	else if(argv[argc-1][strlen(argv[argc-1])-1] == '&'){
		bg = 1;
		argv[argc-1][strlen(argv[argc-1])-1] = '\0';
		//printf("%s\n", argv[argc-1]);
	}

    return bg;
}
/* $end parseline */

/* handle "a " or "  a" or " a " etc..*/
char* handleQuoatation(char*st){
	char* trav = st;
	trav++;

	// " 나 ' 일 경우, 대응되는 짝이 있는지 찾아나감. 없으면 trav == '\0' 이 될거임
	while(*trav && *trav != *st) trav++;

	if(*trav == '\0') return NULL;

	// 짝이 있으면 그 다음 위치 return
	else return ++trav;
}
/* $end handleQuoation */

/*argv를 받아서 따옴표에 대한 모든 예외처리 해주는 함수*/

void handleAllQuoatation(char**argv){
	/* argv 의 끝에 " " 나 ' ' 로 감싸져 있는 경우 */
	for(int i=0; argv[i]; i++){
			//printf("sub : %s\n", subArgv[i]);

		if(argv[i][0] == '\"' && argv[i][strlen(argv[i])-1] == '\"'){
			char tmp[MAXLINE] = {'\0'};
			int j;
			for(int j=1; j < strlen(argv[i])-1; j++){
				tmp[j-1] = argv[i][j];
			}
						
			tmp[j-1] = '\0';
			strcpy(argv[i], tmp);
		}

		else if(argv[i][0] == '\'' && argv[i][strlen(argv[i])-1] == '\''){
			char tmp[MAXLINE] = {'\0'};
			int j;
			for(int j=1; j < strlen(argv[i])-1; j++){
				tmp[j-1] = argv[i][j];
			}
						
			tmp[j-1] = '\0';
			strcpy(argv[i], tmp);
		}
	}

	/* argv의 내부에 ' ' 로 감싸진 문자열이 있는 경우 */
	for(int i=0; argv[i]; i++){
		char *tmp = argv[i];

		while(*tmp){
			if(*tmp == '\'' || *tmp == '\"') break;
			tmp++;
		}

		// 문장 내부에 ' '의 쌍등이 없는 경우
		if(*tmp == '\0') continue;

		char* trav = tmp+1;
		while(*trav){
			if(*trav == *tmp) break;
			trav++;
		}

		if(*trav == '\0') continue;

		char rgv[MAXLINE] = {'\0'};
		int cnt=0;
		tmp = argv[i];

		while(*tmp){
			if(*tmp != *trav) rgv[cnt++] = *tmp;
			tmp++;
		}

		rgv[cnt] = '\0';
		strcpy(argv[i], rgv);
		//printf("argv[i] : %s\n", argv[i]);
	}
}

// pipe 에서의 !!, !# 처리
int replaceMark(char**argv){
	int flag = 0;
	for(int i=0; argv[i]!=NULL; i++){
		if(argv[i][0] == '!'){

			if(strlen(argv[i])==1) continue;

			if(argv[i][1] == '!'){
				argv[i] = (char*)malloc(sizeof(char) * MAXLINE);
				strcpy(argv[i], getRecentHistory());
				flag = 1;
			}

			else if('0' <= argv[i][1] && argv[i][1] <= '9'){
				long long num = getCmdNum(argv[i]);
			
				if(num == 0) continue;

				argv[i] = (char*)malloc(sizeof(char) * MAXLINE);
				strcpy(argv[i], getCertainHistory(num));
				flag = 1;
			}
		}
	}

	if(flag){
      	char tmpCmd[MAXLINE] = {'\0'};
      	for(int i=0; argv[i]!=NULL; i++){
        	strcat(tmpCmd, argv[i]);
         	strcat(tmpCmd, " ");
      	}

		printf("%s\n", tmpCmd);
      	addHistory(tmpCmd);
  	}
	
	return flag;
}


long long getCmdNum(char* cmd){

	//history 가 비어있는 경우 command not found
	if(getSize() == 0) return 0;

	char* firstNoneNum = cmd; // 첫번째로 문자가 등장하는 않는부분 찾음
	char* savedFirstNonnum;
	while(*firstNoneNum && *firstNoneNum != '!') firstNoneNum++;
			
	firstNoneNum++;
	
	int cnt = 0; // 숫자의 길이
	long long num=0;
	long long mpow=1;
	// !00001 과 같은 경우 앞의 0을 제거
	while(*firstNoneNum == '0'){
		firstNoneNum++;
	}

	savedFirstNonnum = firstNoneNum;

	// 숫자의 길이 찾기
	while(*firstNoneNum && ('0' <= *firstNoneNum && *firstNoneNum <= '9')){
		cnt++;
		firstNoneNum++;
	}

	for(int i=1; i<cnt; i++) mpow *= 10;


	while(*savedFirstNonnum && ('0' <= *savedFirstNonnum && *savedFirstNonnum <= '9')){
		num += mpow * (*savedFirstNonnum-'0');
		
		// 해당 번호가 history의 size 보다 큰 경우 command not found
		if(num > getSize()){
			//printf("num is %lld\n", num);
			return 0;
		}
		mpow /= 10;
		savedFirstNonnum++;
	}

	
	return num;
}

void findRecentHistory(char* tmp, char *cmdline){
		
		char recentHistory[MAXLINE] = {'\0'};

		// historyList->size 했더니 segfault 뜸
		if(getSize() == 0){
			Sio_puts("bash: !!: event not found\n");
			return;
		}

		strcpy(recentHistory, getRecentHistory());
		if(!strcmp(recentHistory, "!")){
			Sio_puts("!");
			Sio_puts("\n");
			return;
		}
		
		// 출력물엔, 앞의 스페이스도 모두 포함해야 함.
		char* trav = cmdline;
		while(*trav && *trav == ' ') trav++;
		strncpy(tmp, cmdline, trav-cmdline);
		strcat(tmp, recentHistory);
		
		int cnt=0;
		while(*trav && *trav == '!'){
			cnt++;
			trav++;
			
			if(cnt==2){
				break;
			}
		}
		strcat(tmp, trav);	
		return;

}

void findCertainHistory(char* tmp, char* cmdline, long long num){
	char* firstNoneNum = cmdline; // 첫번째로 문자가 등장하는 않는부분 찾음
	while(*firstNoneNum && *firstNoneNum != '!') firstNoneNum++;

	strncpy(tmp, cmdline, firstNoneNum-cmdline);
	firstNoneNum++;

	while(*firstNoneNum && ('0' <= *firstNoneNum && *firstNoneNum <= '9')) firstNoneNum++;			
	strcat(tmp, getCertainHistory(num));
	strcat(tmp, firstNoneNum);
	return;
}

int parseSubArgv(char ** subArgv, char * tmpCmd){
   char *delim;         /* Points to first space delimiter */
     
	int argc=0;            /* Number of args */

    /* Build the argv list */
    while ((delim = strchr(tmpCmd, ' '))) {
		subArgv[argc++] = tmpCmd;
		*delim = '\0';
		
		tmpCmd = delim + 1;
		while (*tmpCmd && (*tmpCmd == ' ')) /* Ignore spaces */
            tmpCmd++;
      if(*tmpCmd == '\0') break;
   }
    
	subArgv[argc] = NULL;
   return argc;
}

void treatPipeLine(char** argv, int readableEnd, int idx, sigset_t prev_one, int bg, char* cmdline){
	int istherePipe = 0;
	int nxt_idx;
	char* subArgv[MAXARGS];
	char tmpCmd[MAXLINE] = {'\0'};
	int subArgc=0;
	int fds[2];
	pid_t pid;
	
	for(int i=idx; argv[i] != NULL ; i++){
      if(!strcmp(argv[i], "|")){
         istherePipe = 1;
         nxt_idx = i+1;
         break;
      }

      //subArgv[subArgc++] = argv[i];
      strcat(tmpCmd, argv[i]);
      strcat(tmpCmd, " ");
   }

   subArgc = parseSubArgv(subArgv, tmpCmd);

	// 마지막 명령어
	if(!istherePipe){
		
		if(readableEnd != STDIN_FILENO){
			Dup2(readableEnd, STDIN_FILENO);
			Close(readableEnd);
		}


		if((pid=Fork()) == 0){

			Sigprocmask(SIG_SETMASK, &prev_one, NULL);
			Signal(SIGCHLD, SIG_DFL);
			Signal(SIGTSTP, SIG_DFL);
			Signal(SIGINT, SIG_DFL);
			
			
			if(strcmp(subArgv[0], "less"))
				Setpgid(0, 0);

			
			if(!strcmp(subArgv[0], "history")){
				// 올바른 history 일경우 
				if(subArgc == 1){
					printHistory();
				}

				else{
					int cnt=1;
					printf("myshell: ");
					char* cur = argv[cnt++];
					while(cur!=NULL){
						//Sio_puts("%s: ", cur);
						printf("%s:", cur);
						cur = argv[cnt++];
					}

					printf("event not found\n");
				}

				exit(0);
			}

			// built_in cd 일 경우
			else if(!strcmp(subArgv[0], "cd")){
				exit(0);
			}

		
			if(execvp(subArgv[0], subArgv) < 0){
            	fprintf(stderr, "%s: command not found\n", subArgv[0]);
            	exit(1);
         	}
		}

		else{
			sigset_t mask_all, prev_all;
			sigfillset(&mask_all);

			// fg pipe line인 경우
			if(!bg){
				pid_t tpid;
				Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
				foregroundGroup = pid;
				addjob(pid, cmdline, FG);
				Sigprocmask(SIG_SETMASK, &prev_all, NULL);


				hpid = 0;
				while(!hpid)
					Sigsuspend(&prev_one);

				Sigprocmask(SIG_SETMASK, &prev_one, NULL);

				// 마지막 프로세스가 종료될 때까지 대기, Race Condition 방지
				while(hpid != pid) ;
			}

			else{
				Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
				addjob(pid, cmdline, BG);
				Sigprocmask(SIG_SETMASK, &prev_all, NULL);
			}

			return;
		}
	}

	else{
		if(pipe(fds) < 0){
			unix_error("pipe error");
		}

		if((pid = Fork()) == 0){
			Sigprocmask(SIG_SETMASK, &prev_one, NULL);
			Signal(SIGCHLD, SIG_DFL);
			Signal(SIGTSTP, SIG_DFL);
			Signal(SIGINT, SIG_DFL);
			
			if(strcmp(subArgv[0], "less"))
				Setpgid(0, 0);
			
			Close(fds[0]);

			if(readableEnd != STDIN_FILENO){
				Dup2(readableEnd, STDIN_FILENO);
				Close(readableEnd);
			}
			
			if(fds[1] != STDOUT_FILENO){
				Dup2(fds[1], STDOUT_FILENO);
				Close(fds[1]);
			}

			if(!strcmp(subArgv[0], "history")){
				// 올바른 history 일경우 
				if(subArgc == 1){
					printHistory();
				}

				else{
					int cnt=1;
					printf("myshell: ");
					char* cur = argv[cnt++];
					while(cur!=NULL){
						//Sio_puts("%s: ", cur);
						printf("%s:", cur);
						cur = argv[cnt++];
					}

					printf("event not found\n");
				}

				exit(0);
			}

			// built_in cd 일 경우
			else if(!strcmp(subArgv[0], "cd")){
				exit(0);
			}

			if(execvp(subArgv[0], subArgv) < 0){
            	fprintf(stderr, "%s: command not found\n", subArgv[0]);
            	exit(1);
         	}
			
		}

		else{
			Close(readableEnd);
			Close(fds[1]);
			treatPipeLine(argv, fds[0], nxt_idx, prev_one, bg, cmdline);
		}
	}
}



/* job 관련 함수들 */

void printJobs(char** argv){
	if(argv[1] != NULL){
		printf("bash: jobs: ");

		for(int i=1; argv[i]; i++) printf("%s: ", argv[i]);
		printf("no such job\n");
		return;
	}

	for(int i=0; i<jobSize; i++){
		if(jobList[i].state == TT || jobList[i].state == FG) continue;
		printf("[%d]   ", jobList[i].sequence);
		if(jobList[i].state == BG) printf("%s   ", "Running");
		else if(jobList[i].state == ST) printf("%s   ", "Stopped");

		printf("%s\n", jobList[i].cmdline);
	}
}

void addjob(pid_t pid, char* cmdline, int state){
	if(jobSize > MAXJOBS){
		printf("Job is already full.\n");
		return;
	}

	jobList[jobSize].pid = pid;

	/* FG로 들어온 job은 0, BG로 들어온 job은 sequence를 가짐 */
	if(state == BG || state == ST)
		jobList[jobSize].sequence = sequence;
	else
		jobList[jobSize].sequence = 0; 

	jobList[jobSize].state = state;
	strcpy(jobList[jobSize].cmdline, cmdline);
	jobSize++;

	// 어떤 job이 백그라운드로 들어올때만 실제 크기 증가시킴
	if(state != FG) sequence++;
}

void deletejob(pid_t pid){
	for(int i=0; i<jobSize; i++){
		if(jobList[i].pid == pid){
			jobList[i].state = TT;
		}
	}
}

void myfg(char** argv){
	int flag=0;
	// fg만 입력했을 경우 return
	if(argv[1] == NULL){
		printf("No Such Job\n");
		return;
	}

	if(argv[1][0] != '%') flag=1;
	if(argv[1][1] < '0' || argv[1][1] > '9') flag=1;

	if(flag){
		printf("No Such Job\n");
		return;
	}


	sigset_t mask_all, prev_all;
	int jobNum = getJobNumberFromQuery(argv, argv[0]);
	if(!jobNum) return;

	
	Sigfillset(&mask_all);
	Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
	struct job* myjob = getBgJob(jobNum);
	myjob->state = FG;
	Sio_puts("[");
	Sio_putl(myjob->sequence);
	Sio_puts("]   ");
	Sio_puts("Running   ");
	Sio_puts(myjob->cmdline);
	Sio_puts("\n");
	Kill(-myjob->pid, SIGCONT);
	foregroundGroup = myjob->pid; // foregroundGroup을 바꿔줌
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);

	waitpid(foregroundGroup, NULL, WUNTRACED);

	Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
	foregroundGroup = 0;
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
}

void mybg(char** argv){
	int flag=0;
	if(argv[1] == NULL){
		printf("No Such Job\n");
		return;
	}

	if(argv[1][0] != '%') flag=1;
	if(argv[1][1] < '0' || argv[1][1] > '9') flag=1;

	if(flag){
		printf("No Such Job\n");
		return;
	}


	sigset_t mask_all, prev_all;
	int jobNum = getJobNumberFromQuery(argv, argv[0]);
	if(!jobNum) return;

	

	Sigfillset(&mask_all);
	Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
	struct job* myjob = getBgJob(jobNum);
	myjob->state = BG; // job의 상태를 ST 에서 BG로 바꿈
	Sio_puts("[");
	Sio_putl(myjob->sequence);
	Sio_puts("]   ");
	Sio_puts("Running   ");
	Sio_puts(myjob->cmdline);
	Sio_puts("\n");
	Kill(-myjob->pid, SIGCONT);
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	
}

void mykill(char** argv){
	int flag=0;
	if(argv[1] == NULL){
		printf("No Such Job\n");
		return;
	}
	
	if(argv[1][0] != '%') flag=1;
	if(argv[1][1] < '0' || argv[1][1] > '9') flag=1;

	if(flag){
		printf("No Such Job\n");
		return;
	}


	sigset_t mask_all, prev_all;
	int jobNum = getJobNumberFromQuery(argv, argv[0]);
	if(!jobNum) return;

	

	Sigfillset(&mask_all);
	
	Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
	struct job* myjob = getBgJob(jobNum);
	myjob->state = TT;
	Kill(-myjob->pid, SIGKILL);
	Sigprocmask(SIG_SETMASK, &prev_all, NULL);
	

}


struct job* getJob(pid_t pid){ // 해당 pid에 해당하는 job을 return 해줌
	for(int i=0; i<jobSize; i++){
		// ctrl+z 는 fg job에만 적용되므로, jobList중 fg만 찾으면 된다.
		if(jobList[i].pid == pid && jobList[i].state == FG){
			return &jobList[i];
		}
	}
}


struct job* getBgJob(int jobNumber){
	struct job* tmp;

	for(int i=0; i<jobSize; i++){
		if(jobList[i].sequence == jobNumber){
			return &jobList[i];
		}
	}
}

int getJobNumberFromQuery(char** argv, char*cmdline){
	int jobNum=0;
	int numLength=0;
	int mpow=1;
	char*tmp = argv[1]+1;

	// bg %00001 등과 같이 앞의 9을 제거
	while(*tmp && *tmp == '0') tmp++;

	char*trav = tmp;
	char*saveT = trav;

	while(*trav && ('0' <= *trav && *trav <= '9')){
		numLength++;
		trav++;
	}

	//printf("numlenght : %d\n", numLength);
	if(numLength == 0) return 0;

	for(int i=0; i<numLength-1; i++) mpow*=10;

	while(mpow > 0) {
		jobNum = jobNum + mpow*(*saveT-'0');
		saveT++;
		mpow/=10;
	}

	if(jobNum > sequence-1){
		printf("bash: %s: %%%d: Nu Such Job\n", cmdline, jobNum);
		return 0;
	}

	// 해당 job이 이미 종료된 경우
	if(getBgJob(jobNum)-> state == TT){
		printf("No Such Job\n");
		return 0;
	}

	return jobNum;
}
