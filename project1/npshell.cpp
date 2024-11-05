#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <vector>

#define PIPE_READ   0
#define PIPE_WRITE  1

#define EXEC_ERRNO 9
#define EXEC_ARGNO 8

#define BUFFER_SIZE 10000
#define TEMP_BUFF_SIZE 100

#define debug_flg 0
#define debug_child_flg 0
#define debug_set_env 1

std::vector<int *> pipeStack, recycle;
static uint16_t childCount = 0;
char instruction[BUFFER_SIZE] = {0};
char _buf[BUFFER_SIZE] = {0};

void executeCMD(char *cmdStr);                          // Recursively execute command function
void callExecs(char *OP, char *arg);                    // Call execvp/execlp with argument
int *getPipe();                                         // Get pipe from pipeStack
int *makeNewPipe();                                     // Get new pipe
void SIGCHLD_handler(int sig);                          // Main process handle signal SIGCHLD
void split(char **arr, char *str, const char *del);     // Split char* to char* array
void cleanPipes();

int main() {
    char inputBuffer[BUFFER_SIZE] = {0};
    signal(SIGCHLD, SIGCHLD_handler);
#if debug_set_env
    setenv("PATH", "bin:.", 1);
#endif
    while(true){
        printf("%% ");                            // command line prompt
        std::cin.getline(inputBuffer, BUFFER_SIZE);

        while(strlen(inputBuffer)!=0){
	    
            executeCMD(inputBuffer);
            memset(inputBuffer, 0,strlen(inputBuffer));
            strncpy(inputBuffer, _buf, strlen(_buf));
            memset(_buf, 0,strlen(_buf));
            cleanPipes(); 
	}
        while (waitpid(0, 0, WNOHANG)> 0);
        cleanPipes();
    }
}

void split(char **arr, char *str, const char *del) {
    char *s = strtok(str, del);
    while(s != nullptr) {
        *arr++ = s;
        s = strtok(nullptr, del);
    }
    *arr++ = nullptr;
}

void cleanPipes(){
    while(recycle.size()>10){
        close(recycle[0][PIPE_READ]);
        close(recycle[0][PIPE_WRITE]);
        delete [] recycle[0];
        recycle.erase(recycle.begin());
    }
}

