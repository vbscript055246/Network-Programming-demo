#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

using namespace std;

#define BUFFER_SIZE_S 1024
#define BUFFER_SIZE_L 4096
#define NAME_SIZE 20
#define MAX_USERS 30
#define MAX_MSG_NUM 10
#define MAX_MSG_SIZE 1024
#define FIFO_NAME_SIZE 64
#define SHMKEY ((key_t) 9487)
#define PERM 0666

typedef struct FIFO_info{
    int fd;
    char filename[FIFO_NAME_SIZE + 1];
}FIFO;

typedef struct client{
    int id;
    int pid;
    char nickname[NAME_SIZE + 1];
    char ip[16];
    int port;
    char msg[MAX_USERS][MAX_MSG_NUM][MAX_MSG_SIZE +1];
    FIFO fifo[MAX_USERS];
}client;

const char *PWD = "/net/gcs/110/310555007";

char* filename[BUFFER_SIZE_L];
static char *operand[BUFFER_SIZE_L];
char *argv_list[BUFFER_SIZE_L];
int input_num[BUFFER_SIZE_S]={0};
int input_count[BUFFER_SIZE_S];
int output_num[BUFFER_SIZE_S]={0};
int pos = 0;
char *input_backup;

static client *clients = NULL;

static int shmid = 0, client_id = 0;

int handle_new_connection();
int create_socket(int port, int qlen);
void init_shm();
void init_client ();
int add_client(sockaddr_in fsin);

int split_pipe_error(char *str, int n);
int split_pipe(char *str);
int split_space(char *str);
int split_env(char *str);

void shm_handler(int sig);
void sig_handler (int sig);

void execute_who();
void execute_name();
void execute_yell(int num);
void execute_tell(int num);

int do_exec(char *cmd);

void sig_handler(int sig);
void broadcast(char * msg);
void rm_user();

int *parser(int input, int first, int last, int wait, int enter, int n_pipe_err, int output, int get_input, char *cmd){
    int pid, pipe_fd[2];
    char pwd_[20];
    int op, argc, output_fd;
    char *old_cmd = strdup(cmd);
    op = split_space(cmd);
    argc = split_env(getenv("PATH"));

    while (pipe(pipe_fd)<0) ;
    do pid = fork();
    while(pid<0);

    if(pid == 0){
        char buff[100];
        if(first == 1 && last == 0 && input == 0){
            dup2(pipe_fd[1], STDOUT_FILENO);
        }
        else if(first == 1 && last == 0 && input != 0){
            dup2(input, STDIN_FILENO);
            dup2(pipe_fd[1], STDOUT_FILENO);
        }
        else if(first == 0 && last == 0 && input != 0){
            dup2(input, STDIN_FILENO);
            dup2(pipe_fd[1], STDOUT_FILENO);
        }
        else if(first == 1 && last == 1  && get_input == 4 && input == 0){
            dup2(pipe_fd[1], STDOUT_FILENO);
        }
        else if(first == 1 && last == 1  && get_input == 4 && input != 0){
            dup2(pipe_fd[1], STDOUT_FILENO);
            dup2(input, STDIN_FILENO);
        }
        else dup2(input, STDIN_FILENO);

        if(n_pipe_err && get_input!=3) dup2(pipe_fd[1], STDERR_FILENO);
        else if(n_pipe_err && get_input==3)dup2(output, STDERR_FILENO);
        if(output != 0) {
            dup2(output, 1);
            dup2(output, STDERR_FILENO);
        }
        if(enter != 0){
            dup2(input, STDIN_FILENO);
        }
        if(last == 1 && get_input ==4){
            dup2(pipe_fd[1], STDOUT_FILENO);
        }
        else if(last == 1 && get_input ==3){
            dup2(output, STDOUT_FILENO);
        }

        char *__str, *__str2;
        int order = 0;
        if(((__str = strchr(old_cmd, '>')) && (__str[1] != ' ')) && (__str2 = strchr(old_cmd, '<'))){
            if(strlen(__str) > strlen(__str2)) order = 0;
            else order = 1;
        }
        if((__str = strchr(old_cmd, '<')) && order == 0){
            for(int i = 0; i <= op ; i++ ){
                if(strchr(operand[i], '<')) {
                    operand[i] = NULL;
                    break;
                }
            }
        }
        if((__str = strchr(old_cmd, '>'))){
            if(__str[1] == ' '){
                output_fd = creat(operand[op - 1], 0644);
                dup2 ( output_fd, 1);
                close (output_fd);
                for(int i = 0; i <= op ; i++ ){
                    if(strchr(operand[i], '>')) {
                        operand[i] = NULL;
                        break;
                    }
                }
            }
            else{
                char *pipe_id = strdup(old_cmd);
                char *tmp = strtok(pipe_id, ">");
                pipe_id = strtok(NULL, ">");
                pipe_id = strtok(pipe_id, " ");
                for(int i = 0; i <= op ; i++ ){
                    if(strchr(operand[i], '>')){
                        operand[i] = NULL;
                        break;
                    }
                }
            }
        }
        if((__str = strchr(old_cmd, '<')) && order == 1){
            char *pipe_id = strdup(old_cmd);
            char *tmp = strtok(pipe_id, "<");
            pipe_id = strtok(NULL, "<");
            pipe_id = strtok(pipe_id, " ");
            for(int i = 0; i <= op ; i++ )
                if(strchr(operand[i], '<')) {
                    operand[i] = NULL;
                    break;
                }
        }
        free(old_cmd);
        for (int i = 0; i < argc; i++){
            strcpy(pwd_, argv_list[i]);
            strcat(pwd_, "/");
            strcat(pwd_, operand[0]);
            if (execvp(pwd_, operand) != -1) break;
            if(i == argc - 1)
                fprintf(stderr, "Unknown command: [%__str].\n", operand[0]);
        }
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        exit(0);

    }
    else {
        if(wait==1 && last==1) waitpid(pid,0,0);
    }
    if (input != 0) close(input);
    int *pipedata = (int *)malloc(sizeof(pipe_fd));
    memcpy(pipedata, pipe_fd, 2);
    return pipedata;
}

