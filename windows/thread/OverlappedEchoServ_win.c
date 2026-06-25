// OverlappedEchoServ_win.c
// 单线程、异步、支持多客户端的 Echo 服务器（重叠 I/O + 事件通知）
// 使用 select 非阻塞检查监听 socket，主循环同时处理完成事件和新连接

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
    WSAEVENT event;
    char buf[BUF_SIZE];
    WSABUF dataBuf;
    DWORD recvBytes;
    DWORD flags;
    BOOL valid;
} CLIENT;

CLIENT clients[MAX_CLIENTS];
WSAEVENT eventArray[MAX_CLIENTS];
int clientCount = 0;

// 添加新客户端，投递第一个 WSARecv
void AddClient(SOCKET sock) {
    if (clientCount >= MAX_CLIENTS) {
        printf("Max clients reached, reject.\n");
        closesocket(sock);
        return;
    }

    CLIENT* pClient = &clients[clientCount];
    pClient->sock = sock;
    pClient->event = WSACreateEvent();  // manual reset event
    if (pClient->event == WSA_INVALID_EVENT) {
        ErrorHandling("WSACreateEvent() error!");
        closesocket(sock);
        return;
    }

    memset(&pClient->overlapped, 0, sizeof(WSAOVERLAPPED));
    pClient->overlapped.hEvent = pClient->event;
    pClient->dataBuf.buf = pClient->buf;
    pClient->dataBuf.len = BUF_SIZE;
    pClient->recvBytes = 0;
    pClient->flags = 0;
    pClient->valid = TRUE;

    // 投递异步接收
    // 确认重叠I/O完成的两种方法：WSARecv 第六个参数，基于事件对象；第七个参数，基于Completion Routine
    // （这里我们使用第六个参数，基于事件对象确认重叠I/O完成）
    int ret = WSARecv(sock, &pClient->dataBuf, 1, &pClient->recvBytes,
                      &pClient->flags, &pClient->overlapped, NULL);
    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            printf("WSARecv() failed for new client, error %d\n", err);
            closesocket(sock);
            WSACloseEvent(pClient->event);
            pClient->valid = FALSE;
            return;
        }
    }
    // 成功投递，加入事件数组
    eventArray[clientCount] = pClient->event;
    clientCount++;
    printf("New client %d connected, total clients: %d\n", sock, clientCount);
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET hServSock;
    SOCKADDR_IN servAddr;
    int ret;

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

    printf("Echo server started on port %s\n", argv[1]);

    // 主循环
    while (1) {
        // 处理客户端的 I/O 完成事件
        if (clientCount > 0) {
            // 非阻塞检查所有客户端事件（非阻塞）
            DWORD idx = WSAWaitForMultipleEvents(clientCount, eventArray, FALSE, 0, FALSE);
            if (idx != WSA_WAIT_TIMEOUT && idx != WSA_WAIT_FAILED) {
                // 有事件触发，遍历所有事件，处理已触发的
                for (int i = 0; i < clientCount; i++) {
                    // 检查单个事件是否触发（非阻塞）
                    DWORD ret = WSAWaitForMultipleEvents(1, &eventArray[i], FALSE, 0, FALSE);
                    if (ret != WSA_WAIT_EVENT_0)
                        continue;   // 未触发

                    CLIENT* pClient = &clients[i];
                    if (!pClient->valid) continue;

                    // 获取结果
                    DWORD transferred;
                    DWORD flags = 0;
                    BOOL success = WSAGetOverlappedResult(pClient->sock, &pClient->overlapped,
                                                          &transferred, FALSE, &flags);
                    // 先重置事件（无论成功与否，事件都应该重置，否则下次还会被触发）
                    WSAResetEvent(pClient->event);  // 事件被内核使用，重置需要手动

                    if (!success || transferred == 0) {
                        // 客户端断开或出错
                        printf("Client %d disconnected\n", pClient->sock);
                        closesocket(pClient->sock);
                        WSACloseEvent(pClient->event);
                        pClient->valid = FALSE;
                        // 从数组中移除（用最后一个覆盖当前）
                        eventArray[i] = eventArray[clientCount - 1];
                        clients[i] = clients[clientCount - 1];
                        clientCount--;
                        i--;    // 重检当前位置
                        continue;
                    }

                    // 成功收到数据
                    printf("Received %d bytes from %d: %.*s\n", transferred, pClient->sock,
                           transferred, pClient->buf);
                    // 回显（同步发送）
                    send(pClient->sock, pClient->buf, transferred, 0);

                    // 重新投递 WSARecv
                    memset(&pClient->overlapped, 0, sizeof(WSAOVERLAPPED));
                    pClient->overlapped.hEvent = pClient->event;
                    pClient->dataBuf.buf = pClient->buf;
                    pClient->dataBuf.len = BUF_SIZE;
                    pClient->recvBytes = 0;
                    pClient->flags = 0;
                    ret = WSARecv(pClient->sock, &pClient->dataBuf, 1,
                                  &pClient->recvBytes, &pClient->flags,
                                  &pClient->overlapped, NULL);
                    // printf("ret: %d\n", ret);
                    if (ret == SOCKET_ERROR) {
                        int err = WSAGetLastError();
                        // 是pending说明传输暂未完成，不是的话就说明连接断开或者传输错误
                        if (err != WSA_IO_PENDING) {
                            printf("WSARecv() failed for %d, error %d\n", pClient->sock, err);
                            closesocket(pClient->sock);
                            WSACloseEvent(pClient->event);
                            pClient->valid = FALSE;
                            eventArray[i] = eventArray[clientCount - 1];
                            clients[i] = clients[clientCount - 1];
                            clientCount--;
                            i--;
                        }
                    }
                    // 如果 ret == 0 表示数据已经立即就绪，但这种情况极少，我们仍然会等事件触发，所以没问题。
                }
            }
        }

        // 接受新连接（非阻塞）
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(hServSock, &readSet);
        struct timeval tv = {0, 0};   // 立即返回
        if (select(0, &readSet, NULL, NULL, &tv) > 0) {
            SOCKADDR_IN clntAddr;
            int addrLen = sizeof(clntAddr);
            SOCKET hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);
            if (hClntSock != INVALID_SOCKET) {
                AddClient(hClntSock);
            }
        }

        // 避免忙等，稍微休息一下
        Sleep(1);
    }

    // 清理（不会执行）
    WSACleanup();
    return 0;
}

void ErrorHandling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
