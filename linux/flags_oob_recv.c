// flags_oob_recv.c
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#define BUF_SIZE 30

void error_handling(char *message);
void urg_handler(int signo);

// 全局变量：信号处理函数需要访问
int acpt_sock;
int recv_sock;

int main(int argc, char *argv[])
{
    struct sockaddr_in recv_adr, serv_adr;
    int str_len, state;
    socklen_t serv_adr_sz;
    struct sigaction act;
    char buf[BUF_SIZE];

    if ( argc!=2 ) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 注册 SIGURG 信号处理函数
    act.sa_handler = urg_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    acpt_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&recv_adr, 0, sizeof(recv_adr));
    recv_adr.sin_family = AF_INET;
    recv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    recv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(acpt_sock, (struct sockaddr*)&recv_adr, sizeof(recv_adr))==-1)
        error_handling("bind() error");
    listen(acpt_sock, 5);

    serv_adr_sz = sizeof(serv_adr);
    recv_sock = accept(acpt_sock, (struct sockaddr*)&serv_adr, &serv_adr_sz);

    // 设置套接字所有权为当前进程（下文解释）
    fcntl(recv_sock, F_SETOWN, getpid());
    // 注册紧急消息信号
    state = sigaction(SIGURG, &act, 0);

    // 循环接收普通数据
    while((str_len = recv(recv_sock, buf, sizeof(buf), 0)) != 0)
    {
        if(str_len == -1)
            continue;
        buf[str_len] = 0;
        puts(buf);
    }

    close(recv_sock);
    close(acpt_sock);
    return 0;
}

// 紧急消息处理函数（SIGURG信号触发）
void urg_handler(int signo)
{
    int str_len;
    char buf[BUF_SIZE];
    // 接收带外数据
    str_len = recv(recv_sock, buf, sizeof(buf) - 1, MSG_OOB);
    buf[str_len] = 0;
    printf("Urgent message: %s\n",buf);
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
