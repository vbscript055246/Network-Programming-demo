#include <iostream>
#include <string>
#include <cstdio>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
using namespace std;

#define BUFFER_SIZE_L 4096
#define MAX_PIPE_NUM 2048
#define MAX_USER_NUM 60

typedef struct pipe_info{
    int input_num;
    int input_count;
    int output_num;
    int fd;
}pipe_info;

typedef struct user_pipe_info{
    int pipe_fd;
    int flag;
}user_pipe_info;

typedef struct env_map{
    int env_num;
    char key[100][100];
    char value[100][100];
}env_map;

typedef struct client_map{
    int size = 0;
    int clients_fd_set[MAX_USER_NUM];
    char nickname[MAX_USER_NUM][100];
    env_map env[MAX_USER_NUM];
    sockaddr_in clients_addr_in[MAX_USER_NUM];
} client_map;

pipe_info client_pipe[MAX_PIPE_NUM];
client_map clients_map;
user_pipe_info user_pipe[31][31];

char* filename[BUFFER_SIZE_L];
char* input_backup;
static char *operand[BUFFER_SIZE_L];
char *argv_list[BUFFER_SIZE_L];
int pos = 0;

int create_socket(int port, int qlen);
int handle_new_connection(int sockfd);

int split_env(char *str);
int split_space(char *str);
int split_error_pipe(char *str, int n);
int split_pipe(char *str);

void handle_new_connection(int clientfd,sockaddr_in fsin);
void load_env(int sockfd, char *env_str);
char *do_printenv(int sockfd, char* env_str);

void execute_who(int fd);
void execute_tell(int fd, int account);
void execute_yell(int fd, int account);
void execute_name(int fd);

void broadcast_logout(int cid);

int do_exec(char *cmd, int sockfd);

int *parser(int input, int first, int last, int wait, int enter, int n_pipe_err, int output, int get_input, char *cmd, int sockfd);

