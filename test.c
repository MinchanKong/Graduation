#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <siginfo.h>

struct COMMAND{ // 커맨드 구조체
        char* name;
        char* desc;
        bool ( *func )( int argc, char* argv[] ); // 함수포인터. 사용할 함수들의 매개변수를 맞춰줌
};
pid_t child=-1;                                            // 자식프로세스 pid 저장 전역변수
int status;                                                  // 프로세스 status
int list_CHLD[100];                                     // SIGTSTP로 실행을 멈춘 자식프로세스 목록
int count=0;                                                // waitpid 함수에 사용될 파라미터
bool cmd_cd( int argc, char* argv[] );          //cd 명령어
bool cmd_exit( int argc, char* argv[] );        //exit, quit 명령어
bool cmd_help( int argc, char* argv[] );       //help 명령어
bool cmd_ps(int argc, char* argv[]);            //lsb 명령어
bool cmd_rs(int argc, char* argv[]);             //rs 명령어
bool cmd_bg(int argc, char* argv[]);            //lss 명령어
void handler(int sig);                                   //signal handler
struct COMMAND  builtin_cmds[] =
{
    { "cd",    "change directory",                    cmd_cd   },
    { "exit",   "exit this shell",                        cmd_exit  },
    { "quit",   "quit this shell",                        cmd_exit  },
    { "help",  "show this help",                      cmd_help },
    { "?",      "show this help",                      cmd_help },
    { "lsb",    "show process list" ,                 cmd_ps   },
    { "lss",    "show stop-status process list", cmd_bg   },
    { "rs",      "Restart the stop process" ,       cmd_rs    }
};
bool cmd_cd( int argc, char* argv[] ){ //cd : change directory
        if( argc == 1 )
                chdir( getenv( "HOME" ) );
        else if( argc == 2 ){
            if( chdir( argv[1] ) )
                printf( "No directory\n" );
        }
        else
            printf( "USAGE: cd [dir]\n" );
}
bool cmd_exit( int argc, char* argv[] ){
        return false;
}
bool cmd_help( int argc, char* argv[] ){ // 명령어 출력
        int i;
        for( i = 0; i < sizeof( builtin_cmds ) / sizeof( struct COMMAND ); i++ )
        {
                if( argc == 1 || strcmp( builtin_cmds[i].name, argv[1] ) == 0 )
                        printf( "%-10s: %s\n", builtin_cmds[i].name, builtin_cmds[i].desc );
        }
}

bool cmd_ps(int argc, char* argv[]){ //프로세스 목록 출력
                                                     //자세한내용은 http://sosal.tistory.com/115
        DIR *dir;
        struct dirent *entry;
        struct stat fileStat;
        int pid;
        char cmdLine[256];
        char tempPath[256];
        FILE *srcFp;
        printf("[pid]   directory\n");
        dir = opendir("/proc");
        if(dir == NULL)
                return false;
        while ((entry = readdir(dir)) != NULL) { //proc directory 순차검색
                lstat(entry->d_name, &fileStat);
                if (!S_ISDIR(fileStat.st_mode))
                        continue;
                pid = atoi(entry->d_name);
                if (pid <= 0)
                        continue;
                sprintf(tempPath, "/proc/%d/cmdline", pid); //cmdline : 프로세스의 이름
                srcFp = fopen(tempPath, "r");
                memset(cmdLine,'\0',sizeof(cmdLine));
                fgets(cmdLine,256,srcFp);
                fclose(srcFp);
                if(cmdLine[0] != '\0')
                        printf("[%d]\t%s\n", pid, cmdLine);
        }
        closedir(dir);
        return true;
}
bool cmd_bg(int argc, char*argv[]){  //stop process 목록 출력
                                                     //자세한내용은 http://sosal.tistory.com/115
        DIR *dir;
        struct dirent *entry;
        struct stat fileStat;
        int pid;
        char cmdLine[256];
        char tempPath[256];
        FILE *srcFp;
        char status[100];
        printf("[pid]   directory\n");
        dir = opendir("/proc");
        if(dir == NULL)
                return false;
        while ((entry = readdir(dir)) != NULL) {
                lstat(entry->d_name, &fileStat);
                if (!S_ISDIR(fileStat.st_mode))
                        continue;
                pid = atoi(entry->d_name);
                if (pid <= 0)
                        continue;
                sprintf(tempPath, "/proc/%d/status",pid);
                srcFp = fopen(tempPath,"r");
                fgets(status,100,srcFp);
                fgets(status,10,srcFp);
                if(status[7] == 'T'){  //process 의 상태가 ‘T’ (stop) 이라면 출력
                        fclose(srcFp);
                        sprintf(tempPath, "/proc/%d/cmdline", pid);
                        srcFp = fopen(tempPath, "r");
                        memset(cmdLine,'\0',sizeof(cmdLine));
                        fgets(cmdLine,256,srcFp);
                        printf("[%d]\t%s &\n",pid,cmdLine);
                }
                fclose(srcFp);
        }
        closedir(dir);
        return true;
}
bool cmd_rs(int argc, char* argv[]){            // restart 명령어
        child = atoi(argv[1]);
        if(argc!=2){
                printf("usage : rs [pid]\n");
                return true;
        }
        kill(child, SIGCONT);                          //stop process 상태를 run으로
        waitpid(child, &status, WUNTRACED ); //부모프로세스는 다시 wait
        return true;
}
 
