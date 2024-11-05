#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
int connectsock(const char *host, const char *port);
void errexit(const char *content, ...);

int main(int argc, char *argv[]){
    
    int	csock;

    if(argc!=2) errexit("usage: [IP] [port]\n");
    csock = connectsock(argv[1], argv[2]);

    close(csock);

}

int connectsock(const char *host, const char *port)
{
    struct sockaddr_in sin; /* an Internet endpoint address	*/
    int	s;                  /* socket descriptor and socket type	*/

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(host);
    sin.sin_port = htons((u_short)atoi(port));

    /* Allocate a socket */
    s = socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0) errexit("can't create socket: %s\n", strerror(errno));

    /* Connect the socket */
    if(connect(s, (struct sock_addr *)&sin, sizeof(sin)) < 0)
        errexit("Can't connect: %s\n", strerror(errno)); 

    return s;
}

void errexit(const char *content, ...){
    va_list args;
    va_start(args, content);
    vprintf(content, args);
    va_end(args);
    exit(0);
}