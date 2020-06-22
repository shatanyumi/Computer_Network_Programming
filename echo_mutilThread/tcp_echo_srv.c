#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <syscall.h>
#include <pthread.h>
#include <signal.h>

#define MAX_CMD_STR 124
#define MAXLINE 8192
#define LISTENQ 8192

#define bprintf(fp,format,...)\
        if(fp==NULL){printf(format,##__VA_ARGS__);}\
        else {printf(format,##__VA_ARGS__);\
                fprintf(fp,format,##__VA_ARGS__);fflush(fp);}

typedef struct sockaddr *pSA ;
typedef void handler_t(int);

FILE *stu_srv_res_p;
pthread_mutex_t mutex;
int TID;
int sig_type = 0;
int sig_to_exit = 0;

typedef struct PDU{
    int PIN;
    int LEN;
    char BUFFER[MAX_CMD_STR];
}pdu;

void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

void posix_error(int code, char *msg) /* Posix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(code));
    exit(0);
}

void sig_int(int signo)
{
    bprintf(stu_srv_res_p,"[srv](%d) SIGINT is coming!\n",TID);
    sig_type = signo;
    if(signo == SIGINT)
        sig_to_exit = 1;
}

void sig_pipe(int signo)
{
    bprintf(stu_srv_res_p,"[srv](%d) SIGPIPE is coming!\n",TID);
    sig_type = signo;
}

handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if(signum != SIGINT)
        action.sa_flags |= SA_RESTART;

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

void Close(int fd)
{
    if(close(fd)<0)
        unix_error("Close error");
}

void Fclose(FILE *fp)
{
    if(fclose(fp)!=0)
        unix_error("Fclose error");
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
    // 在bind之前打印输出监听情况
    bprintf(stu_srv_res_p,"[srv](%d) server[%s:%s] is initializing!\n",TID,ip,port);
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

void Rename(char *oldname, char *newname)
{
    if(rename(oldname,newname)<0)
        unix_error("Rename error");
}

FILE *Fopen(const char *filename,const char *mode)
{
    FILE *fp;
    if((fp = fopen(filename,mode))==NULL)
        unix_error("Fopen error");
    return fp;
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

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp,
                    void * (*routine)(void *), void *argp)
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
        posix_error(rc, "Pthread_create error");
}

void Pthread_cancel(pthread_t tid) {
    int rc;

    if ((rc = pthread_cancel(tid)) != 0)
        posix_error(rc, "Pthread_cancel error");
}

void Pthread_join(pthread_t tid, void **thread_return) {
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0)
        posix_error(rc, "Pthread_join error");
}

void Pthread_detach(pthread_t tid) {
    int rc;

    if ((rc = pthread_detach(tid)) != 0)
        posix_error(rc, "Pthread_detach error");
}

void Pthread_exit(void *retval) {
    pthread_exit(retval);
}

pthread_t Pthread_self(void) {
    return pthread_self();
}

void Pthread_once(pthread_once_t *once_control, void (*init_function)()) {
    pthread_once(once_control, init_function);
}

void echo(int sockfd)
{
    int pin;
    int len;
    pdu echo_rqt_msg,echo_rep_msg;
    char fn_res[50];
    sprintf(fn_res,"stu_srv_res_%d.txt",TID);
    FILE *fp_res = Fopen(fn_res,"w");
    bprintf(NULL,"[srv](%d) %s is opened!\n",TID,fn_res);

    memset(&echo_rqt_msg,0,sizeof(echo_rqt_msg));
    while (rio_readn(sockfd,&echo_rqt_msg.PIN,sizeof(int))>0){
        memset(&echo_rep_msg,0,sizeof(echo_rep_msg));

        Rio_readn(sockfd,&echo_rqt_msg.LEN,sizeof(int));
        pin = ntohl(echo_rqt_msg.PIN);
        len = ntohl(echo_rqt_msg.LEN);
        Rio_readn(sockfd,echo_rqt_msg.BUFFER,sizeof(char)*len);
        echo_rqt_msg.BUFFER[len] = '\0';
        bprintf(fp_res,"[echo_rqt](%d) %s\n",TID,echo_rqt_msg.BUFFER);

        //echo_rep_msg发送
        echo_rep_msg = echo_rqt_msg;
        Rio_writen(sockfd,&echo_rep_msg,sizeof(int)+sizeof(int)+len);
        memset(&echo_rqt_msg,0,sizeof(echo_rqt_msg));
    }

    Close(sockfd);
    bprintf(fp_res,"[srv](%d) connfd is closed!\n",TID);
    Fclose(fp_res);
    bprintf(NULL,"[srv](%d) %s is closed!\n",TID,fn_res);

    char fn_res_new[50];
    memset(fn_res_new,0,sizeof(fn_res_new));
    sprintf(fn_res_new,"stu_srv_res_%d.txt",pin);
    Rename(fn_res, fn_res_new);
    bprintf(NULL,"[srv](%d) res file rename done!\n",TID);
}

void *thread(void *vargp)
{
    int fd = *((int*)vargp);
    Pthread_detach(Pthread_self());
    echo(fd);
    return NULL;
}

int main(int argc,char **argv)
{
    TID = syscall(SYS_gettid);
    int listenfd,connfd;
    socklen_t clientlen;
    struct sockaddr_in  clientaddr;
    pthread_t tid;

    if(argc !=3){
        fprintf(stderr,"usage: %s <host> <port>",argv[0]);
        exit(0);
    }
    stu_srv_res_p = Fopen("stu_srv_res_p.txt","w");
    bprintf(NULL,"[srv](%d) stu_srv_res_p.txt is opened!\n",TID);

    Signal(SIGINT,sig_int);
    Signal(SIGPIPE,sig_pipe);
    listenfd = Open_listenfd(argv[1],argv[2]);

    char client_ip[32];
    while(!sig_to_exit){
        clientlen = sizeof(struct sockaddr_in);
        connfd = accept(listenfd,(pSA)&clientaddr,&clientlen);
        if(connfd <0 ) continue;

        memset(client_ip,0,sizeof(client_ip));
        inet_ntop(AF_INET, &clientaddr.sin_addr,client_ip, sizeof(client_ip));
        bprintf(stu_srv_res_p,"[srv](%d) client[%s:%hu] is accepted!\n",TID,client_ip,ntohs(clientaddr.sin_port));
        Pthread_create(&tid,NULL,thread,&connfd);
    }
    Close(listenfd);
    bprintf(stu_srv_res_p,"[srv](%d) listenfd is closed!\n",TID);
    Fclose(stu_srv_res_p);
    bprintf(NULL,"[srv](%d) stu_srv_res_p.txt is closed!\n",TID);
    return 0;
}