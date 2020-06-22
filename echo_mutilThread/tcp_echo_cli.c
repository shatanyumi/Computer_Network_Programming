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
#define bprintf(fp,format,...)\
        if(fp==NULL){printf(format,##__VA_ARGS__);}\
        else {printf(format,##__VA_ARGS__);\
                fprintf(fp,format,##__VA_ARGS__);fflush(fp);}

typedef struct sockaddr* pSA;
int  pin;
FILE *fp_res;
pthread_mutex_t mutex;
int TID;
int clientfd;

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

void Connect(int sockfd, struct sockaddr *serv_addr, int addrlen)
{
    int rc;

    if ((rc = connect(sockfd, serv_addr, addrlen)) < 0)
        unix_error("Connect error");
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

int open_clientfd(char *ip,char *port)
{
    int fd;
    struct sockaddr_in srv_addr;

    fd = Socket(AF_INET,SOCK_STREAM,0);

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(port));
    inet_pton(AF_INET,ip,&srv_addr.sin_addr);

    Connect(fd,(pSA)&srv_addr,sizeof(struct sockaddr_in));

    return fd;
}

int Open_clientfd(char *ip,char *port)
{
    int rc;
    if((rc = open_clientfd(ip,port))<0)
        unix_error("Open_clientfd Unix error");
    return rc;
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

void Read()
{
    int len,Pin;
    pdu echo_rep_msg;

    while(rio_readn(clientfd,&echo_rep_msg.PIN,sizeof(echo_rep_msg.PIN))>0){
        Rio_readn(clientfd,&echo_rep_msg.LEN,sizeof(echo_rep_msg.LEN));
        Pin = ntohl(echo_rep_msg.PIN);
        len = ntohl(echo_rep_msg.LEN);
        Rio_readn(clientfd,echo_rep_msg.BUFFER,sizeof(char)*len);
        echo_rep_msg.BUFFER[len] = '\0';

        pthread_mutex_lock(&mutex);
        bprintf(fp_res,"[echo_rep](%d) %s\n",TID,echo_rep_msg.BUFFER);
        pthread_mutex_unlock(&mutex);
    }
    Close(clientfd);
    pthread_mutex_lock(&mutex);
    bprintf(fp_res,"[cli](%d) connfd is closed!\n",TID);
    bprintf(fp_res,"[cli](%d) parent thread is going to exit!\n",TID);
    pthread_mutex_unlock(&mutex);
}

void *Write(void *vargp)
{

    Pthread_detach(pthread_self());
    pthread_mutex_lock(&mutex);
    bprintf(fp_res,"[cli](%d) pthread_detach is done!\n",TID);
    pthread_mutex_unlock(&mutex);

    char td_name[50];
    sprintf(td_name,"td%d.txt",pin);
    FILE *fp_read = Fopen(td_name,"r");
    pdu echo_rqt_msg;
    char rbuf[MAX_CMD_STR+1];

    while (fgets(rbuf, sizeof(rbuf),fp_read)!=NULL) {
        if(strncmp(rbuf,"exit",4) == 0) {
            shutdown(clientfd,SHUT_WR);
            pthread_mutex_lock(&mutex);
            bprintf(fp_res,"[cli](%d) shutdown is called with SHUT_WR!\n",TID);
            pthread_mutex_unlock(&mutex);
            break;
        }
        int len = strnlen(rbuf,MAX_CMD_STR);
        rbuf[len] = '\0';
        //建立echo_rqt_msg并发送
        echo_rqt_msg.PIN = htonl(pin);
        echo_rqt_msg.LEN = htonl(len);
        strcpy(echo_rqt_msg.BUFFER,rbuf);
        Rio_writen(clientfd,&echo_rqt_msg,sizeof(int)+sizeof(int)+len);
    }
    Fclose(fp_read);
    return NULL;
}

void echo_rqt()
{
    pthread_t tid;
    Pthread_create(&tid,NULL,Write,NULL);
    Read();
}

int main(int argc,char **argv){
    if(argc != 4){
        fprintf(stderr,"usage: %s <host> <ip> <pin>",argv[0]);
        exit(0);
    }
    pin = atoi(argv[3]);
    TID = syscall(SYS_gettid);

    char filename[MAXLINE];
    sprintf(filename,"stu_cli_res_%d.txt",pin);
    fp_res = Fopen(filename,"w");
    bprintf(NULL,"[cli](%d) %s is created!\n",TID,filename);

    clientfd = Open_clientfd(argv[1],argv[2]);
    bprintf(fp_res,"[cli](%d) server[%s:%s] is connected!\n",TID,argv[1],argv[2]);

    pthread_mutex_init(&mutex,NULL);
    echo_rqt();

    bprintf(NULL,"[cli](%d) %s is closed!\n",TID,filename);
    exit(0);
}