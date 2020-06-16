#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <wait.h>

#define MAX_CMD_STR 124

typedef struct sockaddr *pSA ;
int sig_to_exit = 0;
int sig_type = 0;
FILE *stu_srv_res_p;

typedef struct PDU{
    int PIN;
    int LEN;
    char BUF[MAX_CMD_STR+1];
}pdu;

void sig_chld(int signo){
    sig_type = signo;
    pid_t pid_chld;
    int stat;
    while(0 < (pid_chld = waitpid(-1, &stat, WNOHANG)));
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

int echo_rep(int sockfd,FILE *fp_res,pid_t pid) {
    char buffer[MAX_CMD_STR];
    int pin = -1;
    pdu echo_rev_pdu;
    if(read(sockfd,&echo_rev_pdu,sizeof(pdu))==-1){
        printf("[srv](%d) echo_rep read error\n",pid);
        exit(1);
    }

    memset(buffer,0,sizeof(buffer));
    sprintf(buffer,"[echo_rqt](%d) %s",pid,echo_rev_pdu.BUF);
    pin = echo_rev_pdu.PIN;
    fputs(buffer,fp_res);

    if(write(sockfd,&echo_rev_pdu,sizeof(pdu))==-1){
        printf("[srv](%d) echo_rep write error\n",pid);
        exit(1);
    }

    return pin;
}

int main(int argc,char *argv[])
{
    char cli_ip[32];
    FILE *fp_res;
    struct sockaddr_in srv_addr,cli_addr;
    socklen_t cli_addr_len;
    int listenfd,connfd;
    char buf[MAX_CMD_STR];

    // 安装SIGPIPE信号处理器
    struct sigaction sigact_pipe, old_sigact_pipe;
    sigact_pipe.sa_handler = sig_pipe;
    sigemptyset(&sigact_pipe.sa_mask);
    sigact_pipe.sa_flags = 0;
    sigact_pipe.sa_flags |= SA_RESTART;
    sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);

    // 安装SIGINT信号处理器
    struct sigaction sigact_int,old_sigact_int;
    sigact_int.sa_handler = sig_int;
    sigemptyset(&sigact_int.sa_mask);
    sigact_int.sa_flags = 0;
    sigact_pipe.sa_flags |= SA_RESTART;
    sigaction(SIGINT,&sigact_int,&old_sigact_int);

    //安装SIG_IGN信号处理器
    struct sigaction sigact_chld, old_sigact_chld;
    sigact_chld.sa_handler = sig_chld;
    sigemptyset(&sigact_chld.sa_mask);
    sigact_chld.sa_flags = 0;
    sigaction(SIGCHLD, &sigact_chld, &old_sigact_chld);


    if(argc != 3){
        printf("Usage: server <IPaddress><Port>");
        exit(1);
    }

    // 父进程启动以前打开 stu_srv_res_p.txt
    char stu_srv_res_p_filename[20];
    sprintf(stu_srv_res_p_filename,"%s","stu_srv_res_p.txt");
    printf("[srv](%d) stu_srv_res_p.txt is opened!\n",getpid());
    stu_srv_res_p = fopen(stu_srv_res_p_filename,"wb");
    if(stu_srv_res_p == NULL){
        printf("stu_srv_res_p.txt open failed!\n");
        exit(1);
    }

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    inet_pton(AF_INET,argv[1],&srv_addr.sin_addr);
    srv_addr.sin_port = htons(atoi(argv[2]));

    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if( listenfd== -1){
        printf("[srv] socket error\n");
        exit(1);
    }

    // 在bind之前打印输出监听情况
    char srv_ip[32];
    memset(buf,0,sizeof(buf));
    inet_ntop(AF_INET, &srv_addr.sin_addr,srv_ip, sizeof(srv_ip));
    sprintf(buf,"[srv](%d) server[%s:%hu] is initializing!\n",getpid(),srv_ip,ntohs(srv_addr.sin_port));
    fputs(buf,stu_srv_res_p);

    if(bind(listenfd,(pSA)&srv_addr,sizeof(struct sockaddr_in)) == -1){
        printf("[srv] bind error\n");
        exit(1);
    }

    if(listen(listenfd,100) == -1){
        printf("[srv] listen error\n");
        exit(1);
    }

    while(!sig_to_exit)
    {
        cli_addr_len = sizeof(cli_addr);
        connfd = accept(listenfd,(pSA)&cli_addr,&cli_addr_len);
        if(connfd == -1 && errno == EINTR && sig_type == SIGINT) break;

        memset(cli_ip,0,sizeof(cli_ip));
        memset(buf,0,sizeof(buf));
        inet_ntop(AF_INET, &cli_addr.sin_addr,cli_ip, sizeof(cli_ip));
        sprintf(buf,"[srv](%d) client[%s:%hu] is accepted!",getpid(),cli_ip,ntohs(cli_addr.sin_port));
        fputs(buf,stu_srv_res_p);

        if(fork() == 0){
            char fn_res[20];
            pid_t child_pid = getpid();
            memset(fn_res,0,sizeof(fn_res));
            sprintf(fn_res,"stu_srv_res_%d.txt",child_pid);
            fp_res = fopen(fn_res,"wb");
            printf("[srv](%d) %s is opened!\n",child_pid,fn_res);


            char buffer_fn_res[MAX_CMD_STR];
            memset(buffer_fn_res,0,sizeof(buffer_fn_res));
            sprintf(buffer_fn_res,"[cli](%d) child process is created!",child_pid);
            fputs(buffer_fn_res,fp_res);

            int PIN = echo_rep(connfd,fp_res,child_pid);
            if(PIN == -1){
                printf("[srv](%d) echo_rep failed!\n",child_pid);
                exit(1);
            }

            char fn_res_new[20];
            memset(fn_res_new,0,sizeof(fn_res_new));
            sprintf(fn_res_new,"stu_srv_res_%d.txt",PIN);
            if(rename(fn_res,fn_res_new) == 0){
                memset(buffer_fn_res,0,sizeof(buffer_fn_res));
                sprintf(buffer_fn_res,"[srv](%d) res file rename done!",child_pid);
                fputs(buffer_fn_res,fp_res);
            }
            else{
                printf("rename error\n");
                exit(1);
            }

            memset(buffer_fn_res,0,sizeof(buffer_fn_res));
            sprintf(buffer_fn_res,"[srv](%d) connfd is closed!",child_pid);
            fputs(buffer_fn_res,fp_res);
            close(connfd);

            memset(buffer_fn_res,0,sizeof(buffer_fn_res));
            sprintf(buffer_fn_res,"[cli](%d) child process is going to exit!",child_pid);
            fputs(buffer_fn_res,fp_res);
            fclose(fp_res);

            printf("[srv](%d) stu_cli_res%d.txt is closed!\n",child_pid,child_pid);
            exit(0);
        }
    }
    close(listenfd);
    memset(buf,0,sizeof(buf));
    sprintf(buf,"[srv](%d) listenfd is closed!",getpid());
    fputs(buf,stu_srv_res_p);

    memset(buf,0,sizeof(buf));
    sprintf(buf,"[srv](%d) parent process is going to exit!",getpid());
    fputs(buf,stu_srv_res_p);

    fclose(stu_srv_res_p);
    printf("[srv])(%d) %s is closed!\n",getpid(),stu_srv_res_p_filename);
    return 0;
}