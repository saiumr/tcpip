#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUF_SIZE 100

void error_handling(char *buf);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    struct timeval timeout;
    fd_set reads, cpy_reads;    // 原始集合 + 备份集合（select核心）
    socklen_t adr_sz;
    int fd_max, str_len, fd_num, i;
    char buf[BUF_SIZE];

    // 1. 参数检查
    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // 2. 创建服务端套接字 + 绑定 + 监听
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // 3. 初始化 select 监视集合
    FD_ZERO(&reads);
    FD_SET(serv_sock, &reads);  // 注册服务端socket（监视连接事件）
    fd_max = serv_sock;         // 记录当前最大文件描述符（select第一个参数用）

    // 4. select 主循环（核心！）
    while (1)
    {
        // ========== select 固定流程：备份原始集合 ==========
        cpy_reads = reads;
        // 设置超时：5秒0微秒
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // ========== 调用 select 函数 ==========
        // 参数1：最大fd+1； 参数2：读集合； 超时时间
        fd_num = select(fd_max + 1, &cpy_reads, NULL, NULL, &timeout);
        if (fd_num == -1)  // select 出错
            break;
        if (fd_num == 0)   // 超时，无事件
            continue;

        // ========== 遍历所有fd，查找就绪的fd ==========
        for (i = 0; i < fd_max + 1; i++)
        {
            // 判断当前fd是否有事件
            if (FD_ISSET(i, &cpy_reads))
            {
                // 情况1：服务端fd就绪 → 有新客户端连接
                if (i == serv_sock)
                {
                    adr_sz = sizeof(clnt_adr);
                    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
                    
                    // 新客户端socket 注册到 select 监视集合
                    FD_SET(clnt_sock, &reads);
                    // 更新最大fd
                    if (fd_max < clnt_sock)
                        fd_max = clnt_sock;
                    printf("connected client: %d\n", clnt_sock);
                }
                // 情况2：客户端fd就绪 → 有数据可读
                else
                {
                    str_len = read(i, buf, BUF_SIZE);
                    // 客户端断开连接
                    if (str_len == 0)
                    {
                        FD_CLR(i, &reads);  // 从监视集合中删除
                        close(i);          // 关闭套接字
                        printf("closed client: %d\n", i);
                    }
                    // 回声：将数据回写给客户端
                    else
                    {
                        write(i, buf, str_len);
                    }
                }
            }
        }
    }

    close(serv_sock);  // 关闭服务端socket
    return 0;
}

// 错误处理函数
void error_handling(char *buf)
{
    fputs(buf, stderr);
    fputc('\n', stderr);
    exit(1);
}
