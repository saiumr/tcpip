// echo_storeserv.c
// 将接收到的消息写入文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 30

void error_handling(char *message);
void read_childproc(int sig);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int fds[2];
    pid_t pid;
    struct sigaction act;
    socklen_t adr_sz;
    int str_len;
    char buf[BUF_SIZE];

    if(argc!=2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 注册SIGCHLD信号处理函数，自动回收子进程
    act.sa_handler = read_childproc;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, 0);

    // 创建监听套接字
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    // 绑定+监听
    if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if(listen(serv_sock, 2) == -1)
        error_handling("listen() error");

    pipe(fds);
    pid = fork();

    if (pid == 0) {
        FILE* fp = fopen("echo_storage.txt", "wt");
        char msgbuf[BUF_SIZE];
        int i, len;

        if (fp == NULL) error_handling("fopen error!");

        // 写入任意数目数据（需要实时写入）
        // while (1) {
        //   len = read(fds[0], msgbuf, BUF_SIZE);
        //   if (len <= 0) break;
        //   fwrite((void*)msgbuf, 1, len, fp);  // 只写内存，不写磁盘
        //   fflush(fp);  // 立即写入磁盘，不缓存
        // }

        // 缓存三条数据
        for(i = 0; i < 3; ++i) {
            len = read(fds[0], msgbuf, BUF_SIZE);
            fwrite((void*)msgbuf, 1, len, fp);
        }

        fclose(fp);  // 此处执行一次 fflush(fp)
        return 0;
    }

    // 无限循环监听客户端连接
    while(1)
    {
        adr_sz = sizeof(clnt_adr);
        // 接收客户端连接
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
        if(clnt_sock == -1)
            continue;
        else
            puts("new client connected...");

        // 创建子进程
        pid = fork();
        if(pid == -1)
        {
            close(clnt_sock);
            continue;
        }

        // 子进程：负责和客户端通信
        if(pid == 0)
        {
            close(serv_sock);  // 子进程关闭监听套接字文件描述符
            // 回声服务：读取客户端数据并回传
            while((str_len = read(clnt_sock, buf, BUF_SIZE)) != 0) {
                write(clnt_sock, buf, str_len);
                write(fds[1], buf, str_len);
            }

            close(clnt_sock);
            puts("client disconnected...");
            return 0;
        }
        // 父进程：关闭客户端套接字文件描述符，继续监听
        else
        {
            close(clnt_sock);
        }
    }
    close(serv_sock);
    return 0;
}

// 子进程退出信号处理函数：回收僵尸进程
void read_childproc(int sig)
{
    pid_t pid;
    int status;
    pid = waitpid(-1, &status, WNOHANG);
    printf("Removed proc id: %d \n", pid);
}

// 错误处理函数
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
