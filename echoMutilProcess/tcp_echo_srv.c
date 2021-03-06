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
#define LISTENQ 1024
#define MAX_BUF_SIZE 8192

#define bprintf(fp,format,...)\
        if(fp==NULL){printf(format,##__VA_ARGS__);}\
        else {printf(format,##__VA_ARGS__);\
                fprintf(fp,format,##__VA_ARGS__);fflush(fp);}

typedef struct sockaddr *pSA ;
typedef void handler_t(int);

//直接构造pdu，利用内存的连续做序列化
typedef struct PDU{
    int PIN;
    int LEN;
    char BUFFER[MAX_CMD_STR];
}pdu;

int sig_to_exit = 0;
int sig_type = 0;
FILE *stu_srv_res_p;

void unix_error(char *msg)
{
    fprintf(stderr,"%s %s",msg,strerror(errno));
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

void sig_chld(int signo)
{
    bprintf(stu_srv_res_p,"[srv](%d) SIGCHLD is coming!\n",getpid());
    sig_type = signo;
    while(waitpid(-1, 0, WNOHANG) >0);
}

void sig_int(int signo)
{
    bprintf(stu_srv_res_p,"[srv](%d) SIGINT is coming!\n",getpid());
    sig_type = signo;
    if(signo == SIGINT)
        sig_to_exit = 1;
}

void sig_pipe(int signo)
{
    bprintf(stu_srv_res_p,"[srv](%d) SIGPIPE is coming!\n",getpid());
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

FILE *Fopen(const char *filename,const char *mode)
{
    FILE *fp;
    if((fp = fopen(filename,mode))==NULL)
        unix_error("Fopen error");
    return fp;
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
    bprintf(stu_srv_res_p,"[srv](%d) server[%s:%s] is initializing!\n",getpid(),ip,port);
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

pid_t Fork()
{
    pid_t pid;

    if((pid = fork())<0)
        unix_error("Fork error");
    return pid;
}

void Rename(char *oldname, char *newname, FILE *stream, pid_t pid)
{
    if(rename(oldname,newname)<0)
        unix_error("Rename error");
    bprintf(stream,"[srv](%d) res file rename done!\n",pid);
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

void echo(int sockfd,FILE *fp_res,pid_t pid,int *PIN)
{
    int pin;
    int len;
    pdu echo_rqt_msg,echo_rep_msg;
    //收到echo_rqt_msg后转化
    memset(&echo_rqt_msg,0,sizeof(echo_rqt_msg));
    while (rio_readn(sockfd,&echo_rqt_msg.PIN,sizeof(int))>0){
        memset(&echo_rep_msg,0,sizeof(echo_rep_msg));

        Rio_readn(sockfd,&echo_rqt_msg.LEN,sizeof(int));
        pin = ntohl(echo_rqt_msg.PIN);
        len = ntohl(echo_rqt_msg.LEN);
        Rio_readn(sockfd,echo_rqt_msg.BUFFER,sizeof(char)*len);
        echo_rqt_msg.BUFFER[len] = '\0';
        bprintf(fp_res,"[echo_rqt](%d) %s\n",pid,echo_rqt_msg.BUFFER);

        //pin通过引用发送到函数外
        *PIN = pin;
        //echo_rep_msg发送
        echo_rep_msg = echo_rqt_msg;
        Rio_writen(sockfd,&echo_rep_msg,sizeof(int)+sizeof(int)+len);
        memset(&echo_rqt_msg,0,sizeof(echo_rqt_msg));
    }
}

int main(int argc,char **argv)
{
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len;
    int listenfd,connfd;

    Signal(SIGPIPE,sig_pipe);
    Signal(SIGINT,sig_int);
    Signal(SIGCHLD,sig_chld);

    if(argc != 3)
    {
        printf("Usage: <IPaddress><Port>");
        exit(1);
    }

    // 父进程启动以前打开 stu_srv_res_p.txt
    stu_srv_res_p = Fopen("stu_srv_res_p.txt","w");
    bprintf(NULL,"[srv](%d) stu_srv_res_p.txt is opened!\n",getpid());

    listenfd = Open_listenfd(argv[1],argv[2]);
    char cli_ip[32];
    while(!sig_to_exit)
    {
        cli_addr_len = sizeof(struct sockaddr_in);
        connfd = accept(listenfd,(pSA)&cli_addr,&cli_addr_len);
        if(connfd <0 ) continue;

        memset(cli_ip,0,sizeof(cli_ip));
        inet_ntop(AF_INET, &cli_addr.sin_addr,cli_ip, sizeof(cli_ip));
        bprintf(stu_srv_res_p,"[srv](%d) client[%s:%hu] is accepted!\n",getpid(),cli_ip,ntohs(cli_addr.sin_port));

        if(Fork()==0)
        {
            Fclose(stu_srv_res_p);

            char fn_res[50];
            pid_t child_pid = getpid();
            memset(fn_res,0,sizeof(fn_res));
            sprintf(fn_res,"stu_srv_res_%d.txt",child_pid);
            FILE *fp_res = Fopen(fn_res,"w");
            bprintf(NULL,"[srv](%d) %s is opened!\n",child_pid,fn_res);
            bprintf(fp_res,"[srv](%d) child process is created!\n",child_pid);

            Close(listenfd);
            bprintf(fp_res,"[srv](%d) listenfd is closed!\n",child_pid);

            int PIN;
            echo(connfd,fp_res,child_pid,&PIN);

            char fn_res_new[50];
            memset(fn_res_new,0,sizeof(fn_res_new));
            sprintf(fn_res_new,"stu_srv_res_%d.txt",PIN);

            Rename(fn_res, fn_res_new, fp_res, child_pid);

            bprintf(fp_res,"[srv](%d) connfd is closed!\n",child_pid);
            Close(connfd);

            bprintf(fp_res,"[srv](%d) child process is going to exit!\n",child_pid);
            Fclose(fp_res);

            bprintf(NULL,"[srv](%d) stu_cli_res%d.txt is closed!\n",child_pid,child_pid);
            exit(0);
        }
        Close(connfd);
    }
    Close(listenfd);

    bprintf(stu_srv_res_p,"[srv](%d) listenfd is closed!\n",getpid());
    bprintf(stu_srv_res_p,"[srv](%d) parent process is going to exit!\n",getpid());
    Fclose(stu_srv_res_p);
    bprintf(NULL,"[srv](%d) stu_srv_res_p.txt is closed!\n",getpid());
    return 0;
}