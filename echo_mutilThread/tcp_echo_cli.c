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

#define MAX_CMD_STR 100

typedef struct sockaddr* pSA;

typedef struct PDU{
    int PIN;
    int LEN;
    char BUF[MAX_CMD_STR+1];
}pdu;

int pin;
FILE *res;
long tid;

void *Write(void *arg){
    pthread_detach(pthread_self());
    int sockfd = *(int*)arg;
    pdu echo_rqt_pdu;
    char buf[MAX_CMD_STR+1];
    char td_file[MAX_CMD_STR];

    sprintf(td_file,"td%d.txt",pin);
    FILE *td = fopen(td_file,"r");
    if(td == NULL){
        printf("td_file open failed!\n");
        exit(1);
    }
    while (fgets(buf, sizeof(buf), td) != NULL) {

        if (strncmp(buf, "exit", 4) == 0){
            shutdown(sockfd,SHUT_WR);
            memset(buf,0,sizeof(buf));
            sprintf(buf,"[cli](%ld) shutdown is called with SHUT_WR!",tid);
            fputs(buf,res);
        }

        int len = strnlen(buf, MAX_CMD_STR);
        buf[len] = '\0';

        echo_rqt_pdu.PIN = pin;
        echo_rqt_pdu.LEN = len;
        strcpy(echo_rqt_pdu.BUF, buf);

        if(write(sockfd,&echo_rqt_pdu,sizeof(pdu)) == -1){
            printf("write echo_rqt_pdu error \n");
            exit(1);
        }

    }
    
    fclose(td);
}

int echo_rqt(int sockfd) {
    char buf[MAX_CMD_STR+1];
    pdu echo_rep_pdu,echo_rqt_pdu;
    pthread_t socket_write;

    if(pthread_create(&socket_write,NULL,Write,&sockfd) != 0){
        printf("pthread_create failed!\n");
        exit(1);
    }

        pthread_join(socket_write,NULL);
        memset(buf,0,sizeof(buf));
        sprintf(buf,"[cli](%ld) pthread_detach is done!",tid);
        fputs(buf,res);


        int rd;
        while(1){
            rd = read(sockfd,&echo_rep_pdu,sizeof(pdu));
            if(rd == 0) break;
            if(rd == -1){
                printf("[cli] read echo_rep_pdu error \n");
                exit(1);
            }
            char res_buf[MAX_CMD_STR+8];
            sprintf(res_buf,"[echo_rep](%ld) %s",tid,echo_rep_pdu.BUF);
            fputs(res_buf,res);
        }

    return 0;
}

int main(int argc,char *argv[])
{
    struct sockaddr_in srv_addr;
    int connfd;
    if(argc != 4){
        printf("Usage:%s <IP> <PORT> <mutl_num>\n", argv[0]);
        return 0;
    }

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET,argv[1],&srv_addr.sin_addr);
    pin = atoi(argv[3]);

    tid = syscall(SYS_gettid);
    char file_name[MAX_CMD_STR];
    sprintf(file_name,"stu_cli_res_%d.txt",pin);
    printf("[cli](%ld) %s is created!\n",tid,file_name);
    FILE *res = fopen(file_name,"wb");
    if(res == NULL){
        printf("stu_cli_res_PIN.txt open failed!\n");
        exit(1);
    }

    connfd = socket(AF_INET,SOCK_STREAM,0);
    if(connfd == -1){
        printf("[cli] socket error\n");
        exit(1);
    }
    char buf[MAX_CMD_STR+1];
    do{
            int result = connect(connfd,(pSA)&srv_addr,sizeof(srv_addr));
            if(result == 0){
                memset(buf,0,sizeof(buf));
                sprintf(buf,"[cli](%ld) server[%s:%s] is connected!",tid,argv[1],argv[2]);
                fputs(buf,res);
                if (echo_rqt(connfd) == 0){
                    break;
                }
            }
            else if(result == -1 && errno == EINTR){
                continue;
            }
        }while(1);

    close(connfd);
    sprintf(buf,"[cli](%ld) connfd is closed!",tid);
    fputs(buf,res);

    sprintf(buf,"[cli](%ld) parent thread is going to exit!",tid);
    fputs(buf,res);
    fclose(res);

    printf("[cli](%ld) %s is closed!\n",tid,file_name);
    return 0;
}


