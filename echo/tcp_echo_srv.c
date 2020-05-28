/*
 * 单进程TCP服务器部分
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define MAX_CMD_STR 100

typedef struct sockaddr *pSA ;
// 全局变量sig_to_exit，用于指示服务器程序是否退出；
int sig_to_exit = 0;
int sig_type = 0;

// SIGINT信号处理函数；
void sig_int(int signo) {
    // 记录本次系统信号编号；
    sig_type = signo;
    // 设置全局变量，以提示服务器程序释放资源；
    if(signo == SIGINT)
        sig_to_exit = 1;
}

// SIGPIPE信号处理函数；
void sig_pipe(int signo) {
    // 记录本次系统信号编号；
    sig_type = signo;
    printf("[srv] SIGPIPE is coming!\n");
}

// 业务逻辑处理函数；
void echo_rep(int sockfd) {
    // 定义数据长度变量len，以及读取结果变量res；
    int len = 0,res = 0;
    // 定义缓存指针变量buf;
    char *buf;
    do{
        // (1)读取数据长度： res = read(x,x,x);
        res = read(sockfd,&len,sizeof(int));
        // 以下代码紧跟read();
        if(res < 0){
            printf("[srv] read len return %d and errno is %d\n", res, errno);
            if(errno == EINTR){
                if(sig_type == SIGINT)
                    return;//但凡收到SIGINT，指示服务器结束
                continue;//若是其他信号中断，则重新执行读取
            }
            return;
        }
        // 若read返回0（并非指定读0字节返回零），return；
        if(res == 0 ) return;
        // (2) 按长读取数据；
        // 采用malloc，根据数据长度分配buf；
        buf = (char *)malloc(sizeof(char)*len);
        do{
            // 按长读取数据： res = read(x,x,x);
            res = read(sockfd,buf,sizeof(char)*len);
            // 以下代码紧跟read();
            if(res < 0){
                printf("[srv] read data return %d and errno is %d\n", res, errno);
                if(errno == EINTR){
                    if(sig_type == SIGINT) {
                        free(buf);
                        return;//但凡收到SIGINT，指示服务器结束
                    }
                    continue;   //若是其他信号中断，则重新执行读取
                }
                free(buf);
                return;
            }
            // 若read返回0（并非指定读0字节返回零），return；
            if (res == 0){
                return;
            }
            break;
        }while(1);

        // 本轮数据长度以及数据本身的读取结束：
        // 按题设要求打印接收到的[echo_rqt]信息；
        printf("[echo_rqt] %s\n",buf);

        // 回写客户端[echo_rep]信息；根据读写边界定义，同样需先发长度，再发数据：res = write(x,x,x);res = write(x,x,x);
        if(write(sockfd,&len,sizeof(int)) == -1){
            printf("write len error \n");
            exit(1);
        }

        if(write(sockfd,buf,sizeof(char)*len) ==-1){
            printf("write buf error\n");
            exit(1);
        }
        // 发送结束，释放buf；
        free(buf);
    }while(1);
}

int main(int argc,char *argv[])
{
    int res = 0;
    // 安装SIGPIPE信号处理器
    struct sigaction sigact_pipe, old_sigact_pipe;
    sigact_pipe.sa_handler = sig_pipe;
    sigemptyset(&sigact_pipe.sa_mask);
    sigact_pipe.sa_flags = 0;
    // 通过SA_RESTART设置受影响的慢调用在中断结束后立刻重启：
    sigact_pipe.sa_flags |= SA_RESTART;
    sigaction(SIGPIPE, &sigact_pipe, &old_sigact_pipe);

    // 安装SIGINT信号处理器
    struct sigaction sigact_int,old_sigact_int;
    sigact_int.sa_handler = sig_int;
    sigemptyset(&sigact_int.sa_mask);
    sigact_int.sa_flags = 0;
    sigact_pipe.sa_flags |= SA_RESTART;
    sigaction(SIGINT,&sigact_int,&old_sigact_int);


    // 定义服务器Socket地址srv_addr，以及客户端Socket地址cli_addr；
    struct sockaddr_in srv_addr,cli_addr;

    // 定义客户端Socket地址长度cli_addr_len（类型为socklen_t）；
    socklen_t cli_addr_len;

    // 定义Socket监听描述符listenfd，以及Socket连接描述符connfd；
    int listenfd,connfd;

    // 初始化服务器Socket地址srv_addr，其中会用到argv[1]、argv[2]
    if(argc != 3){
        printf("Usage: server <IPaddress><Port>");
        exit(1);
    }
    bzero(&srv_addr,sizeof(srv_addr));

    /* IP地址转换推荐使用inet_pton()；端口地址转换推荐使用atoi(); */
    srv_addr.sin_family = AF_INET;
    inet_pton(AF_INET,argv[1],&srv_addr.sin_addr);
    srv_addr.sin_port = htons(atoi(argv[2]));

    //按题设要求打印服务器端地址server[ip:port]，推荐使用inet_ntop();
    char srv_ip[32];
    inet_ntop(AF_INET, &srv_addr.sin_addr,srv_ip, sizeof(srv_ip));
    printf("[srv] server[%s:%hu] is initializing!\n",srv_ip,ntohs(srv_addr.sin_port));

    // 获取Socket监听描述符: listenfd = socket(x,x,x);
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if( listenfd== -1){
        printf("[srv] socket error\n");
        exit(1);
    }

    // 绑定服务器Socket地址: res = bind(x,x,x);
    if(bind(listenfd,(pSA)&srv_addr,sizeof(struct sockaddr_in)) == -1){
        printf("[srv] bind error\n");
        exit(1);
    }

    // 开启服务监听: res = listen(x,x);
    if(listen(listenfd,100) == -1){
        printf("[srv] listen error\n");
        exit(1);
    }

    // 开启accpet()主循环，直至sig_to_exit指示服务器提出；
    while(!sig_to_exit)
    {
        // TODO 获取cli_addr长度，执行accept()：connfd = accept(x,x,x);
        cli_addr_len = sizeof(cli_addr);
        connfd = accept(listenfd,(pSA)&cli_addr,&cli_addr_len);
        // 以下代码紧跟accept()，用于判断accpet()是否因SIG_INT信号退出（本案例中只关心SIGINT）；
        // 也可以不做此判断，直接执行 connfd<0 时continue，因为此时sig_to_exit已经指明需要退出accept()主循环，两种方式二选一即可。

        // 若上述if判断不成立且connfd<0，则重启accept();
        if (connfd < 0)
            continue;

        // 按题设要求打印客户端端地址client[ip:port]，推荐使用inet_ntop();
        char cli_ip[32];
        inet_ntop(AF_INET, &cli_addr.sin_addr,cli_ip, sizeof(cli_ip));
        printf("[srv] client[%s:%hu] is initializing!\n",cli_ip,ntohs(cli_addr.sin_port));
        // 执行业务处理函数echo_rep()，进入业务处理循环;
        echo_rep(connfd);
        // 业务函数退出，关闭connfd;
        close(connfd);
        printf("[srv] connfd is closed!\n");
    }
    // TODO accpet()主循环结束，关闭lstenfd;
    close(listenfd);
    printf("[srv] listenfd is closed!\n");
    printf("[srv] server is exiting\n");
    return 0;
}


