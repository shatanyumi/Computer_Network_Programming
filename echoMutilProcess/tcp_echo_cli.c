#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAX_CMD_STR 124
#define bprintf(fp,format,...)\
        if(fp==NULL){printf(format,##__VA_ARGS__);}\
        else {printf(format,##__VA_ARGS__);\
                fprintf(fp,format,##__VA_ARGS__);fflush(fp);}

typedef struct sockaddr *pSA ;
typedef void handler_t(int);

//直接用结构体构造pdu，利用内存的连续做序列化
typedef struct {
    int PIN;
    int LEN;
    char BUFFER[MAX_CMD_STR];
}pdu;

void unix_error(char *msg)
{
    fprintf(stderr,"%s %s",msg,strerror(errno));
    exit(0);
}

void sig_chld(int signo)
{
    while(waitpid(-1, 0, WNOHANG) >0);
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

pid_t Fork()
{
    pid_t pid;

    if((pid = fork())<0)
        unix_error("Fork error");
    return pid;
}

ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      /* and call read() again */
            else                /* Interrupted by sig handler return */
                return -1;/* errno set by read() */
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
            if (errno == EINTR)
                nwritten = 0;      /* and call read() again */
                /* Interrupted by sig handler return */
            else
                return -1; /* errno set by read() */
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

void Rio_readn(int fd,char *userbuf,size_t n)
{
    if(rio_readn(fd,userbuf,n)<0)
        unix_error("Rio_read error");

}

int open_clientfd(char *ip,char *port)
{
    int clientfd;
    struct sockaddr_in srv_addr;

    clientfd = Socket(AF_INET,SOCK_STREAM,0);

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(port));
    inet_pton(AF_INET,ip,&srv_addr.sin_addr);

    Connect(clientfd,(pSA)&srv_addr,sizeof(struct sockaddr_in));

    return clientfd;
}

int Open_clientfd(char *ip,char *port)
{
    int rc;
    if((rc = open_clientfd(ip,port))<0)
        unix_error("Open_clientfd Unix error");
    return rc;
}

void echo_rqt(int sockfd,int pin,pid_t PID,FILE *fnres)
{
    char rbuf[MAX_CMD_STR+20];
    char buf[MAX_CMD_STR+20];
    pdu echo_pdu;
    char td_name[50];

    sprintf(td_name,"td%d.txt",pin);
    FILE *fp = Fopen(td_name,"r");

    while (fgets(rbuf, sizeof(rbuf),fp)!=NULL) {
        if(strncmp(rbuf,"exit",4) == 0) break;
        int len = strnlen(rbuf,MAX_CMD_STR);
        rbuf[len] = '\0';
        //printf("[cli](%d) send %d %d %s\n",PID,pin,len,rbuf);

        //构造pdu并写入缓存
        memset(&echo_pdu,0,sizeof(echo_pdu));
        echo_pdu.PIN = htonl(pin);
        echo_pdu.LEN = htonl(len);
        strncpy(echo_pdu.BUFFER,rbuf,len);
        memset(buf,0,sizeof(buf));
        memcpy(buf,&echo_pdu,sizeof(echo_pdu));

        Rio_writen(sockfd,buf,sizeof(buf));

        memset(buf,0,sizeof(buf));
        Rio_readn(sockfd,buf,sizeof(buf));
        memset(&echo_pdu,0,sizeof(echo_pdu));
        memcpy(&echo_pdu,buf,sizeof(echo_pdu));
        echo_pdu.BUFFER[ntohl(echo_pdu.LEN)] = '\0';

        bprintf(fnres,"[echo_rep](%d) %s",PID,echo_pdu.BUFFER);
    }
    Fclose(fp);
}

int main(int argc,char **argv)
{
    char *ip,*port,buf[MAX_CMD_STR];
    //最大并发数目

    Signal(SIGCHLD,sig_chld);
    int mutl_num = 0;
    if(argc != 4)
    {
        printf("Usage: %s <IP> <PORT> <process>\n", argv[0]);
        return 0;
    }

    ip = argv[1];
    port = argv[2];
    mutl_num = atoi(argv[3]);

    pid_t pid;
    int pin;
    for(pin= mutl_num-1;pin>0;pin--)
    {
        pid = Fork();
        if(pid == -1|| pid == 0) break;
    }

    if(pid<0)
    {
        printf("process error!\n");
        exit(1);
    }
    else if(pid == 0)
    {
        pid_t PID = getpid();

        char file_name[50];
        sprintf(file_name,"stu_cli_res_%d.txt",pin);
        FILE *fnres = Fopen(file_name,"w");
        printf("[cli](%d) %s is created!\n",PID,file_name);

        bprintf(fnres,"[cli](%d) child process %d is created!\n",PID,pin);

        int client_fd = Open_clientfd(ip,port);

        bprintf(fnres,"[cli](%d) server[%s:%s] is connected!\n",PID,ip,port);

        echo_rqt(client_fd,pin,PID,fnres);

        Close(client_fd);
        bprintf(fnres,"[cli](%d) connfd is closed!\n",PID);

        bprintf(fnres,"[cli](%d) child process is going to exit!\n",PID);
        Fclose(fnres);
        printf("[cli](%d) %s is closed!\n",PID,file_name);
        exit(0);
    }
    else
    {
        pid_t PID = getpid();

        char file_name[50];
        sprintf(file_name,"stu_cli_res_%d.txt",pin);
        FILE *fnres = Fopen(file_name,"w");
        printf("[cli](%d) %s is created!\n",PID,file_name);

        bprintf(fnres,"[cli](%d) child process %d is created!\n",PID,pin);

        int client_fd = Open_clientfd(ip,port);

        bprintf(fnres,"[cli](%d) server[%s:%s] is connected!\n",PID,ip,port);

        echo_rqt(client_fd,pin,PID,fnres);

        Close(client_fd);
        bprintf(fnres,"[cli](%d) connfd is closed!\n",PID);

        bprintf(fnres,"[cli](%d) parent process is going to exit!\n",PID);
        Fclose(fnres);
        printf("[cli](%d) %s is closed!\n",PID,file_name);
        exit(0);
    }
}