void handler(int sig){             //signal 핸들러
        if(child != 0){                //자식과 부모의 구분
                switch(sig){
                    case SIGINT:
                            printf("Ctrl + c SIGINT\n");
                            break;
                    case SIGTSTP:
                            printf("Ctrl + z SIGTSTP\n");
                            kill(0,SIGCHLD); // stpt 받았을 때, 자신은 다시 run
                            break;
                    case SIGCONT:
                            printf("Restart rs SIGCONT\n");
                            break;
                }
        }
}
int tokenize( char* buf, char* delims, char* tokens[], int maxTokens ){
        int token_count = 0;
        char* token;
        token = strtok( buf, delims );
        while( token != NULL && token_count < maxTokens ){
                tokens[token_count] = token;
                token_count++;
                token = strtok( NULL, delims );
        }
        tokens[token_count] = NULL;
        return token_count;
}
bool run( char* line ){
        char delims[] = " \r\n\t";
        char* tokens[128];
        int token_count;
        int i;
        bool back;
        for(i=0;i<strlen(line);i++){ //background 실행은 wait하지 않는다.
                if(line[i] == '&'){
                        back=true;
                        line[i]='\0';
                        break;
                }
        }
        token_count = tokenize( line, delims, tokens, sizeof( tokens ) / sizeof( char* ) );
        if( token_count == 0 )
                return true;
        for( i = 0; i < sizeof( builtin_cmds ) / sizeof( struct COMMAND ); i++ ){
                if( strcmp( builtin_cmds[i].name, tokens[0] ) == 0 )
                        return builtin_cmds[i].func( token_count, tokens );
        }
        child = fork();
        if( child == 0 ){
                if( signal(SIGINT,handler) == SIG_ERR){
                        printf("SIGINT Error\n");
                        _exit(1);
                }
                execvp( tokens[0], tokens );
                printf( "No such file\n" );
                _exit( 0 );
        }
        else if( child < 0 )
        {
            printf( "Failed to fork()!" );
            _exit( 0 );
        }
        else if(back == false){
            waitpid(child, &status, WUNTRACED );
        }
        return true;
}
int main(){
        char line[1024];
		char wd[BUFSIZ];
        signal(SIGINT,handler);
        signal(SIGTSTP,handler);

		getcwd(wd,BUFSIZ);
        while( 1 )
        {
            printf( "[%s] $ ", wd);
            fgets( line, sizeof( line ) - 1, stdin );
            if( run( line ) == false )
                break;
        }
        return 0;
}