void executeCMD(char *cmdStr){

    char symbolMeth[2] = {0};

    char OP[TEMP_BUFF_SIZE] = {0};
    char arg[TEMP_BUFF_SIZE] = {0};

    int numPipe=0;
    int *prevPipe, *currentPipe;

    // Analysis command
    // Numbered Pipe
    int n = sscanf(cmdStr, "%[a-zA-Z0-9-\"\'.: ]%[|!]%d\n", instruction, symbolMeth, &numPipe);
    // Ordinary Pipe/File Redirection/Execute cmd/setenv/printenv
    if(n != 3) n = sscanf(cmdStr, "%[a-zA-Z0-9-\"\'.: ]%[|!>] %[^\n]", instruction, symbolMeth, _buf);
    // instruction => file + argument
    int ins_cnt = sscanf(instruction, "%s %[a-zA-Z0-9-\"\'.: ]\n", OP, arg);



#if debug_flg
    printf("I: %s, M: %s, B: %s\n", instruction, symbolMeth, _buf);
#endif

    switch(n){
        case -1:    // Press Enter
//            getPipe();
            break;
        case 1:     // Execution of commands/Setenv/Printenv
            if(strcmp(OP, "setenv") == 0){
                char var[TEMP_BUFF_SIZE] = {0}, value[TEMP_BUFF_SIZE] = {0};
                sscanf(arg, "%s %s\n", var, value);
                setenv(var, value, 1);
            }
            else if (strcmp(OP, "printenv") == 0){
                char *value = getenv(arg);
                if (value != nullptr) printf("%s\n", value);
//                else printf("\n");
            }
            else if (strcmp(OP, "exit") == 0){
                exit(0);
            }
            // Execution of commands
            else{
                prevPipe = getPipe();

                int cpid = fork();
                while(cpid<0) {
                    cpid = fork();
                }

                if (cpid != 0) {
                    childCount++;
                    while (wait(0)>0);
		    return ;
                }
                else{
                    if(prevPipe != nullptr) dup2(prevPipe[PIPE_READ], STDIN_FILENO);
                    if(ins_cnt <= 2) callExecs(OP, arg);
                    else exit(EXEC_ARGNO);
                }
            }
            break;
        case 3:     // Ordinary Pipe/Numbered Pipe/File Redirection
            prevPipe = getPipe();
            // Numbered Pipe
            if(numPipe){
                while(numPipe > pipeStack.size()) pipeStack.push_back(nullptr);
                if (pipeStack.empty() || pipeStack[numPipe - 1] == nullptr){
                    currentPipe = makeNewPipe();
                    pipeStack[numPipe - 1] = currentPipe;
                }
                else{
                    currentPipe = pipeStack[numPipe - 1];
                }

                if (fork() != 0) {
                    childCount++;
                    return ;
                }
                else{
                    if(prevPipe != nullptr) dup2(prevPipe[PIPE_READ], STDIN_FILENO);
                    dup2(currentPipe[PIPE_WRITE], STDOUT_FILENO);
                    if(symbolMeth[0] == '!') dup2(currentPipe[PIPE_WRITE], STDERR_FILENO);

                    if(ins_cnt <= 2) callExecs(OP, arg);
                    else exit(EXEC_ARGNO);
                }
            }
            // Ordinary Pipe/File Redirection
            else{
                // Ordinary Pipe
                if(symbolMeth[0] == '|'){
                    if(pipeStack.empty() || pipeStack[0] == nullptr){
                        currentPipe = makeNewPipe();
                        pipeStack.insert(pipeStack.begin(), currentPipe);
                    }
                    else{
                        currentPipe = pipeStack[0];
                    }
                    int cpid = fork();
                    while(cpid<0) {
                        cpid = fork();
//			printf("%d\n", childCount);
                    }

                    if(cpid!=0){
                        childCount++;
//                        executeCMD(_buf);
                        return;
                    }
                    else{
                        if(prevPipe != nullptr) dup2(prevPipe[PIPE_READ], STDIN_FILENO);
                        dup2(currentPipe[PIPE_WRITE], STDOUT_FILENO);

                        if(ins_cnt <= 2) callExecs(OP, arg);
                        else exit(EXEC_ARGNO);
                    }

                }
                // File Redirection
                else if(symbolMeth[0] == '>') {
                    
                    int cpid = fork();
                    while(cpid<0) {
                        cpid = fork();
                    }

                    if(cpid!=0){
                        _buf[0] = '\0';
                        childCount++;
                        while (wait(0)>0);
			return ;
                    }
                    else{
                        // init file
                        int file_fd = open(_buf,O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO );
                        if (file_fd == - 1) {
                            printf("File can't open~");
                            return ;
                        }

                        if (prevPipe != nullptr) dup2(prevPipe[PIPE_READ], STDIN_FILENO);
                        dup2(file_fd, STDOUT_FILENO);

                        if (ins_cnt <= 2) callExecs(OP, arg);
                        else exit(EXEC_ARGNO);
                    }
                }
                else{
                    printf("input Error~\n");
                    return ;
                }
            }
            break;
        default:
            printf("input Error~\n");
            return ;
    }
}

void SIGCHLD_handler(int sig){
    int stat, pid = wait(&stat);
#if debug_child_flg
    printf("child pid :%d done, state %d, signal %d\n", pid, stat, sig);
#endif
    childCount--;
}

void callExecs(char *OP, char *arg){
    if (strlen(arg)) {
        char *argv[TEMP_BUFF_SIZE] = {0};
        argv[0] = OP;
        split(&(argv[1]), arg, " ");
        execvp(OP, argv);
    }
    else {
        execlp(OP, OP, nullptr);
    }
    printf("Unknown command: [%s].\n", OP);
    exit(EXEC_ERRNO);
}

int *makeNewPipe(){
    int *_pipe;
    _pipe = new int[2];
    while(pipe(_pipe)<0){
        cleanPipes();
//        printf("pipe fail\n");
//        exit(0);
    }
    return _pipe;
}

int *getPipe(){
    int * _pipe;
    if(!pipeStack.empty()){
        _pipe = pipeStack[0];
        if(_pipe != nullptr){
            close(_pipe[PIPE_WRITE]);
            recycle.push_back(_pipe);
        }
        pipeStack.erase(pipeStack.begin());
        return _pipe;
    }
    return nullptr;
}
