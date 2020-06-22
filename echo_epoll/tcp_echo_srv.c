#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define LISTENQ          8192
#define MAX_EVENT_COUNT  32
#define MAXBUF  8192
#define MAXLINE 8192
typedef struct sockaddr *pSA ;

void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

int Socket(int domain, int type, int protocol)
{
    int rc;

    if ((rc = socket(domain, type, protocol)) < 0)
        unix_error("Socket error");
    return rc;
}

void Bind(int sockfd, struct sockaddr *my_addr, int addrlen)
{
    int rc;

    if ((rc = bind(sockfd, my_addr, addrlen)) < 0)
        unix_error("Bind error");
}

void Listen(int s, int backlog)
{
    int rc;

    if ((rc = listen(s,  backlog)) < 0)
        unix_error("Listen error");
}

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    int rc;

    if ((rc = accept(s, addr, addrlen)) < 0)
        unix_error("Accept error");
    return rc;
}

int open_listenfd(char *ip,char *port)
{
    int listenfd,optval=1;
    struct sockaddr_in srv_addr;

    listenfd = Socket(AF_INET,SOCK_STREAM,0);

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&srv_addr.sin_addr);
    srv_addr.sin_port = htons(atoi(port));
    Bind(listenfd,(pSA)&srv_addr,sizeof(struct sockaddr_in));

    Listen(listenfd,LISTENQ);
    return listenfd;
}

int Open_listenfd(char *ip,char *port)
{
    int rc;
    if((rc = open_listenfd(ip,port))<0)
        unix_error("Open_listenfd");
    return rc;
}

ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno == EINTR) /* Interrupted by sig handler return */
                nread = 0;      /* and call read() again */
            else
                return -1;      /* errno set by read() */
        }
        else if (nread == 0)
            break;              /* EOF */
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);         /* return >= 0 */
}


ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)  /* Interrupted by sig handler return */
                nwritten = 0;    /* and call write() again */
            else
                return -1;       /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}


void Rio_writen(int fd,void *userbuf,size_t n)
{
    if(rio_writen(fd,userbuf,n) != n)
        unix_error("Rio_writen error");
}

void Rio_readn(int fd,void *userbuf,size_t n)
{
    if(rio_readn(fd,userbuf,n)<0)
        unix_error("Rio_read error");

}

int main(int argc,char **argv)
{
    int ret,i;
    int listenfd,epollfd;
    int ready_count;
    struct sockaddr_in clientaddr;
    socklen_t clientlen;
    struct epoll_event  event;
    struct epoll_event* event_array;
    char *buf;

    event_array = (struct epoll_event*)malloc(sizeof(struct epoll_event)*MAX_EVENT_COUNT);
    buf =   (char*)malloc(sizeof(char)*MAXBUF);

    listenfd = Open_listenfd(argv[1],argv[2]);

    epollfd = epoll_create1(0);
    event.events = EPOLLIN;
    event.data.fd = listenfd;

    while(1){
        ready_count = epoll_wait(epollfd,event_array,MAX_EVENT_COUNT,-1);
        if(ready_count==-1){
            unix_error("epoll_wait error");
            exit(0);
        }
        for(i = 0;i<ready_count;i++){
            if(event_array[i].data.fd==listenfd){
                clientlen = sizeof(struct sockaddr_in);
                int clientfd = Accept(listenfd,(pSA*)&clientaddr,&clientlen);

                event.events = EPOLLIN;
                event.data.fd = clientfd;
                ret = epoll_ctl(epollfd,EPOLL_CTL_ADD,clientfd,&event);
                if(ret==-1){
                    unix_error("epoll_ctl error");
                }
            }
            else{
                if(rio_readn(event_array[i].data.fd,buf,MAXBUF)<=0){
                    close(event_array[i].data.fd);
                    epoll_ctl(epollfd,EPOLL_CTL_DEL,event_array[i].data.fd,&event);
                    continue;
                }
                printf("server recieve buffer: %s\n",buf);
                Rio_writen(event_array[i].data.fd,buf,sizeof(char)*strnlen(buf,MAXBUF));
            }
        }

    }

    close(epollfd);
    close(listenfd);
    free(event_array);
    free(buf);
    return 0;
}