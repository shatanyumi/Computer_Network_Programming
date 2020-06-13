#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syscall.h>
#include <pthread.h>

#define MAX_CMD_STR 124

typedef struct sockaddr *pSA ;
int sig_to_exit = 0;
int sig_type = 0;
FILE *fp_res;
long tid;
int pin;

typedef struct PDU{
    int PIN;
    int LEN;
    char BUF[MAX_CMD_STR+1];
}pdu;

void sig_int(int signo) {
    sig_type = signo;
    if(signo == SIGINT)
        sig_to_exit = 1;
}

void sig_pipe(int signo) {
    sig_type = signo;
    printf("[srv](%ld) SIGPIPE is coming!\n",tid);
}

void *echo_rep(void *arg) {

    int sockfd = *(int*)arg;
    char res_filename[MAX_CMD_STR];
    memset(res_filename,0,sizeof(res_filename));
    sprintf(res_filename,"stu_srv_res_%ld.txt",tid);
    FILE *fn_res = fopen(res_filename,"wb");
    if(fn_res == NULL){
        printf("fn_res open failed!\n");
        exit(1);
    }
    printf("[srv](%ld) %s is opened!\n",tid,res_filename);

    char buffer[MAX_CMD_STR];
    pdu echo_rev_pdu;
    if(read(sockfd,&echo_rev_pdu,sizeof(pdu))==-1){
        printf("[srv] echo_rep read error\n");
        exit(1);
    }

    memset(buffer,0,sizeof(buffer));
    sprintf(buffer,"[echo_rqt](%ld) %s",tid,echo_rev_pdu.BUF);
    pin = echo_rev_pdu.PIN;
    fputs(buffer,fn_res);

    if(write(sockfd,&echo_rev_pdu,sizeof(pdu))==-1){
        printf("[srv] echo_rep write error\n");
        exit(1);
    }

    memset(buffer,0,sizeof(buffer));
    sprintf(buffer,"[srv](%ld) connfd is closed!",tid);
    fputs(buffer,fn_res);
    close(sockfd);

    printf("[srv](%ld) %s is closed!\n",tid,res_filename);
    fclose(fn_res);

    char res_filename_new[20];
    memset(res_filename_new,0,sizeof(res_filename_new));
    sprintf(res_filename_new,"stu_srv_res_%d.txt",pin);
    if(rename(res_filename,res_filename_new) == 0){
        printf("[srv](%ld) res file rename done!",tid);
    }
    else{
        printf("rename error\n");
        exit(1);
    }

}

int main(int argc,char *argv[])
{
    char cli_ip[32];
    struct sockaddr_in srv_addr,cli_addr;
    socklen_t cli_addr_len;
    int listenfd,connfd;
    char buf[MAX_CMD_STR];
    char file_name[MAX_CMD_STR];

    struct sigaction sigact_pipe, old_sigact_pipe;
    sigact_pipe.sa_handler = sig_pipe;
    sigemptyset(&sigact_pipe.sa_mask);
    sigact_pipe.sa_flags = 0;
    sigact_pipe.sa_flags |= SA_RESTART;
    sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);

    struct sigaction sigact_int,old_sigact_int;
    sigact_int.sa_handler = sig_int;
    sigemptyset(&sigact_int.sa_mask);
    sigact_int.sa_flags = 0;
    sigact_pipe.sa_flags |= SA_RESTART;
    sigaction(SIGINT,&sigact_int,&old_sigact_int);


    if(argc != 3){
        printf("Usage: server <IPaddress><Port>");
        exit(1);
    }
    tid = syscall(SYS_gettid);
    sprintf(file_name,"%s","stu_srv_res_p.txt");
    printf("[srv](%ld) stu_srv_res_p.txt is opened!\n",tid);
    fp_res = fopen(file_name,"wb");
    if(fp_res==NULL){
        printf("%s open failed!",file_name);
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
    inet_ntop(AF_INET, &srv_addr.sin_addr,srv_ip, sizeof(srv_ip));
    sprintf(buf,"[srv](%ld) server[%s:%hu] is initializing!\n",tid,srv_ip,ntohs(srv_addr.sin_port));
    fputs(buf,fp_res);

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
        if (connfd < 0) continue;

        pthread_t socket_rep;
        pthread_create(&socket_rep,NULL,echo_rep,&connfd);

        memset(cli_ip,0,sizeof(cli_ip));
        memset(buf,0,sizeof(buf));
        inet_ntop(AF_INET, &cli_addr.sin_addr,cli_ip, sizeof(cli_ip));
        sprintf(buf,"[srv](%ld) client[%s:%hu] is accepted!",tid,cli_ip,ntohs(cli_addr.sin_port));
        fputs(buf,fp_res);
        pthread_join(socket_rep,NULL);
    }

    close(listenfd);
    memset(buf,0,sizeof(buf));
    sprintf(buf,"[srv](%ld) listenfd is closed!",tid);
    fputs(buf,fp_res);

    fclose(fp_res);
    printf("[srv])(%ld) %s is closed!\n",tid,file_name);
    return 0;
}