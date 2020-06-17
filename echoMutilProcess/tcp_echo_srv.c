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

typedef struct sockaddr *pSA ;

int sig_to_exit = 0;
int sig_type = 0;

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

void sig_chld(int signo){
    sig_type = signo;
    pid_t pid_chld;
    while(0 < (pid_chld = waitpid(-1, 0, WNOHANG)));
    printf("[srv](%d) server child(%d) terminated.\n",getpid(),pid_chld);
}

void sig_int(int signo) {
    sig_type = signo;
    if(signo == SIGINT)
        sig_to_exit = 1;
}

void sig_pipe(int signo) {
    sig_type = signo;
    printf("[srv](%d) SIGPIPE is coming!\n",getpid());
}

__sighandler_t *Signal(int signum,__sighandler_t sighandler){
    struct sigaction sigact, old_sigact;
    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_flags |= SA_RESTART;
    if(sigaction(SIGPIPE, &sigact, &old_sigact)<0)
        unix_error("Signal error");
    return (old_sigact.sa_handler);
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

int open_listenfd(char *ip,char *port,FILE *file){
    int listenfd, optval=1;
    struct sockaddr_in srv_addr;
    char buf[MAX_CMD_STR];

    if((listenfd = socket(AF_INET,SOCK_STREAM,0))<0)
        return -1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&srv_addr.sin_addr);
    srv_addr.sin_port = htons(atoi(port));

    // 在bind之前打印输出监听情况
    sprintf(buf,"[srv](%d) server[%s:%s] is initializing!\n",getpid(),ip,port);
    Fwrite(buf,file);

    if(bind(listenfd,(pSA)&srv_addr,sizeof(struct sockaddr_in)) == -1)
        return -1;

    if(listen(listenfd,LISTENQ) == -1)
        return -1;

    return listenfd;
}

int Open_listenfd(char *ip,char *port,FILE *file){
    int rc;
    if((rc = open_listenfd(ip,port,file))<0)
        unix_error("Open_listenfd");
    return rc;
}

int Accept(int fd,struct sockaddr *addr,socklen_t *addrlen){
    int rc;
    if((rc = accept(fd,addr,addrlen))<0)
        unix_error("Accept error");
    return rc;
}

pid_t Fork(){
    pid_t pid;

    if((pid = fork())<0)
        unix_error("Fork error");
    return pid;
}


void Rename(char *oldname, char *newname, FILE *stream, pid_t pid) {

    if(rename(oldname,newname)<0)
        unix_error("Rename error");

    char buf[MAX_CMD_STR];
    memset(buf,0,sizeof(buf));
    sprintf(buf,"[srv](%d) res file rename done!\n",pid);
    Fwrite(buf,stream);
}

void echo(int sockfd,FILE *fp_res,pid_t pid,int *pin) {
    int res;
    int len;
    char *buf;

    if(read(sockfd,&pin,sizeof(int))<0){
        printf("[srv](%d) read pin return %d and errno is %d\n",pid,res, errno);
        if(errno == EINTR){
            if(sig_type == SIGINT){
                return;
            }
        }
    }

    if(read(sockfd,&len,sizeof(int))<0){
        printf("[srv](%d) read len return %d and errno is %d\n",pid,res, errno);
        if(errno == EINTR){
            if(sig_type == SIGINT)
                return;
        }
    }

    buf = (char *)malloc(sizeof(char)*len);
    if(read(sockfd,buf,sizeof(char)*len)<0){
        printf("[srv](%d) read buf return %d and errno is %d\n",pid,res, errno);
        if(errno == EINTR){
            if(sig_type == SIGINT) {
                free(buf);
                return;
            }
        }
        free(buf);
    }

    printf("%s\n",buf);
    char buffer[MAX_CMD_STR+124];
    memset(buffer,0,sizeof(buffer));
    sprintf(buffer,"[echo_rqt](%d) %s",pid,buf);
    Fwrite(buffer,fp_res);

    if(write(sockfd,&pin,sizeof(int)) == -1){
        printf("write echo_rqt_pdu.HEADER.PIN error \n");
        exit(1);
    }

    if(write(sockfd,&len,sizeof(int)) == -1){
        printf("write echo_rqt_pdu.HEADER.LEN error \n");
        exit(1);
    }

    if(write(sockfd,&buf,sizeof(char)*len) == -1){
        printf("write echo_rqt_pdu.BUF error \n");
        exit(1);
    }
}