int main(int argc, char *argv[]){
    sockaddr_in fsin;
    int msock, ssock;
    socklen_t client_len;
    fd_set rset, allset;
    int fd, max_fds;

    if (argc != 2){
        puts("Usage: ./np_server [port]");
        exit(1);
    }
    int port = atoi(argv[1]);
    msock = create_socket(port, 30);
    max_fds = getdtablesize();

    memset(&allset, 0, sizeof(allset));
    FD_SET(msock, &allset);
    while(1){
        memcpy(&rset, &allset, sizeof(rset));

        while (select(max_fds, &rset, 0, 0, 0) < 0);

        if(FD_ISSET(msock, &rset)){
            client_len = sizeof(fsin);
            do
                ssock = accept(msock, (sockaddr *)&fsin, &client_len);
            while (ssock < 0);

            printf("----------------\n");
            printf(" new user login \n");
            printf("----------------\n");
            handle_new_connection(ssock, fsin);

            FD_SET(ssock, &allset);

            for(int i = 0; i <= clients_map.size; ++i){
                if(clients_map.clients_fd_set[i] == 0){
                    clients_map.clients_fd_set[i] = ssock;
                    clients_map.clients_addr_in[i] = fsin;
                    strcpy(clients_map.nickname[i], "(no name)");
                    if(i == clients_map.size) clients_map.size++;
                    break;
                }
            }
            load_env(ssock, "PATH=bin:.");
        }
        for(fd=0; fd < max_fds; ++fd){
            if(fd != msock && FD_ISSET(fd, &rset)){
                if(handle_new_connection(fd) == 1){
                    int i;
                    close(fd);
                    FD_CLR(fd,&allset);
                    for(i=0; i <= clients_map.size; ++i){
                        if(clients_map.clients_fd_set[i] == fd){ clients_map.clients_fd_set[i] = 0;
                            broadcast_logout(i + 1); break;}
                    }
                }
            }
        }

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

int handle_new_connection(int sockfd){
    char buffer[20000];

    memset(buffer, 0, sizeof(buffer));
    read(sockfd, buffer, sizeof(buffer));
    buffer[strlen(buffer)-1] = '\0';
    input_backup = strdup(buffer);

    int end = do_exec(buffer, sockfd);
    if(end == 1) return 1;
    write(sockfd, "% ", 2);
    return 0;
}

int split_pipe(char *str){
    int n=1;
    filename[0] = strtok(str, "|");
    while((filename[n] = strtok(NULL, "|")) != NULL) n++;
    filename[n] = NULL;
    return n;
}

int split_error_pipe(char *str, int n){
    int i;
    filename[n] = strtok(str, "!");
    while((filename[n] = strtok(NULL, "!")) != NULL) n++;
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

void handle_new_connection(int clientfd, sockaddr_in fsin){
    char ipv4[20];
    int fd_old = dup(STDOUT_FILENO);
    inet_ntop(AF_INET, &fsin.sin_addr, ipv4, sizeof(sockaddr));
    dup2(clientfd, STDOUT_FILENO);
    string _str = "****************************************\n";
    _str +="** Welcome to the information server. **\n";
    _str +="****************************************\n";
    cout << _str;
    printf("*** User '(no name)' entered from %s:%d. ***\n", ipv4, ntohs(fsin.sin_port));
    write(clientfd, "% ", strlen("% "));
    for(int i = 0; i < clients_map.size; ++i){
        if(clients_map.clients_fd_set[i] == clientfd || clients_map.clients_fd_set[i] == 0) continue;
        dup2(clients_map.clients_fd_set[i], STDOUT_FILENO);
        printf("*** User '(no name)' entered from %s:%d. ***\n", ipv4, ntohs(fsin.sin_port));
    }
    dup2(fd_old, STDOUT_FILENO);
    close(fd_old);
}

void load_env(int sockfd, char *env_str){
    int clientid=0, label = -1;
    char *key, *val;
    key = strtok(strdup(env_str), "=");
    val = strtok(NULL, "=");

    for(int i = 0; i < clients_map.size; ++i){
        if(clients_map.clients_fd_set[i] == sockfd) {
            clientid = i + 1;
            break;
        }
    }
    if(clients_map.env[clientid - 1].env_num == 0) label = -1;
    else{
        for(int i = 0; i < clients_map.env[clientid - 1].env_num; ++i){
            if(strcmp(clients_map.env[clientid - 1].key[i], key) == 0){
                label = i;
                break;
            }
        }
    }
    if(label == -1){
        strcpy(clients_map.env[clientid - 1].key[clients_map.env[clientid].env_num], key);
        strcpy(clients_map.env[clientid - 1].value[clients_map.env[clientid].env_num], val);
        clients_map.env[clientid - 1].env_num++;
        printf("%d\n", clients_map.env[clientid - 1].env_num);
    }
    else strcpy(clients_map.env[clientid - 1].value[label], val);
}

void execute_who(int fd){
    int old_fd = dup(STDOUT_FILENO);
    char ipv4[20];
    dup2(fd, STDOUT_FILENO);
    cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for(int i = 0; i <= clients_map.size; ++i){
        if(clients_map.clients_fd_set[i] == 0) continue;
        inet_ntop(AF_INET, &clients_map.clients_addr_in[i].sin_addr, ipv4, sizeof(sockaddr));
        if(clients_map.clients_fd_set[i] == fd) printf("%d\t%s\t%s:%d\t<-me\n", i + 1, clients_map.nickname[i], ipv4, ntohs(clients_map.clients_addr_in[i].sin_port));
        else printf("%d\t%s\t%s:%d\n", i+1, clients_map.nickname[i], ipv4, ntohs(clients_map.clients_addr_in[i].sin_port));
    }
    dup2(old_fd, STDOUT_FILENO);
    close(old_fd);
}

void execute_tell(int fd, int account){
    int old_fd = dup(STDOUT_FILENO);
    int write_fd, clientid;
    int receid = atoi(operand[1]) - 1;
    if(clients_map.clients_fd_set[receid] == 0){
        dup2(fd, STDOUT_FILENO);
        printf("*** Error: user #%d does not exist yet. ***\n", receid + 1);
        dup2(old_fd, STDOUT_FILENO);
        close(old_fd);
        return ;
    }
    else write_fd = clients_map.clients_fd_set[receid];
    for(int i = 0; i < clients_map.size; ++i){
        if(clients_map.clients_fd_set[i] == fd) { clientid = i + 1; break;}
    }
    char env[512];
    strcpy(env, operand[2]);
    for(int i = 3;i<account;i++){
        strcat(env, " ");
        strcat(env, operand[i]);
    }
    dup2(write_fd, STDOUT_FILENO);
    printf("*** %s told you ***: %s\n", clients_map.nickname[clientid - 1], env);
    dup2(old_fd, STDOUT_FILENO);
    close(old_fd);
}

void execute_yell(int fd, int account){
    int old_fd = dup(STDOUT_FILENO);
    int clientid;
    char env[1024];
    strcpy(env, operand[1]);
    for(int i = 2;i<account;++i){
        strcat(env, " ");
        strcat(env, operand[i]);
    }
    strcat(env, "\n");
    for(int i = 0; i < clients_map.size; ++i){
        if(clients_map.clients_fd_set[i] == fd){
            clientid = i;
            break;
        }
    }
    for(int i = 0; i < clients_map.size; ++i){
        if(clients_map.clients_fd_set[i] == 0) continue;
        dup2(clients_map.clients_fd_set[i], STDOUT_FILENO);
        printf("*** %s yelled ***: %s", clients_map.nickname[clientid], env);
    }
    dup2(old_fd, STDOUT_FILENO);
    close(old_fd);

}

void broadcast_logout(int cid){
    int fd_old = dup(STDOUT_FILENO);
    for(int i = 1;i<=30;++i){
        user_pipe[cid][i].flag = 0;
        user_pipe[i][cid].flag = 0;
    }
    for(int i = 0; i < clients_map.size; ++i){
        if(i == cid - 1 || clients_map.clients_fd_set[i] == 0) continue;
        dup2(clients_map.clients_fd_set[i], STDOUT_FILENO);
        printf("*** User '%s' left. ***\n", clients_map.nickname[cid - 1]);
    }
    dup2(fd_old, STDOUT_FILENO);
    close(fd_old);
}

void execute_name(int fd){
    int old_fd = dup(1);
    char name[100],ipv4[20];
    int cid;
    strcpy(name, operand[1]);
    for(int i = 0; i < clients_map.size; ++i){
        if(clients_map.clients_fd_set[i] == fd) cid = i;
        if(clients_map.clients_fd_set[i] != fd && clients_map.clients_fd_set[i] != 0){
            if(!strcmp(name, clients_map.nickname[i])){
                dup2(fd, STDOUT_FILENO);
                printf("*** User '%s' already exists. ***\n", name);
                dup2(old_fd, STDOUT_FILENO);
                close(old_fd);
                return ;
            }

        }
    }
    strcpy(clients_map.nickname[cid], name);
    inet_ntop(AF_INET, &clients_map.clients_addr_in[cid].sin_addr, ipv4, sizeof(sockaddr));

    for(int i = 0; i < clients_map.size; ++i){
        if(clients_map.clients_fd_set[i] == 0) continue;
        dup2(clients_map.clients_fd_set[i], STDOUT_FILENO);
        printf("*** User from %s:%d is named '%s'. ***\n", ipv4, ntohs(clients_map.clients_addr_in[cid].sin_port), name);
    }
    dup2(old_fd, STDOUT_FILENO);
    close(old_fd);
}

char *do_printenv(int sockfd, char *env_str){
    int cid;
    char *key, *val;
    key = strdup(env_str);
    for(int i = 0; i < clients_map.size; ++i)
        if(clients_map.clients_fd_set[i] == sockfd) {
            cid = i + 1;
            break;
        }

    for(int i = 0; i < clients_map.env[cid - 1].env_num; ++i){
        if(strcmp(clients_map.env[cid - 1].key[i], key) == 0)
            val = strdup(clients_map.env[cid - 1].value[i]);
        return val;
    }
}

int do_exec(char *cmd, int sockfd){
    int input = 0, output = 0, first = 1, next_n = 0, last = 0, wait = 1, enter, n;
    int pipe_err_n = 0, get_input = 0, output_fd = sockfd;

    printf("sockfd:%d\n", sockfd);
    n = split_pipe(cmd);
    if(filename[0] == NULL || filename[0][0] == ' ') return 0;

    char *_str = strchr(filename[n - 1], '!');
    if(n!=0 && _str && (strlen(_str) >= 2 && _str[1] != ' ')) next_n = split_error_pipe(filename[n - 1], n);
    if(next_n - 1 == n){
        pipe_err_n = 1;
        n=next_n;
    }

    if(n!=1 && filename[n - 1][0] != ' '){
        wait = 0;
        int pipe_number = strtol(filename[n - 1], (char **)NULL, 10);
        for(int i=0; i < MAX_PIPE_NUM - 1; i++){
            if(client_pipe[i].input_count == pipe_number + 1 && client_pipe[i].fd == sockfd){
                output_fd = client_pipe[i].output_num;
                get_input = 1;
                break;
            }
        }
        if(get_input==0){
            client_pipe[pos].input_count = pipe_number + 1;
            client_pipe[pos].fd = sockfd;
            get_input = 2;
        }
        n = n-1;
    }
    if(signal(SIGCHLD,SIG_IGN) == SIG_ERR){
        perror("signal error");
        exit(1);
    }
    char *old_cmd = strdup(input_backup);
    char *s;
    if((s = strchr(old_cmd, '<'))){
        char *pipeid = strdup(old_cmd);
        char *tmp = strtok(pipeid, "<");
        pipeid = strtok(NULL, "<");
        pipeid = strtok(pipeid, " ");
        int sendid = atoi(pipeid);
        int recvid;
        int old_fd = dup(STDOUT_FILENO);
        for(int i = 0; i < clients_map.size; ++i){
            if(clients_map.clients_fd_set[i] == sockfd) { recvid = i + 1; break;}
        }
        if(sendid <= 0 || sendid > 30 || clients_map.clients_fd_set[sendid - 1] == 0){
            dup2(sockfd, STDOUT_FILENO);
            printf("*** Error: user #%d does not exist yet. ***\n", sendid);
            dup2(old_fd, STDOUT_FILENO);
            close(old_fd);
            return 0;
        }
        else if(user_pipe[sendid][recvid].flag == 0){
            dup2(sockfd, STDOUT_FILENO);
            printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n", sendid, recvid);
            dup2(old_fd, STDOUT_FILENO);
            close(old_fd);
            return 0;
        }
        else{
            for(int i = 0; i < clients_map.size; ++i){
                if(clients_map.clients_fd_set[i] == 0){
                    continue;
                }
                dup2(clients_map.clients_fd_set[i], 1);
                printf("*** %s (#%d) just received from %s (#%d) by '%s' ***\n", clients_map.nickname[recvid - 1], recvid, clients_map.nickname[sendid - 1], sendid, input_backup);
            }
            user_pipe[sendid][recvid].flag = 0;
            dup2(old_fd, STDOUT_FILENO);
            close(old_fd);
            input = user_pipe[sendid][recvid].pipe_fd;
        }

    }
    if((s = strchr(old_cmd, '>'))){
        if(s[1] != ' '){
            char *pipeid = strdup(old_cmd);
            char *tmp = strtok(pipeid, ">");
            pipeid = strtok(NULL, ">");
            pipeid = strtok(pipeid, " ");
            int recvid = atoi(pipeid);
            int sendid;
            int old_fd = dup(STDOUT_FILENO);
            for(int i = 0; i < clients_map.size; ++i){
                if(clients_map.clients_fd_set[i] == sockfd) { sendid = i + 1; break;}
            }
            if(recvid <= 0 || recvid > 30 || clients_map.clients_fd_set[recvid - 1] == 0){
                dup2(sockfd, STDOUT_FILENO);
                printf("*** Error: user #%d does not exist yet. ***\n", recvid);
                dup2(old_fd, STDOUT_FILENO);
                close(old_fd);
                return 0;
            }
            if(user_pipe[sendid][recvid].flag == 1){
                dup2(sockfd, STDOUT_FILENO);
                printf("*** Error: the pipe #%d->#%d already exists. ***\n", sendid, recvid);
                dup2(old_fd, STDOUT_FILENO);
                close(old_fd);
                return 0;
            }
            for(int i = 0; i < clients_map.size; ++i){
                if(clients_map.clients_fd_set[i] == 0){
                    continue;
                }
                dup2(clients_map.clients_fd_set[i], STDOUT_FILENO);
                printf("*** %s (#%d) just piped '%s' to %s (#%d) ***\n", clients_map.nickname[sendid - 1], sendid, input_backup, clients_map.nickname[recvid - 1], recvid);
            }
            dup2(old_fd, STDOUT_FILENO);
            close(old_fd);
        }
    }
    for(int i=0; i<n; i++){
        if(strstr(filename[i], "printenv") != NULL && n == 1){
            split_space(filename[i]);
            char *envvalue = do_printenv(sockfd, operand[1]);
            int old_fd = dup(STDOUT_FILENO);
            dup2(sockfd, STDOUT_FILENO);
            printf("%s\n", envvalue);
            dup2(old_fd, STDOUT_FILENO);
            close(old_fd);
            break;
        }
        else if(strstr(filename[i], "setenv") != NULL && n == 1){
            split_space(filename[i]);
            char env[512];
            strcpy(env, operand[1]);
            strcat(env, "=");
            strcat(env, operand[2]);
            load_env(sockfd, env);
            break;
        }
        else if(strstr(filename[i], "exit") != NULL && n == 1) return 1;
        else if(strstr(filename[i], "who") != NULL && n == 1){
            execute_who(sockfd);
            return 0;
        }
        else if(strstr(filename[i], "tell") != NULL && n == 1){
            int account = split_space(filename[i]);
            execute_tell(sockfd, account);
            return 0;
        }
        else if(strstr(filename[i], "yell") != NULL && n == 1){
            int account = split_space(filename[i]);
            execute_yell(sockfd, account);
            return 0;
        }
        else if(strstr(filename[i], "name") != NULL && n == 1){
            int account = split_space(filename[i]);
            execute_name(sockfd);
            return 0;
        }
        if(i == n-1){
            last = 1;
            if(get_input==1){output = output_fd; get_input = 3; }
            else if(get_input==2) get_input = 4;
        }

        enter = 0;
        if(first == 1){
            for(int j=0; j<1000; j++){
                if(client_pipe[j].input_count == 1 && client_pipe[j].fd == sockfd){
                    input = client_pipe[j].input_num;
                    output = client_pipe[j].output_num;
                    enter = 1;
                    printf("HAS PIPE INPUT\n");
                }
                if(client_pipe[j].fd == sockfd) client_pipe[j].input_count--;
            }
        }
        if(enter==1) close(output);
        int *get_data = parser(input, first, last, wait, enter, pipe_err_n, output_fd, get_input, filename[i], sockfd);
        if(input) close(input);
        input = get_data[0];
        output = get_data[1];
        if(i == n-1){
            if(get_input == 0) close(output);
            if(get_input == 4){
                client_pipe[pos].input_num = input;
                client_pipe[pos].output_num = output;
                pos++;
                if(pos > 1000) pos=0;
            }

        }
        else{ close(output);}
        first = 0;
        if(i!=0 && i%200==0) sleep(3);
    }
    return 0;
}

int *parser(int input, int first, int last, int wait, int enter, int n_pipe_err, int output, int get_input, char *cmd, int sockfd){
    int pid, pipe_fd[2];
    char cmd_dir[20];
    int args_num, cmd_args_num, output_fd;
    char *old_cmd = strdup(cmd);
    char *envValue = do_printenv(sockfd, "PATH");

    args_num = split_space(cmd);
    cmd_args_num = split_env(envValue);

    while (pipe(pipe_fd) < 0) ;

    printf("input:%d output:%d\n",input, output);
    printf("pipe_fd:%d %d\n", pipe_fd[0], pipe_fd[1]);
    pid = fork();
    if(pid == 0){
        char buff[100];
        dup2(sockfd, STDOUT_FILENO);
        if(first == 1 && last == 0 && input == 0){
            dup2(pipe_fd[1], STDOUT_FILENO);
            dup2(sockfd, STDERR_FILENO);
        }
        else if(first == 0 && last == 0 && input != 0){
            dup2(input, 0);
            dup2(pipe_fd[1], STDOUT_FILENO);
            dup2(sockfd, STDERR_FILENO);
        }
        else if(first == 1 && last == 1  && get_input == 4 && input == 0){
            dup2(pipe_fd[1], STDOUT_FILENO);
            dup2(sockfd, STDERR_FILENO);
        }
        else if(first == 1 && last == 1  && get_input == 4 && input != 0){
            dup2(input, STDIN_FILENO);
            dup2(pipe_fd[1], STDOUT_FILENO);
            dup2(sockfd, STDERR_FILENO);
        }
        else if(first == 1 && last == 0 && input != 0){
            dup2(input, STDIN_FILENO);
            dup2(pipe_fd[1], STDOUT_FILENO);
            dup2(sockfd, STDERR_FILENO);
        }
        else {
            dup2(input, STDIN_FILENO);
            dup2(output, STDOUT_FILENO);
            dup2(output, STDERR_FILENO);
        }
        if(n_pipe_err && get_input!=3) dup2(pipe_fd[1], 2);
        else if(n_pipe_err && get_input==3)dup2(output, 2);

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
        if(((__str = strchr(old_cmd, '>')) && (__str[1] != ' ')) && (__str2 = strchr(old_cmd, '<')))
            order = (strlen(__str) > strlen(__str2))? 0 : 1;
        if((__str = strchr(old_cmd, '<')) && order == 0){
            char *pipeid = strdup(old_cmd);
            char *tmp = strtok(pipeid, "<");
            char *_str = strtok(pipeid, "<");
            pipeid = strtok(NULL, "<");
            pipeid = strtok(pipeid, " ");
            int dstid;
            int old_fd = dup(STDOUT_FILENO);
            for(int i = 0; i < clients_map.size; ++i){
                if(clients_map.clients_fd_set[i] == sockfd) {
                    dstid = i + 1;
                    break;
                }
            }
            for(int i = 0; i<= args_num ; i++ ){
                if(strchr(operand[i], '<')) {
                    operand[i] = NULL;
                    break;
                }
            }
            close(old_fd);
        }
        if((__str = strchr(old_cmd, '>'))){
            if(__str[1] == ' '){
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
            else{
                char *pipeid = strdup(old_cmd);
                char *tmp = strtok(pipeid, ">");
                pipeid = strtok(NULL, ">");
                pipeid = strtok(pipeid, " ");
                int recvid = atoi(pipeid), sendid, old_fd = dup(STDOUT_FILENO);
                for(int i = 0; i < clients_map.size; ++i){
                    if(clients_map.clients_fd_set[i] == sockfd) {
                        sendid = i + 1;
                        break;
                    }
                }
                if(recvid <= 0 || recvid > 30 || clients_map.clients_fd_set[recvid - 1] == 0) return 0;
                if(user_pipe[sendid][recvid].flag == 1) return 0;
                close(old_fd);
                dup2(pipe_fd[1], STDOUT_FILENO);
                for(int i = 0; i<= args_num ; i++ ){
                    if(strchr(operand[i], '>')) {
                        operand[i] = NULL;
                        break;
                    }
                }
            }
        }
        if((__str = strchr(old_cmd, '<')) && order == 0){
            char *pipeid = strdup(old_cmd);
            char *tmp = strtok(pipeid, "<");
            pipeid = strtok(NULL, "<");
            pipeid = strtok(pipeid, " ");
            int recvid, old_fd = dup(STDOUT_FILENO);
            for(int i = 0; i < clients_map.size; ++i){
                if(clients_map.clients_fd_set[i] == sockfd) {
                    recvid = i + 1;
                    break;}
            }

            for(int i = 0; i<= args_num ; i++ ){
                if(strchr(operand[i], '<')) {
                    operand[i] = NULL;
                    break;
                }
            }
            close(old_fd);
        }
        free(old_cmd);

        for (int i = 0; i < cmd_args_num; i++){
            strcpy(cmd_dir, argv_list[i]);
            strcat(cmd_dir, "/");
            strcat(cmd_dir, operand[0]);
            if (execvp(cmd_dir, operand) != -1)break;
            if(i==cmd_args_num-1)
                fprintf(stderr, "Unknown command: [%__str].\n", operand[0]);
        }
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        exit(0);

    }
    else {
        char *str;
        if((str = strchr(old_cmd, '>'))){
            if(str[1] != ' '){
                char *pipeid = strdup(old_cmd);
                char *tmp = strtok(pipeid, ">");
                pipeid = strtok(NULL, ">");
                pipeid = strtok(pipeid, " ");
                int recvid = atoi(pipeid);
                int i, sendid;
                int old_fd = dup(STDOUT_FILENO);
                for(i = 0; i < clients_map.size; ++i){
                    if(clients_map.clients_fd_set[i] == sockfd) {
                        sendid = i + 1;
                        break;
                    }
                }
                if(!(recvid <= 0 || recvid > 30 || clients_map.clients_fd_set[recvid - 1] == 0) && user_pipe[sendid][recvid].flag == 0){
                    user_pipe[sendid][recvid].flag = 1;
                    user_pipe[sendid][recvid].pipe_fd = pipe_fd[0];
                }

                printf("%d %d %d \n", sendid, recvid, user_pipe[sendid][recvid].flag);
                close(old_fd);
            }
        }
        if((str = strchr(old_cmd, '<'))){
            char *pipeid = strdup(old_cmd);
            char *tmp = strtok(pipeid, "<");
            pipeid = strtok(NULL, "<");
            pipeid = strtok(pipeid, " ");
            int sendid = atoi(pipeid);
            int recvid, old_fd = dup(STDOUT_FILENO);
            for(int i = 0; i < clients_map.size; ++i){
                if(clients_map.clients_fd_set[i] == sockfd) { recvid = i + 1; break;}
            }
            if(!(sendid <= 0 || sendid > 30 || clients_map.clients_fd_set[sendid - 1] == 0) && user_pipe[sendid][recvid].flag == 1  ){
                user_pipe[sendid][recvid].flag = 0;
                close(user_pipe[sendid][recvid].pipe_fd);
            }
            close(old_fd);
        }
        if(wait==1 && last==1) waitpid(pid,0,0);
    }

    int *pipedata = (int *)malloc(sizeof(pipe_fd));
    memcpy(pipedata, pipe_fd, 2);
    return pipedata;
}
