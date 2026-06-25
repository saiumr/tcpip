// OverlappedEchoServ_APC_win.c
// 重叠 I/O + Completion Routine（APC 回调）
// 单线程、异步、支持多客户端 Echo 服务器
// 使用 Completion Routine 通知 I/O 完成

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <winsock2.h>

#define BUF_SIZE 100
#define MAX_CLIENTS 64

void ErrorHandling(char* message);

// 每个客户端的状态
typedef struct _CLIENT {
    SOCKET sock;
    WSAOVERLAPPED overlapped;
    char buf[BUF_SIZE];
    WSABUF dataBuf;
    DWORD recvBytes;
    DWORD flags;
    BOOL valid;
} CLIENT;

CLIENT clients[MAX_CLIENTS];
int clientCount = 0;

// 声明完成例程（必须在 WSARecv 调用之前声明）
void CALLBACK CompletionRoutine(DWORD dwError, DWORD cbTransferred,
                                LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

// 添加新客户端，投递第一个 WSARecv
void AddClient(SOCKET sock) {
    if (clientCount >= MAX_CLIENTS) {
        printf("Max clients reached, reject.\n");
        closesocket(sock);
        return;
    }

    CLIENT* pClient = &clients[clientCount];
    pClient->sock = sock;
    pClient->recvBytes = 0;
    pClient->flags = 0;
    pClient->valid = TRUE;

    memset(&pClient->overlapped, 0, sizeof(WSAOVERLAPPED));
    // 注意：使用 Completion Routine 时，OVERLAPPED::hEvent 必须为 NULL
    pClient->overlapped.hEvent = NULL;

    pClient->dataBuf.buf = pClient->buf;
    pClient->dataBuf.len = BUF_SIZE;

    // 投递异步接收，第七个参数传入完成例程
    int ret = WSARecv(sock, &pClient->dataBuf, 1, &pClient->recvBytes,
                      &pClient->flags, &pClient->overlapped,
                      CompletionRoutine);   // <--- 关键：传入回调函数

    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            printf("WSARecv() failed for new client, error %d\n", err);
            closesocket(sock);
            pClient->valid = FALSE;
            return;
        }
    }

    clientCount++;
    printf("New client %d connected, total clients: %d\n", sock, clientCount);
}

// 完成例程（APC 回调）
// 当 I/O 完成时，系统会在线程处于可警告等待状态时调用此函数
void CALLBACK CompletionRoutine(DWORD dwError, DWORD cbTransferred,
                                LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags) {
    // 根据 OVERLAPPED 地址找到对应的客户端
    // 这里我们通过遍历 clients 数组来匹配（实际项目可优化为从 OVERLAPPED 反推）
    CLIENT* pClient = NULL;
    for (int i = 0; i < clientCount; i++) {
        if (&clients[i].overlapped == lpOverlapped && clients[i].valid) {
            pClient = &clients[i];
            break;
        }
    }
    if (pClient == NULL) return;

    // 处理错误或连接关闭
    if (dwError != 0 || cbTransferred == 0) {
        printf("Client %d disconnected (error: %d, transferred: %d)\n",
               pClient->sock, dwError, cbTransferred);
        closesocket(pClient->sock);
        pClient->valid = FALSE;
        // 注意：这里不能直接修改 clientCount 或移动数组，因为 APC 在循环中调用
        // 我们通过 valid 标记惰性删除，主循环中会清理
        return;
    }

    // 成功收到数据，直接在此处理（无需主循环介入）
    printf("Received %d bytes from %d: %.*s\n", cbTransferred, pClient->sock,
           cbTransferred, pClient->buf);

    // 回显（同步发送）
    send(pClient->sock, pClient->buf, cbTransferred, 0);

    // 重新投递下一个 WSARecv（在回调中直接发起下一次投递）
    memset(&pClient->overlapped, 0, sizeof(WSAOVERLAPPED));
    pClient->overlapped.hEvent = NULL;
    pClient->dataBuf.buf = pClient->buf;
    pClient->dataBuf.len = BUF_SIZE;
    pClient->recvBytes = 0;
    pClient->flags = 0;

    int ret = WSARecv(pClient->sock, &pClient->dataBuf, 1,
                      &pClient->recvBytes, &pClient->flags,
                      &pClient->overlapped, CompletionRoutine);

    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            printf("WSARecv() failed for %d, error %d\n", pClient->sock, err);
            closesocket(pClient->sock);
            pClient->valid = FALSE;
        }
    }
}

// 清理无效客户端（由主循环定期调用）
void CleanupClients() {
    for (int i = 0; i < clientCount; i++) {
        if (!clients[i].valid) {
            // 用最后一个覆盖当前
            clients[i] = clients[clientCount - 1];
            clientCount--;
            i--;  // 重新检查当前位置
        }
    }
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET hServSock;
    SOCKADDR_IN servAddr;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    // 创建重叠 I/O 监听 socket
    hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (hServSock == INVALID_SOCKET)
        ErrorHandling("WSASocket() error!");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(atoi(argv[1]));

    if (bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
        ErrorHandling("bind() error!");

    if (listen(hServSock, 5) == SOCKET_ERROR)
        ErrorHandling("listen() error!");

    printf("Echo server (APC completion routine) started on port %s\n", argv[1]);

    // 主循环：使用可警告等待 + 非阻塞 accept
    while (1) {
        // 1. 清理无效客户端（惰性删除）
        CleanupClients();

        // 2. 接受新连接（非阻塞）
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(hServSock, &readSet);
        struct timeval tv = {0, 0};
        if (select(0, &readSet, NULL, NULL, &tv) > 0) {
            SOCKADDR_IN clntAddr;
            int addrLen = sizeof(clntAddr);
            SOCKET hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);
            if (hClntSock != INVALID_SOCKET) {
                AddClient(hClntSock);
            }
        }

        // 3. 进入可警告等待状态，让 APC 回调有机会执行
        //    SleepEx(0, TRUE) 不会阻塞，但会立即检查 APC 队列并执行
        //    如果有 APC 等待，执行回调；如果没有，立即返回。
        //    我们用一个短超时，既能让 APC 执行，又不会让 CPU 空转太久。
        SleepEx(100, TRUE);   // 等待 100ms，TRUE 表示可被 APC 唤醒
    }

    WSACleanup();
    return 0;
}

void ErrorHandling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
