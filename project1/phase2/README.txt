/* Phase2 Readme */

1. 컴파일 방법

- terminal에서 phase2 디렉토리로 이동 후, 'make' 명령어를 입력 해 컴파일 한다.


2. 실행 방법

- myshell의 실행 파일 이름은 myshell 이다. ./myshell을 입력 해 shell 프로그램을 실행한다.

- shell 프로그램이 실행되면, '> ' 문자와 함께 사용자의 입력을 대기하고 있는 상태가 된다.

- Phase2에선 Phase1에서 실행했던 기본 명령어들과 더불어, pipeline 명령어도 실행할 수 있다.

- 'cat filename | sort', 'cat filename | grep -i "abc" | sort -r' 등의 pipeline 명령어를 입력하면,
    앞의 process 의 output이 뒤의 process의 input으로 들어가 최종 결과가 stdout 으로 출력된다.

- pipeline에서도 phase1 에서 제공했던 '!!', '!#' 명령어를 포함해서 실행할 수 있다.
    예를 들어 '!! | !!' 와 같은 명령어를 실행하면, '!!' 부분이 가장 최근 실행했던 명령어로 대체되어 pipeline 명령어가 수행된다.

- shell 프로그램을 종료시키고 싶으면 'exit' 명령어를 입력하면 된다.

/* phase2 Readme End */