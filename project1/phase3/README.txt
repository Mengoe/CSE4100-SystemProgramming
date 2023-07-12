/* Phase3 Readme */

1. 컴파일 방법

- terminal에서 phase3 디렉토리로 이동 후, 'make' 명령어를 입력 해 컴파일 한다.


2. 실행 방법

- myshell의 실행 파일 이름은 myshell 이다. ./myshell을 입력 해 shell 프로그램을 실행한다.

- shell 프로그램이 실행되면, '> ' 문자와 함께 사용자의 입력을 대기하고 있는 상태가 된다.

- Phase3에선 Phase1, 2에서 실행했던 기본 명령어, pipeline 명령어와 더불어 background process 생성 및 job 관리에 대한 명령어도 수행할 수 있다.

- background process를 생성하고 싶은 경우, 명령어 뒤에 '&' 를 붙이면 된다. '&' 를 붙여서 실행한 명령은 background 에서 돌아가며, 
 foreground shell은 계속해서 입력을 받고 명령어를 실행한다.

- 현재 background process에 있는 process 들의 상태를 확인하고 싶으면 'jobs' 명령어를 입력하면 된다.

- 현재 background process에서 stop 돼있는 job을 running 상태로 바꾸고 싶으면 'bg' 명령어와 함께 job의 번호를 입력하면 된다.

- 현재 background process에 있는 job을 종료시키고 싶으면, 'kill' 명령어와 함께 job의 번호를 입력하면 된다.

- 현재 background process에 있는 job을 foreground process로 만들고 싶으면, 'fg' 명령어와 함께 job의 번호를 입력하면 된다.

- shell 프로그램을 종료시키고 싶으면 'exit' 명령어를 입력하면 된다.

/* Phase3 Readme End */

