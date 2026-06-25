// echo_selectserv_win.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define BUF_SIZE 1024

void ErrorHandling(char *message);

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    SOCKET hServSock, hClntSock;
    SOCKADDR_IN servAdr, clntAdr;
    TIMEVAL timeout;
    fd_set reads, cpyReads;

    int adrSz;
    int strLen, fdNum, i;
    char buf[BUF_SIZE];

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // Windows 套接字初始化
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    // 创建服务端套接字
    hServSock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAdr.sin_port = htons(atoi(argv[1]));

    // 绑定+监听
    if (bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
        ErrorHandling("bind() error");
    if (listen(hServSock, 5) == SOCKET_ERROR)
        ErrorHandling("listen() error");

    // 初始化 select 监视集合
    FD_ZERO(&reads);
    FD_SET(hServSock, &reads);

    while (1)
    {
        // 备份集合（和Linux逻辑一致）
        cpyReads = reads;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // Windows select：第一个参数固定填0
        fdNum = select(0, &cpyReads, 0, 0, &timeout);
        if (fdNum == SOCKET_ERROR)
            break;
        if (fdNum == 0)
            continue;

        // Windows 遍历方式：遍历 fd_count（核心差异）
        for (i = 0; i < reads.fd_count; i++)
        {
            if (FD_ISSET(reads.fd_array[i], &cpyReads))
            {
                // 1. 服务端套接字：新客户端连接
                if (reads.fd_array[i] == hServSock)
                {
                    adrSz = sizeof(clntAdr);
                    hClntSock = accept(hServSock, (SOCKADDR*)&clntAdr, &adrSz);
                    FD_SET(hClntSock, &reads);
                    printf("connected client: %d\n", hClntSock);
                }
                // 2. 客户端套接字：读取数据并回声
                else
                {
                    strLen = recv(reads.fd_array[i], buf, BUF_SIZE - 1, 0);
                    // 客户端断开连接
                    if (strLen == 0)
                    {
                        // 从集合删除+关闭套接字
                        FD_CLR(reads.fd_array[i], &reads);
                        closesocket(reads.fd_array[i]);
                        printf("closed client: %d\n", reads.fd_array[i]);
                    }
                    else
                    {
                        // 回声回传
                        send(reads.fd_array[i], buf, strLen, 0);
                    }
                }
            }
        }
    }

    // 资源释放
    closesocket(hServSock);
    WSACleanup();
    return 0;
}

// 错误处理函数
void ErrorHandling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

