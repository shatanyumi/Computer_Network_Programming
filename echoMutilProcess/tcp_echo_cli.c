#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_CMD_STR 124

typedef struct sockaddr* pSA;

typedef struct header_st{
    int PIN;
    int LEN;
}header;

typedef struct PDU{
    header HEADER;
    char BUF[MAX_CMD_STR+1];
}pdu;

// 业务逻辑处理函数
int echo_rqt(int sockfd,int pin,pid_t PID,FILE *fnres) {
    char buf[MAX_CMD_STR+1];
    char td_name[50];
    pdu echo_rqt_pdu,echo_rep_pdu;

    sprintf(td_name,"td%d.txt",pin);
    FILE *fp = fopen(td_name,"r");
    if(fp == NULL){
        printf("[cli](%d) open %s failed!\n",PID,td_name);
        exit(1);
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if(strncmp(buf,"exit",4) == 0) return 0;
        int len = strnlen(buf,MAX_CMD_STR);
        buf[len] = '\0';
        printf("%s\n",buf);
        // 构建pdu
        echo_rqt_pdu.HEADER.PIN = pin;
        echo_rqt_pdu.HEADER.LEN = len;
        strcpy(buf,echo_rqt_pdu.BUF);

        if(write(sockfd,&echo_rqt_pdu.HEADER.PIN,sizeof(int)) == -1){
            printf("write echo_rqt_pdu.HEADER.PIN error \n");
            exit(1);
        }
        if(write(sockfd,&echo_rqt_pdu.HEADER.LEN,sizeof(int)) == -1){
            printf("write echo_rqt_pdu.HEADER.LEN error \n");
            exit(1);
        }
        if(write(sockfd,&echo_rqt_pdu.BUF,sizeof(char)*echo_rqt_pdu.HEADER.LEN) == -1){
            printf("write echo_rqt_pdu.BUF error \n");
            exit(1);
        }

        if(read(sockfd,&echo_rep_pdu.HEADER.PIN,sizeof(int)) == -1){
            printf("read echo_rep_pdu.HEADER.PIN error \n");
            exit(1);
        }

        if(read(sockfd,&echo_rep_pdu.HEADER.LEN,sizeof(int)) == -1){
            printf("read echo_rep_pdu.HEADER.LEN error \n");
            exit(1);
        }

        if(read(sockfd,&echo_rep_pdu.BUF,sizeof(char)*echo_rep_pdu.HEADER.LEN) == -1){
            printf("write echo_rep_pdu.HEADER.PIN error \n");
            exit(1);
        }

        char res[MAX_CMD_STR+124];
        sprintf(res,"[echo_rep](%d) %s",PID,echo_rep_pdu.BUF);
        fputs(res,fnres);
    }
    fclose(fp);
    return 0;
}

int main(int argc,char *argv[])
{
    struct sockaddr_in srv_addr;
    int connfd;
    //最大并发数目
    int mutl_num;

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
    char buf[MAX_CMD_STR+124];

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
        char file_name[50];
        sprintf(file_name,"stu_cli_res_%d.txt",pin);
        printf("[cli](%d) %s is created!\n",PID,file_name);

        FILE *fnres = fopen(file_name,"wb");
        if(fnres == NULL){
            printf("fopen file failed\n");
            exit(1);
        }
        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) child process %d is created!\n",PID,pin);
        fputs(buf,fnres);

        do{
            int res = connect(connfd,(pSA)&srv_addr,sizeof(srv_addr));
            if(res == 0){
                memset(buf,0,sizeof(buf));
                sprintf(buf,"[cli](%d) server[%s:%s] is connected!\n",PID,argv[1],argv[2]);
                fputs(buf,fnres);
                if (echo_rqt(connfd,pin,PID,fnres) == 0){
                    break;
                }
            }
            else if(res == -1 && errno == EINTR){
                continue;
            }
        }while(1);

        close(connfd);
        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) connfd is closed!\n",PID);
        fputs(buf,fnres);

        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) child process is going to exit!\n",PID);
        fputs(buf,fnres);
        
        fclose(fnres);
        printf("[cli](%d) %s is closed!\n",PID,file_name);
        exit(0);
    }
    else{
        pid_t PID = getpid();
        //printf("父进程 %d\n",pin);
        char file_name[50];
        sprintf(file_name,"stu_cli_res_%d.txt",pin);
        printf("[cli](%d) %s is created!\n",PID,file_name);

        FILE *fnres = fopen(file_name,"wb");
        if(fnres == NULL){
            printf("fopen file failed\n");
            exit(1);
        }
        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) child process %d is created!\n",PID,pin);
        fputs(buf,fnres);

        do{
            int res = connect(connfd,(pSA)&srv_addr,sizeof(srv_addr));
            if(res == 0){
                memset(buf,0,sizeof(buf));
                sprintf(buf,"[cli](%d) server[%s:%s] is connected!\n",PID,argv[1],argv[2]);
                fputs(buf,fnres);
                if (echo_rqt(connfd,pin,PID,fnres) == 0){
                    break;
                }
            }
            else if(res == -1 && errno == EINTR){
                continue;
            }
        }while(1);

        close(connfd);
        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) connfd is closed!\n",PID);
        fputs(buf,fnres);

        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%d) parent process is going to exit!\n",PID);
        fputs(buf,fnres);
        fclose(fnres);
        printf("[cli](%d) %s is closed!\n",PID,file_name);
        exit(0);
    }
    printf("[cli] client is exiting!\n");
    return 0;
}


