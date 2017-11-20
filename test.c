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

struct COMMAND{ // Ŀ�ǵ� ����ü
        char* name;
        char* desc;
        bool ( *func )( int argc, char* argv[] ); // �Լ�������. ����� �Լ����� �Ű������� ������
};
pid_t child=-1;                                            // �ڽ����μ��� pid ���� ��������
int status;                                                  // ���μ��� status
int list_CHLD[100];                                     // SIGTSTP�� ������ ���� �ڽ����μ��� ���
int count=0;                                                // waitpid �Լ��� ���� �Ķ����
bool cmd_cd( int argc, char* argv[] );          //cd ��ɾ�
bool cmd_exit( int argc, char* argv[] );        //exit, quit ��ɾ�
bool cmd_help( int argc, char* argv[] );       //help ��ɾ�
bool cmd_ps(int argc, char* argv[]);            //lsb ��ɾ�
bool cmd_rs(int argc, char* argv[]);             //rs ��ɾ�
bool cmd_bg(int argc, char* argv[]);            //lss ��ɾ�
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
bool cmd_help( int argc, char* argv[] ){ // ��ɾ� ���
        int i;
        for( i = 0; i < sizeof( builtin_cmds ) / sizeof( struct COMMAND ); i++ )
        {
                if( argc == 1 || strcmp( builtin_cmds[i].name, argv[1] ) == 0 )
                        printf( "%-10s: %s\n", builtin_cmds[i].name, builtin_cmds[i].desc );
        }
}

bool cmd_ps(int argc, char* argv[]){ //���μ��� ��� ���
                                                     //�ڼ��ѳ����� http://sosal.tistory.com/115
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
        while ((entry = readdir(dir)) != NULL) { //proc directory �����˻�
                lstat(entry->d_name, &fileStat);
                if (!S_ISDIR(fileStat.st_mode))
                        continue;
                pid = atoi(entry->d_name);
                if (pid <= 0)
                        continue;
                sprintf(tempPath, "/proc/%d/cmdline", pid); //cmdline : ���μ����� �̸�
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
bool cmd_bg(int argc, char*argv[]){  //stop process ��� ���
                                                     //�ڼ��ѳ����� http://sosal.tistory.com/115
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
                if(status[7] == 'T'){  //process �� ���°� ��T�� (stop) �̶�� ���
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
bool cmd_rs(int argc, char* argv[]){            // restart ��ɾ�
        child = atoi(argv[1]);
        if(argc!=2){
                printf("usage : rs [pid]\n");
                return true;
        }
        kill(child, SIGCONT);                          //stop process ���¸� run����
        waitpid(child, &status, WUNTRACED ); //�θ����μ����� �ٽ� wait
        return true;
}
 
void handler(int sig){             //signal �ڵ鷯
        if(child != 0){                //�ڽİ� �θ��� ����
                switch(sig){
                    case SIGINT:
                            printf("Ctrl + c SIGINT\n");
                            break;
                    case SIGTSTP:
                            printf("Ctrl + z SIGTSTP\n");
                            kill(0,SIGCHLD); // stpt �޾��� ��, �ڽ��� �ٽ� run
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
        for(i=0;i<strlen(line);i++){ //background ������ wait���� �ʴ´�.
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
