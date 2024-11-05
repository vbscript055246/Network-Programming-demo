#include <iostream>
#include <string>
#include <cstdio>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

using namespace std;

#define BUFFER_SIZE_S 1024
#define BUFFER_SIZE_L 4096

char prompt[512];
char pwd[BUFFER_SIZE_S];
char* filename[BUFFER_SIZE_L];
static char *operand[BUFFER_SIZE_L];
char *argv_list[BUFFER_SIZE_L];
int input_num[BUFFER_SIZE_S]={0};
int input_count[BUFFER_SIZE_S];
int output_num[BUFFER_SIZE_S]={0};
int pos = 0;

void get_pwd();
void handle_new_connection(int sockfd);
int do_exec(char *cmd, int sockfd);
int create_socket(int port, int qlen);
int *parser(int input, int first, int last, int wait, int enter, int n_pipe_err, int output, int get_input, char *cmd);
int split_env(char *str);
int split_space(char *str);
int split_pipe_error(char *str, int n);
int split_pipe(char *str);

int main(int argc, char *argv[]){
    if (argc != 2){
        puts("Usage: ./np_server [port]");
        exit(1);
    }
    int port = atoi(argv[1]);

    struct sockaddr_in fsin;
    int msock, ssock;
    socklen_t client_len;
    pid_t pid;
    msock = create_socket(port, 30);
    signal(SIGCHLD,SIG_IGN);
    while(1){
        do
            ssock = accept(msock, (struct sockaddr *) &fsin, &client_len);
        while(ssock<0);

        int optval=1;
        if (setsockopt(ssock, SOL_SOCKET , SO_REUSEADDR, &optval, sizeof(optval))== -1 ){
            puts("Error: setsockopt() failed");
            exit(1);
        }
        do
            pid = fork();
        while ( pid < 0 );

        if(pid == 0){
            close(msock);
            handle_new_connection(ssock);
            exit(0);
        }
        else close(ssock);
    }

}

void handle_new_connection(int sockfd){
    char buffer[20000];
    clearenv();
    setenv("PATH", "bin:.", 1);
    while(1){
        get_pwd(); // TODO
        send(sockfd, "% ", strlen("% "), 0);
        memset(buffer, 0, sizeof(buffer));
        read(sockfd, buffer, sizeof(buffer));
        buffer[strlen(buffer)-1] = '\0';
        do_exec(buffer, sockfd);
    }

}

int do_exec(char *cmd, int sockfd){
    int input = 0, output = sockfd, first = 1, n =0, new_n = 0, last = 0, wait = 1,enter = 0;
    int n_pipe_err = 0, get_input = 0, last_output = sockfd;
    n = split_pipe(cmd);
    if(n!=0) new_n = split_pipe_error(filename[n - 1], n);
    if( new_n - 1 == n){
        n_pipe_err = 1;
        n = new_n;
    }
    int pipe_number;
    if(n!=1 && filename[n - 1][0] != ' '){
        wait = 0;
        pipe_number = strtol(filename[n - 1], (char **)NULL, 10);
        for(int i=0;i<1000;i++){
            if(input_count[i] == pipe_number + 1){
                last_output = output_num[i];
                get_input = 1;
                break;
            }
        }
        if(get_input==0) {
            input_count[pos] = pipe_number + 1;
            get_input = 2;
        }
        n = n-1;

    }
    if(signal(SIGCHLD,SIG_IGN) == SIG_ERR){
        perror("signal error");
        exit(1);
    }

    for(int i=0; i<n; i++){
        if(filename[0] == NULL || filename[0][0] == ' ') break;

        if(strstr(filename[i], "printenv") != NULL && n == 1){
            int old_fd = dup(1);
            dup2(sockfd, 1);
            split_space(filename[i]);
            printf(getenv(operand[1]));
            if(getenv(operand[1])) printf("\n");
            dup2(old_fd, 1);
            close(old_fd);
            break;
        }
        else if(strstr(filename[i], "setenv") != NULL && n == 1){
            split_space(filename[i]);
            char env[512];
            strcpy(env, operand[1]);
            strcat(env, "=");
            strcat(env, operand[2]);
            putenv(env);
            break;
        }
        else if(strstr(filename[i], "exit") != NULL && n == 1){
            exit(0);
        }

        if(i == n-1){
            last = 1;
            if(get_input==1){output = last_output; get_input = 3; }
            else if(get_input==2) get_input = 4;
        }

        enter = 0;
        if(first == 1){
            for(int j=0; j<1000; j++){
                if(input_count[j] == 1){
                    input = input_num[j];
                    output = output_num[j];
                    enter = 1;
                }
                input_count[j]--;
            }
        }
        if(enter) close(output);

        int *get_data = parser(input, first, last, wait, enter, n_pipe_err, last_output, get_input, filename[i]);
        input = get_data[0];
        output = get_data[1];

        if(i == n-1){
            if(get_input == 0) close(output);
            if(get_input == 4){
                input_num[pos] = input;
                output_num[pos] = output;
                pos++;
                if(pos > 1000) pos=0;
            }
        }
        else close(output);
        first = 0;
        if(i!=0 && i%200==0) sleep(3);
    }
}

