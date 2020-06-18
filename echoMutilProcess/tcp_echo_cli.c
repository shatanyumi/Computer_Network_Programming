#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define MAX_CMD_STR 124
#define MAXLINE 8192

typedef struct sockaddr *pSA;

void unix_error(char *msg){
    fprintf(stderr,"%s %s",msg,strerror(errno));
    exit(0);
}

void Close(int fd){
    if(close(fd)<0)
        unix_error("Close error");
}

void Fclose(FILE *fp){
    if(fclose(fp)!=0)
        unix_error("Fclose error");
}

FILE *Fopen(const char *filename,const char *mode){
    FILE *fp;
    if((fp = fopen(filename,mode))==NULL)
        unix_error("Fopen error");
    return fp;
}

void Fwrite(const char *buffer,FILE *stream){
    if(fputs(buffer,stream)<0)
        unix_error("Fwrite error");
}

pid_t Fork(){
    pid_t pid;

    if((pid = fork())<0)
        unix_error("Fork error");
    return pid;
}

int open_clientfd(char *ip,char *port){
    int clientfd;
    struct sockaddr_in srv_addr;

    if((clientfd = socket(AF_INET,SOCK_STREAM,0))<0)
        return -1;

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(port));
    inet_pton(AF_INET,ip,&srv_addr.sin_addr);

    if(connect(clientfd,(pSA)&srv_addr,sizeof(struct sockaddr_in))<0)
        return -1;

    return clientfd;
}

int Open_clientfd(char *ip,char *port){
    int rc;
    if((rc = open_clientfd(ip,port))<0)
        unix_error("Open_clientfd Unix error");
    return rc;
}

ssize_t rio_read(int fd, void *usrbuf, size_t n)
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

void Rio_read(int fd,void *userbuf,size_t n){
    if(rio_read(fd,userbuf,n)<0)
        unix_error("Rio_read error");
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

void Rio_writen(int fd,void *userbuf,size_t n){
    if(rio_writen(fd,userbuf,n) != n)
        unix_error("Rio_writen error");
}

void echo_rqt(int sockfd,int pin,pid_t PID,FILE *fnres) {
    char buf[MAX_CMD_STR+1];
    char td_name[50];
    sprintf(td_name,"td%d.txt",pin);
    FILE *fp = Fopen(td_name,"r");

    while (fgets(buf, sizeof(buf),fp) != NULL) {
        if(strncmp(buf,"exit",4) == 0) return 0;
        int len = strnlen(buf,MAX_CMD_STR);
        buf[len] = '\0';
        printf("%s\n",buf);

        Rio_writen(sockfd,&pin,sizeof(int));
        Rio_writen(sockfd,&len,sizeof(int));
        Rio_writen(sockfd,buf,sizeof(buf));

        Rio_read(sockfd,&pin,sizeof(int));
        Rio_read(sockfd,&len,sizeof(int));
        Rio_read(sockfd,buf,sizeof(char)*len);

        char result[MAX_CMD_STR+124];
        sprintf(result,"[echo_rep](%d) %s",PID,buf);
        Fwrite(result,fnres);
    }
    Fclose(fp);
}

int main(int argc,char *argv[])
{
    char *ip,*port,buf[MAXLINE];
    //最大并发数目
    int mutl_num;
    if(argc != 4){
        printf("Usage: %s <IP> <PORT> <process>\n", argv[0]);
        return 0;
    }

    ip = argv[1];
    port = argv[2];

    pid_t pid;
    int pin;
    for(pin= mutl_num-1;pin>0;pin--){
        pid = Fork();
        if(pid == -1|| pid == 0) break;
    }

    if(pid<0){
        printf("process error!\n");
        exit(1);
    }
    else if(pid == 0){
        pid_t PID = getpid();

        char file_name[50];
        sprintf(file_name,"stu_cli_res_%d.txt",pin);
        printf("[cli](%d) %s is created!\n",PID,file_name);

        FILE *fnres = Fopen(file_name,"wb");

        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) child process %d is created!\n",PID,pin);
        Fwrite(buf,fnres);

        int client_fd = Open_clientfd(ip,port);

        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) server[%s:%s] is connected!\n",PID,ip,port);
        Fwrite(buf,fnres);

        echo_rqt(client_fd,pin,PID,fnres);

        Close(client_fd);
        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) connfd is closed!\n",PID);
        Fwrite(buf,fnres);

        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) child process is going to exit!\n",PID);
        Fwrite(buf,fnres);

        Fclose(fnres);
        printf("[cli](%d) %s is closed!\n",PID,file_name);
        exit(0);
    }
    else{
        pid_t PID = getpid();

        char file_name[50];
        sprintf(file_name,"stu_cli_res_%d.txt",pin);
        printf("[cli](%d) %s is created!\n",PID,file_name);

        FILE *fnres = Fopen(file_name,"wb");

        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) child process %d is created!\n",PID,pin);
        Fwrite(buf,fnres);

        int client_fd = Open_clientfd(ip,port);

        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) server[%s:%s] is connected!\n",PID,ip,port);
        Fwrite(buf,fnres);

        echo_rqt(client_fd,pin,PID,fnres);

        Close(client_fd);
        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) connfd is closed!\n",PID);
        Fwrite(buf,fnres);

        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) parent process is going to exit!\n",PID);
        Fwrite(buf,fnres);

        Fclose(fnres);
        printf("[cli](%d) %s is closed!\n",PID,file_name);
        exit(0);
    }
}