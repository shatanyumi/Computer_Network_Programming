#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include<signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include<sys/stat.h>
#include <fcntl.h>

#define MAX_CMD_STR 100

typedef struct sockaddr* pSA;

void sig_chld(int signo){
    pid_t pid;
    int stat;
    while((pid=waitpid(-1,&stat,WNOHANG))>0);
    return;
}

typedef struct PDU{
    int PIN;
    int LEN;
    char BUF[MAX_CMD_STR+1];
}pdu;

// 业务逻辑处理函数
int echo_rqt(int sockfd,int pin,pid_t PID) {
    char buf[MAX_CMD_STR+1];
    char file_name[MAX_CMD_STR];
    char td_name[MAX_CMD_STR];
    pdu echo_rqt_pdu,echo_rep_pdu;

    sprintf(file_name,"stu_cli_res_%d.txt",pin);
    sprintf(td_name,"td%d.txt",pin);

    FILE *fp = fopen(td_name,"r");
    FILE *fres = fopen(file_name,"w");

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if(strncmp(buf,"exit",4) == 0) return 0;
        int len = strnlen(buf,MAX_CMD_STR);
        buf[len] = '\0';
        // 构建pdu
        echo_rqt_pdu.PIN = pin;
        echo_rqt_pdu.LEN = len;
        strcpy(echo_rqt_pdu.BUF,buf);

        if(write(sockfd,&echo_rqt_pdu,sizeof(pdu)) == -1){
            printf("write echo_rqt_pdu error \n");
            exit(1);
        }

        if(read(sockfd,&echo_rep_pdu,sizeof(pdu)) == -1){
            printf("[cli] read echo_rep_pdu error \n");
            exit(1);
        }

        char res[MAX_CMD_STR+8];
        sprintf(res,"[echo_rep](%d) %s",PID,echo_rep_pdu.BUF);
        fputs(res,fres);
    }
    fclose(fp);
    fclose(fres);
    return 0;
}

int main(int argc,char *argv[])
{

    struct sockaddr_in srv_addr;
    int connfd;
    int mutl_num;

    struct sigaction sigact_chld, old_sigact_chld;
    sigact_chld.sa_handler = sig_chld;
    sigemptyset(&sigact_chld.sa_mask);
    sigact_chld.sa_flags = 0;
    sigaction(SIGCHLD, &sigact_chld, &old_sigact_chld);

    if(argc != 4){
        printf("Usage:%s <IP> <PORT> <mutl_num>\n", argv[0]);
        return 0;
    }

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET,argv[1],&srv_addr.sin_addr);

    mutl_num = atoi(argv[3]);

    connfd = socket(AF_INET,SOCK_STREAM,0);
    if(connfd == -1){
        printf("[cli] socket error\n");
        exit(1);
    }

    pid_t pid;
    int pin;
    for(pin= mutl_num-1;pin>0;pin--){
        pid = fork();
        if(pid == -1|| pid == 0) break;
    }

    if(pid<0){
        printf("process error!\n");
        exit(1);
    }
    else if(pid == 0){
        pid_t PID = getpid();
        //printf("子进程 %d\n",pin);
        char file_name[MAX_CMD_STR];
        sprintf(file_name,"stu_cli_res_%d.txt",pin);
        printf("[cli](%d) %s is created!\n",PID,file_name);

        FILE *fnres = fopen(file_name,"w");
        if(fnres == NULL){
            printf("fopen file failed\n");
            exit(1);
        }
        char buf[MAX_CMD_STR];
        sprintf(buf,"[cli](%d) child process %d is created!",PID,pin);
        fputs(buf,fnres);
        fclose(fnres);

        do{
            int res = connect(connfd,(pSA)&srv_addr,sizeof(srv_addr));
            if(res == 0){
                fnres = fopen(file_name,"w");
                sprintf(buf,"[cli](%d) server[%s:%s] is connected!\n",PID,argv[1],argv[2]);
                fputs(buf,fnres);
                fclose(fnres);
                if (echo_rqt(connfd,pin,PID) == 0){
                    break;
                }
            }
            else if(res == -1 && errno == EINTR){
                continue;
            }
        }while(1);
        close(connfd);

        fnres = fopen(file_name,"w");
        sprintf(buf,"[cli](%d) connfd is closed!\n",PID);
        fputs(buf,fnres);
        sprintf(buf,"[cli](%d) child process is going to exit!\n",PID);
        fputs(buf,fnres);
        fclose(fnres);

        printf("[cli](%d) %s is closed!\n",PID,file_name);
        exit(0);
    }
    else{
        //printf("父进程 %d\n",pin);
        pid_t PID = getpid();
        char file_name[MAX_CMD_STR];
        sprintf(file_name,"stu_cli_res_%d.txt",pin);
        printf("[cli](%d) %s is created!\n",PID,file_name);

        FILE *fnres = fopen(file_name,"w");
        if(fnres == NULL){
            printf("fopen file failed\n");
            exit(1);
        }
        char buf[MAX_CMD_STR];
        sprintf(buf,"[cli](%d) child process %d is created!",PID,pin);
        fputs(buf,fnres);
        fclose(fnres);

        do{
            int res = connect(connfd,(pSA)&srv_addr,sizeof(srv_addr));
            if(res == 0){
                fnres = fopen(file_name,"w");
                sprintf(buf,"[cli](%d) server[%s:%s] is connected!\n",PID,argv[1],argv[2]);
                fputs(buf,fnres);
                fclose(fnres);
                if (echo_rqt(connfd,pin,PID) == 0){
                    break;
                }
            }
            else if(res == -1 && errno == EINTR){
                continue;
            }
        }while(1);
        close(connfd);

        fnres = fopen(file_name,"w");
        sprintf(buf,"[cli](%d) connfd is closed!\n",PID);
        fputs(buf,fnres);
        sprintf(buf,"[cli](%d) parent process is going to exit!\n",PID);
        fputs(buf,fnres);
        fclose(fnres);

        printf("[cli](%d) %s is closed!\n",PID,file_name);
        exit(0);
    }
    printf("[cli] client is exiting!\n");
    return 0;
}


