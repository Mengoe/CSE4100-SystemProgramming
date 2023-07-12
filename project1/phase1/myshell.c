/* $begin shellmain */
#include "csapp.h"
#include "history.h"
#include <errno.h>
#define MAXARGS   128
volatile pid_t foregroundGroup;


/* parsing 관련 함수들 */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv, char*cmdline); 
char* handleQuoatation(char*st);
void handleAllQuoatation(char**argv);

/* history 관련 함수들 */
long long getCmdNum(char* cmd);
void findRecentHistory(char* tmp, char *cmdline);
void findCertainHistory(char *tmp, char* cmdline, long long num);

/* handler 관련 함수들*/
void sigint_handler(int sig);
void sigtstp_handler(int sig);

int main() 
{
   char cmdline[MAXLINE]; /* Command line */
   foregroundGroup = 0;

   Signal(SIGINT, sigint_handler);
   Signal(SIGTSTP, sigtstp_handler);
   makeHistory();

   while (1) {
   /* Read */
          
      printf("CSE4100-MP-P1> ");           
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


/* handler */
void sigint_handler(int sig){
	if(foregroundGroup!=0) Kill(-foregroundGroup, sig);
	if(foregroundGroup!=0) Sio_puts("\n");
	return;
}

void sigtstp_handler(int sig){
   if(foregroundGroup!=0){
      Kill(-foregroundGroup, sig);
      Sio_puts("\n");
   }
}

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

    if (argv[0] == NULL)  
      return;   /* Ignore empty lines */
    
   
   handleAllQuoatation(argv);
   

   if (!builtin_command(argv, cmdline)) {
        addHistory(cmdline);
      
      if((pid = Fork()) == 0){
         Signal(SIGINT, SIG_DFL);
         Signal(SIGTSTP, SIG_DFL);

         if(strcmp(argv[0], "less"))
            Setpgid(0, 0);

         if(execvp(argv[0], argv) < 0){
            fprintf(stderr, "%s: command not found\n", argv[0]);
            exit(0);
         }
      }

   /* Parent waits for foreground job to terminate */
      if (!bg){ 
         int status;
         sigset_t mask_all, prev_all;
         Sigfillset(&mask_all);

         Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
			foregroundGroup = pid;
			Sigprocmask(SIG_SETMASK, &prev_all, NULL);
 
         Waitpid(pid, &status, WUNTRACED);        

         Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
			foregroundGroup = 0;
			Sigprocmask(SIG_SETMASK, &prev_all, NULL);

      }

   }
   
   return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv, char*cmdline) 
{
   if(!strcmp(argv[0], "exit")) exit(0);

   
   else if(!strcmp(argv[0], "history")){
      if(argv[1] == NULL){
         addHistory(argv[0]);
         printHistory();
      }

      else{
         int cnt=1; 
         printf("myshell: ");
         char* cur = argv[cnt++];
         while(cur!=NULL){
            //Sio_puts("%s: ", cur);s
            printf("%s", cur);
            printf(": ");
            cur = argv[cnt++];
         }
         printf("event not found");
         printf("\n");
      }

      return 1;
   }
      
   else if(argv[0][0] == '!'){

      if(strlen(argv[0])==1) return 0;

      // '!!' 구현
      if(argv[0][1] == '!'){
         // * 길이 고려
         char tmp[MAXLINE] = {'\0'};
         findRecentHistory(tmp, cmdline);
         if(tmp[0]  == '\0') return 1;
         printf("%s", tmp);
         eval(tmp); // 새롭게 만들어진 명령어 다시 수행
         addHistory(tmp);
      }

      // !# 구현
      else if('0' <= argv[0][1] && argv[0][1] <= '9'){
         long long num = getCmdNum(argv[0]);
         
         if(num == 0) return 0;

         char tmp[MAXLINE] = {'\0'};
         findCertainHistory(tmp, cmdline, num);
         printf("%s", tmp);
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

   
   return 0;                     /* Not a builtin command */
}
/* $end eval */


/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
   char *delim;         /* Points to first space delimiter */
   int bg;              /* Background job? */
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
	
   // sort& 등과 같이 &가 붙어 있는 경우
	else if(argv[argc-1][strlen(argv[argc-1])-1] == '&'){
		bg = 1;
		argv[argc-1][strlen(argv[argc-1])-1] = '\0';
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

void handleAllQuoatation(char**argv){
	/* argv 의 끝에 " " 나 ' ' 로 감싸져 있는 경우 */
	for(int i=0; argv[i]; i++){

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

		// 문장 내부에 ' '의 쌍이 없는 경우
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
	}
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
         return 0;
      }
      mpow /= 10;
      savedFirstNonnum++;
   }

   
   return num;
}

void findRecentHistory(char* tmp, char *cmdline){
      
      char recentHistory[MAXLINE] = {'\0'};

      // history가 없을 때 !! 입력시 무시
      if(getSize() == 0){
         Sio_puts("bash: !!: event not found\n");
         return;
      }

      strcpy(recentHistory, getRecentHistory());
      if(!strcmp(recentHistory, "!")){
         printf("!\n");
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