int main(int argc,char *argv[])
{
    struct sockaddr_in srv_addr,cli_addr;
    socklen_t cli_addr_len;
    int listenfd,connfd;

    Signal(SIGPIPE,sig_pipe);
    Signal(SIGINT,sig_int);
    Signal(SIGCHLD,sig_chld);

    if(argc != 3){
        printf("Usage: server <IPaddress><Port>");
        exit(1);
    }

    // 父进程启动以前打开 stu_srv_res_p.txt
    FILE *stu_srv_res_p = Fopen("stu_srv_res_p.txt","wb");

    listenfd = Open_listenfd(argv[1],argv[2],stu_srv_res_p);
    char cli_ip[32];
    char buf[MAX_CMD_STR];
    while(!sig_to_exit)
    {
        cli_addr_len = sizeof(cli_addr);
        connfd = Accept(listenfd,(pSA)&cli_addr,&cli_addr_len);
        if(connfd == -1 && errno == EINTR && sig_type == SIGINT) break;

        memset(cli_ip,0,sizeof(cli_ip));
        memset(buf,0,sizeof(buf));
        inet_ntop(AF_INET, &cli_addr.sin_addr,cli_ip, sizeof(cli_ip));
        sprintf(buf,"[srv](%d) client[%s:%hu] is accepted!",getpid(),cli_ip,ntohs(cli_addr.sin_port));
        Fwrite(buf,stu_srv_res_p);

        if(Fork()==0){
            Close(listenfd);

            char fn_res[50];
            pid_t child_pid = getpid();
            memset(fn_res,0,sizeof(fn_res));
            sprintf(fn_res,"stu_srv_res_%d.txt",child_pid);
            FILE *fp_res = Fopen(fn_res,"wb");
            printf("[srv](%d) %s is opened!\n",child_pid,fn_res);

            memset(buf,0,sizeof(buf));
            sprintf(buf,"[cli](%d) child process is created!\n",child_pid);
            Fwrite(buf,fp_res);

            int PIN;
            echo(connfd,fp_res,child_pid,&PIN);

            char fn_res_new[50];
            memset(fn_res_new,0,sizeof(fn_res_new));
            sprintf(fn_res_new,"stu_srv_res_%d.txt",PIN);

            Rename(fn_res, fn_res_new, fp_res, child_pid);

            memset(buf,0,sizeof(buf));
            sprintf(buf,"[srv](%d) connfd is closed!\n",child_pid);
            Fwrite(buf,fp_res);
            Close(connfd);

            memset(buf,0,sizeof(buf));
            sprintf(buf,"[cli](%d) child process is going to exit!\n",child_pid);
            Fwrite(buf,fp_res);
            Fclose(fp_res);

            printf("[srv](%d) stu_cli_res%d.txt is closed!\n",child_pid,child_pid);
            exit(0);
        }
        Close(connfd);
    }
    Close(listenfd);
    memset(buf,0,sizeof(buf));
    sprintf(buf,"[srv](%d) listenfd is closed!\n",getpid());
    Fwrite(buf,stu_srv_res_p);

    memset(buf,0,sizeof(buf));
    sprintf(buf,"[srv](%d) parent process is going to exit!\n",getpid());
    Fwrite(buf,stu_srv_res_p);

    Fclose(stu_srv_res_p);
    printf("[srv])(%d) stu_srv_res_p.txt is closed!\n",getpid());
    return 0;
}