int create_socket(int port, int  qlen){
    sockaddr_in server_addr;
    int server_socket;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = PF_INET;
    server_addr.sin_port = htons( port);
    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if(bind(server_socket, (sockaddr*) &server_addr, sizeof(server_addr)) < 0){
        puts("Error: bind() failed");
        exit(1);
    }
    if(listen(server_socket, qlen) < 0){
        puts("Error: bind() failed");
        exit(1);
    }
    return server_socket;
}

int *parser(int input, int first, int last, int wait, int enter, int n_pipe_err, int output, int get_input, char *cmd){
    int pipefd[2];
    int args_num, cmd_args_num, output_fd;
    char *old_cmd = strdup(cmd);
    args_num = split_space(cmd);
    cmd_args_num = split_env(getenv("PATH"));

    while (pipe(pipefd) < 0);

    int pid = fork();
    if(pid == 0){
        if(first == 1 && last == 0 && input == 0){
            dup2(pipefd[1], STDOUT_FILENO);
        }
        else if(first == 0 && last == 0 && input != 0){
            dup2(input, 0);
            dup2(pipefd[1], STDOUT_FILENO);
        }
        else if(first == 1 && last == 1  && get_input == 4){
            dup2(pipefd[1], STDOUT_FILENO);
        }
        else {
            dup2(input, STDIN_FILENO);
            dup2(output, STDOUT_FILENO);
            dup2(output, STDERR_FILENO);
        }
        if(n_pipe_err && get_input!=3) dup2(pipefd[1], STDERR_FILENO);
        else if(n_pipe_err && get_input==3)dup2(output, STDERR_FILENO);

        if(enter != 0) dup2(input, STDIN_FILENO);

        if(last == 1 && get_input ==4) dup2(pipefd[1], STDOUT_FILENO);
        else if(last == 1 && get_input ==3) dup2(output, STDOUT_FILENO);

        if(strchr(old_cmd, '>')){
            output_fd = creat(operand[args_num - 1], 0644);
            dup2 ( output_fd, STDOUT_FILENO);
            close (output_fd);
            for(int i = 0; i<= args_num ; i++ ){
                if(strchr(operand[i], '>')) {
                    operand[i] = NULL;
                    break;
                }
            }

        }
        free(old_cmd);

        for (int i = 0; i < cmd_args_num; i++){
            string source = string(argv_list[i]) + "/" + operand[0];
            if (execvp(source.c_str(), operand) != -1){
                break;
            }
            if(i==cmd_args_num-1)
                cerr << "Unknown command: [" << operand[0] << "].\n";
        }
        close(pipefd[0]);
        close(pipefd[1]);
        exit(0);

    }
    else {
        if(wait ==1 && last == 1) waitpid(pid,0,0);
    }
    if (input != 0) close(input);

    int *pipedata = (int *)malloc(sizeof(pipefd));
    memcpy(pipedata, pipefd, 2);
    return pipedata;
}

int split_env(char *str){
    int m = 1;
    char *cmd_colon = strdup(str);
    argv_list[0] = strtok(cmd_colon, ":");
    while ((argv_list[m] = strtok(NULL, ":")) != NULL) m++;
    argv_list[m] = NULL;
    return m;
}

int split_space(char *str){
    int m = 1;
    operand[0] = strtok(str, " ");
    while ((operand[m] = strtok(NULL, " ")) != NULL) m++;
    operand[m] = NULL;
    return m;
}

int split_pipe_error(char *str, int n){
    filename[n] = strtok(str, "!");
    while((filename[n] = strtok(NULL, "!")) != NULL) n++;
    filename[n] = NULL;
    return n;
}

int split_pipe(char *str){
    int n=1;
    filename[0] = strtok(str, "|");
    while((filename[n] = strtok(NULL, "|")) != NULL) n++;
    filename[n] = NULL;
    return n;
}

void get_pwd(){
    if(getcwd(pwd, sizeof(pwd)) != NULL) strcpy(prompt, "% ");
}