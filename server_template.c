#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/unistd.h>

#include <sys/errno.h>
#include <signal.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <sys/wait.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <sys/time.h>


#define QLEN 50
#define ACMODE 0666
#define SHMKEY 9487
#define SEMKEY 9487
#define SEMCNT 1

int passivesock(const char * port, int qlen);
void errexit(const char *content, ...);

int main(int argc, char *argv[]) {

    struct sockaddr_in fsin;   /* the address of a client */
    int	alen;	               /* length of client's address */
    int	msock;	               /* master server socket */
    int	ssock;	               /* slave server socket */
    if(argc!=2) errexit("usage: XXX [port]\n");

    msock = passivesock(argv[1], QLEN);
    
    while (1) {
        alen = sizeof(fsin);
        ssock = accept(msock, (struct sockaddr *) &fsin, (socklen_t*)&alen);
        if (ssock < 0) {
            if (errno == EINTR) continue;
            errexit("accept: %s\n", strerror(errno));
        }
		pid_t pid = fork();
        switch(pid){
            case 0:		/* child */
                close(msock);
                // TODO
                // exit(shop_service(ssock));
            default:	/* parent */
                close(ssock);
				printf("client PID:%d\n", pid);
                break;
            case -1:
                errexit("fork: %s\n", strerror(errno));
        }
    }
}

int passivesock(const char * port, int qlen)
{
    struct sockaddr_in sin; /* an Internet endpoint address	*/
    int	s;                  /* socket descriptor and socket type	*/

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons((u_short)atoi(port));

    /* Allocate a socket */
    s = socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0) errexit("can't create socket: %s\n", strerror(errno));

    /* Bind the socket */
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errexit("can't bind to port: %s\n", strerror(errno));

    if (listen(s, qlen) < 0) errexit("can't listen on port: %s\n", strerror(errno));
    return s;
}

void errexit(const char *content, ...){
    va_list args;
    va_start(args, content);
    vprintf(content, args);
    va_end(args);
    exit(0);
}