int main(int argc, char *argv[]){

    if (argc != 2){
        puts("Usage: ./np_server [port]");
        exit(1);
    }
    int port = atoi(argv[1]);

    sockaddr_in fsin;
    int msock, ssock;
    socklen_t client_len;
    int pid;
    msock = create_socket(port, 30);

    signal(SIGCHLD, shm_handler);
    signal(SIGINT, shm_handler);
    signal(SIGQUIT, shm_handler);
    signal(SIGTERM, shm_handler);

    init_shm();

    while(1){
        client_len = sizeof(fsin);
        ssock = accept(msock, (struct sockaddr *) &fsin, &client_len);
        if(ssock<0) printf("acceptfailed:\n");
        else printf("server accept the client...\n");
        do pid = fork();
        while(pid<0);
        if(pid == 0){
            close(msock);
            dup2(ssock, STDIN_FILENO);
            dup2(ssock, STDOUT_FILENO);
            dup2(ssock, STDERR_FILENO);

            init_client();
            if(add_client(fsin) < 0){
                shmdt(clients);
                return -1;
            }
            exit(handle_new_connection());
        }
        else close(ssock);
    }
}

int create_socket(int port, int qlen){
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

void init_shm(){
    int shmid = 0;
    client *clients = NULL;
    if((shmid = shmget(SHMKEY, MAX_USERS * sizeof(client) , PERM | IPC_CREAT)) < 0 ){
        fputs("server error: shmget failed\n", stderr);
        exit(1);
    }
    if((clients = (client *) shmat(shmid, NULL, 0)) == (client *)-1){
        fputs("server error:shmat failed\n", stderr);
        exit(1);
    }
    memset(clients, 0, MAX_USERS * sizeof(client));
    shmdt(clients);
}

void init_client (){
    if ((shmid = shmget (SHMKEY, MAX_USERS * sizeof (client), PERM)) < 0) {
        fputs ("server error: shmget failed\n", stderr);
        exit (1);
    }
    if ((clients = (client *) shmat (shmid, NULL, 0)) == (client *) -1) {
        fputs ("server error: shmat failed\n", stderr);
        exit (1);
    }
    clearenv();
    setenv ("PATH", "bin:.", 1);

    signal(SIGUSR1, sig_handler);
    signal(SIGUSR2, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);
}

int add_client(sockaddr_in fsin){
    char ip[20];
    inet_ntop(AF_INET, &fsin.sin_addr, ip, sizeof(sockaddr));
    cout << "****************************************\n";
    cout << "** Welcome to the information server. **\n";
    cout << "****************************************\n";

    char msg[MAX_MSG_SIZE +1];
    for(client_id = 0; client_id < MAX_USERS; ++client_id){
        if(clients[client_id].id == 0){
            clients[client_id].id = client_id + 1;
            clients[client_id].pid = getpid();
            strncpy(clients[client_id].nickname, "(no name)", NAME_SIZE);
            strncpy(clients[client_id].ip, inet_ntoa(fsin.sin_addr), 16);
            clients[client_id].port = ntohs(fsin.sin_port);
            snprintf(msg, MAX_MSG_SIZE +1, "*** client '%s' entered from %s:%d. ***\n", clients[client_id].nickname, clients[client_id].ip, clients[client_id].port);
            broadcast(msg);
            write(1, "% ", strlen("% "));
            return client_id;
        }
    }
    fputs("The server is full.\nPlease try again later...\n", stderr);
    return -1;
}

int handle_new_connection(){
    char buffer[20000];
    string str;
    clearenv();
    setenv("PATH", "bin:.", 1);
    while(1){
        getline(cin, str);
        memset(buffer, 0, sizeof(buffer));
        buffer[strlen(buffer)-1] = '\0';
        input_backup = strdup(buffer);
        do_exec(buffer);
        write(STDOUT_FILENO, "% ", strlen("% "));
    }
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

int split_space(char *str){
    int m = 1;
    operand[0] = strtok(str, " ");
    while ((operand[m] = strtok(NULL, " ")) != NULL) m++;
    operand[m] = NULL;
    return m;
}

int split_env(char *str){
    int m = 1;
    char *cmd_colon = strdup(str);
    argv_list[0] = strtok(cmd_colon, ":");
    while ((argv_list[m] = strtok(NULL, ":")) != NULL) m++;
    argv_list[m] = NULL;
    return m;
}

void execute_who(){
    cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for(int i = 0;i<MAX_USERS;++i){
        if(clients[i].id > 0){
            printf("%d\t%s\t%s:%d%s\n", clients[i].id, clients[i].nickname, clients[i].ip, clients[i].port, (i == client_id)?"\t<-me":"");
        }
    }
}

void execute_name(){
    char msg[MAX_MSG_SIZE +1];
    for(int i = 0;i<MAX_USERS;++i){
        if(clients[i].id > 0){
            if(!strcmp(clients[i].nickname, operand[1])){
                printf("*** client '%s' already exists. ***\n", operand[1]);
                return;
            }
        }
    }
    strcpy(clients[client_id].nickname, operand[1]);
    snprintf (msg, MAX_MSG_SIZE + 1, "*** client from %s:%d is named '%s'. ***\n", clients[client_id].ip, clients[client_id].port, clients[client_id].nickname);
    broadcast(msg);
}

void execute_yell(int num){
    char msg[MAX_MSG_SIZE +1];
    string str = string(operand[1]);
    for(int i = 2; i < num; ++i){
        str += " ";
        str += operand[i];
    }
    sprintf(msg, "*** %s yelled ***: %s\n", clients[client_id].nickname, str.c_str());
    broadcast(msg);
}

void execute_tell(int num){
    char msg[MAX_MSG_SIZE +1];
    string str=string(operand[2]);
    int recvid = atoi(operand[1]);
    for(int i = 3; i < num; ++i){
        str += " ";
        str += operand[i];
    }
    if(recvid > 0 && clients[recvid - 1].id){
        sprintf(msg, "*** %s told you ***: %s\n", clients[client_id].nickname, str.c_str());
        for(int i = 0;i<MAX_MSG_NUM;++i){
            if(clients[recvid - 1].msg[client_id][i][0] == 0){
                strncpy(clients[recvid - 1].msg[client_id][i], msg, MAX_MSG_SIZE + 1);
                kill(clients[recvid - 1].pid, SIGUSR1);
                break;
            }
        }
    }
    else printf("*** Error: client #%d does not exist yet. ***\n", recvid);
}

int do_exec(char *cmd){
    int select = 0;
    int user_pipe_out = 0, user_pipe_in = 0;
    int input = 0, output = 0, first = 1, n, next_num = 0, last = 0, wait = 1;
    int pipe_err = 0, get_input = 0, output_fd = 0;
    int sendid = 0, recvid = 0;
    if(cmd[0] == '\0') return 0;
    n = split_pipe(cmd);
    char *tmp = strchr(filename[n - 1], '!');
    if(filename[0] == NULL || filename[0][0] == ' ') return 0;
    if(n!=0  && tmp && (strlen(tmp)>=2 && tmp[1] != ' ')) next_num = split_pipe_error(filename[n - 1], n);
    if(next_num - 1 == n){ pipe_err = 1; n=next_num;}

    int pipe_number;
    if(n!=1 && filename[n - 1][0] != ' '){
        wait = 0;
        pipe_number = strtol(filename[n - 1], (char **)NULL, 10);
        for(int i=0;i<1000;i++){
            if(input_count[i] == pipe_number + 1){
                output_fd = output_num[i];
                get_input = 1;
                break;
            }
        }
        if(get_input==0) { input_count[pos] = pipe_number + 1; get_input = 2;}
        n = n-1;
    }
    if(signal(SIGCHLD,SIG_IGN) == SIG_ERR){
        perror("signal error");
        exit(1);
    }

    char *old_cmd = strdup(input_backup);
    char *str;
    if((str = strchr(old_cmd, '<'))){
        char *pipe_id = strdup(old_cmd);
        char *tmp = strtok(pipe_id, "<");
        pipe_id = strtok(NULL, "<");
        pipe_id = strtok(pipe_id, " ");
        sendid = atoi(pipe_id);
        char msg[MAX_MSG_SIZE +1];
        if(sendid < 0 || sendid > MAX_USERS || clients[sendid - 1].id == 0){
            printf("*** Error: client #%d does not exist yet. ***\n", sendid);
            return 0;
        }
        else if(clients[client_id].fifo[sendid - 1].fd == 0){
            printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n", sendid, clients[client_id].id);
            return 0;
        }
        else{
            sprintf(msg, "*** %str (#%d) just received from %str (#%d) by '%str' ***\n", clients[client_id].nickname, clients[client_id].id, clients[sendid - 1].nickname, clients[sendid - 1].id, input_backup);
            broadcast(msg);
            user_pipe_in = clients[client_id].fifo[sendid - 1].fd;
            input = clients[client_id].fifo[sendid - 1].fd;
        }
    }
    if((str = strchr(old_cmd, '>'))){
        if(str[1] != ' '){
            char *pipe_id = strdup(old_cmd);
            char *tmp = strtok(pipe_id, ">");
            pipe_id = strtok(NULL, ">");
            pipe_id = strtok(pipe_id, " ");
            recvid = atoi(pipe_id);
            char msg[MAX_MSG_SIZE +1], fifo[FIFO_NAME_SIZE + 1];
            if(recvid <= 0 || recvid > MAX_USERS || clients[recvid - 1].id == 0){
                printf("*** Error: client #%d does not exist yet. ***\n", recvid);
                return 0;
            }
            else if(clients[recvid - 1].fifo[client_id].fd != 0){
                printf("*** Error: the pipe #%d->#%d already exists. ***\n", clients[client_id].id, recvid);
                return 0;
            }
            strncpy(msg, PWD, FIFO_NAME_SIZE + 1);
            strcat(msg, "/user_fifo/%d->%d");
            snprintf(fifo, FIFO_NAME_SIZE + 1, msg, clients[client_id].id, recvid);
            if(mkfifo(fifo, 0600) < 0){
                snprintf(msg, MAX_MSG_SIZE + 1, "error: failed to create FIFO\n");
                write(STDERR_FILENO, msg, strlen(msg));
                return 0;
            }
            else{
                strncpy(clients[recvid - 1].fifo[client_id].filename, fifo, FIFO_NAME_SIZE + 1);
                kill(clients[recvid - 1].pid, SIGUSR2);
                user_pipe_out = open(fifo, O_WRONLY);
            }
            snprintf(msg, MAX_MSG_SIZE+1, "*** %str (#%d) just piped '%str' to %str (#%d) ***\n", clients[client_id].nickname, clients[client_id].id, input_backup, clients[recvid - 1].nickname, recvid);
            broadcast(msg);
        }
    }
    for(int i=0; i<n; i++){
        if(strstr(filename[i], "printenv") != NULL && n == 1){
            split_space(filename[i]);
            printf(getenv(operand[1]));
            if(getenv(operand[1])) printf("\n");
            break;
        }
        else if(strstr(filename[i], "setenv") != NULL && n == 1){
            split_space(filename[i]);
            string str = string(operand[1]);
            str += "=";
            str += operand[2];
            putenv(strdup(str.c_str()));
            break;
        }
        else if(strstr(filename[i], "exit") != NULL && n == 1){
            if(clients[client_id].pid == getpid())  rm_user();
            shmdt(clients);
            exit(0);
        }
        else if(strstr(filename[i], "who") != NULL && n == 1){
            execute_who();
            break;
        }
        else if(strstr(filename[i], "nickname") != NULL && n == 1){
            execute_name();
            break;
        }
        else if(strstr(filename[i], "yell") != NULL && n == 1){
            int account = split_space(filename[i]);
            execute_yell(account);
            break;
        }
        else if(strstr(filename[i], "tell") != NULL && n == 1){
            int account = split_space(filename[i]);
            execute_tell(account);
            break;
        }

        if(i == n-1){
            last = 1;
            if(get_input==1){output = output_fd; get_input = 3; }
            else if(get_input==2) get_input = 4;
            if(user_pipe_out != 0) { output_fd = user_pipe_out; }
        }

        select = 0;
        if(first == 1){
            for(int j=0; j<1000; j++){
                if(input_count[j] == 1){
                    input = input_num[j];
                    output = output_num[j];
                    select = 1;
                }
                input_count[j]--;
            }
        }
        if(select == 1) close(output);
        int *get_data = parser(input, first, last, wait, select, pipe_err, output_fd, get_input, filename[i]);
        if(user_pipe_in && i == 0) close(user_pipe_in);
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
    if(user_pipe_out) close(user_pipe_out);
    if(user_pipe_in && clients[client_id].fifo[sendid - 1].fd){
        unlink(clients[client_id].fifo[sendid - 1].filename);
        memset(&clients[client_id].fifo[sendid - 1], 0, sizeof(FIFO));
    }
}

void broadcast(char * msg){
    for(int i = 0;i<MAX_USERS;++i){
        if(clients[i].id > 0){
            for(int j = 0;j< MAX_MSG_NUM; ++j){
                if(clients[i].msg[client_id][j][0] == 0){
                    strncpy(clients[i].msg[client_id][j], msg, MAX_MSG_SIZE + 1);
                    kill(clients[i].pid, SIGUSR1);
                    break;
                }
            }
        }
    }
}

void rm_user(){
    char msg[MAX_MSG_SIZE +1];
    snprintf (msg, MAX_MSG_SIZE + 1, "*** client '%s' left. ***\n", clients[client_id].nickname);
    broadcast(msg);
    close (STDIN_FILENO);
    close (STDOUT_FILENO);
    close (STDERR_FILENO);

    for(int i =0;i<MAX_USERS;++i){
        if(clients[client_id].fifo[i].fd){
            close(clients[client_id].fifo[i].fd);
            unlink(clients[client_id].fifo[i].filename);
        }
    }
    memset(&clients[client_id], 0, sizeof(client));
}

void shm_handler(int sig){
    if(sig == SIGCHLD){
        while(waitpid(-1, NULL, WNOHANG) >0);
    }
    else if(sig == SIGINT || sig == SIGQUIT || sig == SIGTERM){
        int shmid;
        if((shmid = shmget(SHMKEY, MAX_USERS * sizeof(client), PERM)) < 0){
            fputs ("server error: shmget failed\n", stderr);
            exit(1);
        }
        if(shmctl(shmid, IPC_RMID, NULL) <0){
            fputs ("server error: shmctl IPC_RMID failed\n", stderr);
            exit(1);
        }
        exit(0);
    }
    signal(sig, shm_handler);
}

void sig_handler (int sig){
    if (sig == SIGUSR1) {
        int i, j;
        for (i = 0; i < MAX_USERS; ++i) {
            for (j = 0; j < MAX_MSG_NUM; ++j) {
                if (clients[client_id].msg[i][j][0] != 0) {
                    write (STDOUT_FILENO, clients[client_id].msg[i][j], strlen(clients[client_id].msg[i][j]));
                    memset (clients[client_id].msg[i][j], 0, MAX_MSG_SIZE);
                }
            }
        }
    }
    else if (sig == SIGUSR2) {
        for (int i = 0; i < MAX_USERS; ++i) {
            if (clients[client_id].fifo[i].fd == 0 && clients[client_id].fifo[i].filename[0] != 0)
                clients[client_id].fifo[i].fd = open (clients[client_id].fifo[i].filename, O_RDONLY | O_NONBLOCK);
        }
    }
    else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
        rm_user();
        shmdt(clients);
    }
    signal (sig, sig_handler);
}