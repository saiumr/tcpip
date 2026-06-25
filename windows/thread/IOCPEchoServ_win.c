// IOCPEchoServ_win_fixed.c
// IOCP Echo 服务器（deepseek 改进版）

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <winsock2.h>
#include <windows.h>

#define BUF_SIZE 1024
#define READ  3
#define WRITE 5
#define EXIT  9   // 新增：退出标志

typedef struct {
    SOCKET hClntSock;
    SOCKADDR_IN clntAddr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[BUF_SIZE];
    int rwMode;
    LPPER_HANDLE_DATA handleInfo;  // 反向关联，方便清理
} PER_IO_DATA, *LPPER_IO_DATA;

unsigned int WINAPI EchoThreadMain(LPVOID CompletionPortIO);
void ErrorHandling(char* message);

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    HANDLE hComPort;
    SYSTEM_INFO sysInfo;
    LPPER_HANDLE_DATA handleInfo;
    LPPER_IO_DATA ioInfo;
    SOCKET hServSock;
    SOCKADDR_IN servAddr;
    int recvBytes = 0, flags = 0;
    int i;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    // 1. 创建完成端口
    hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (hComPort == NULL)
        ErrorHandling("CreateIoCompletionPort() error!");

    // 2. 创建工作者线程（CPU核心数 × 2）
    GetSystemInfo(&sysInfo);
    int threadCount = sysInfo.dwNumberOfProcessors * 2;
    printf("Creating %d worker threads...\n", threadCount);

    for (i = 0; i < threadCount; ++i) {
        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, EchoThreadMain, (LPVOID)hComPort, 0, NULL);
        if (hThread == 0)
            ErrorHandling("_beginthreadex() error!");
        CloseHandle(hThread);  // 线程运行后，主线程不再需要这个句柄
    }

    // 3. 创建重叠 I/O 监听 socket（OVERLAPPED）
    hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (hServSock == INVALID_SOCKET)
        ErrorHandling("WSASocket() error!");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(atoi(argv[1]));

    if (bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
        ErrorHandling("bind() error!");

    if (listen(hServSock, SOMAXCONN) == SOCKET_ERROR)
        ErrorHandling("listen() error!");

    printf("Echo server started on port %s\n", argv[1]);
    printf("Worker threads: %d\n", threadCount);

    // 4. 主循环：接受连接并投递第一个 WSARecv
    while (1) {
        SOCKET hClntSock;
        SOCKADDR_IN clntAddr;
        int addrLen = sizeof(clntAddr);

        hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);
        if (hClntSock == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                continue;  // 非阻塞模式下没连接，继续循环
            }
            ErrorHandling("accept() error!");
        }

        printf("Client connected: %d\n", hClntSock);

        // 分配连接级上下文
        handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
        if (handleInfo == NULL) {
            closesocket(hClntSock);
            continue;
        }
        handleInfo->hClntSock = hClntSock;
        memcpy(&handleInfo->clntAddr, &clntAddr, sizeof(clntAddr));

        // 关联 socket 到完成端口
        if (CreateIoCompletionPort((HANDLE)hClntSock, hComPort, (ULONG_PTR)handleInfo, 0) == NULL) {
            closesocket(hClntSock);
            free(handleInfo);
            ErrorHandling("CreateIoCompletionPort(associate) error!");
            continue;
        }

        // 分配 I/O 级上下文
        ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
        if (ioInfo == NULL) {
            closesocket(hClntSock);
            free(handleInfo);
            continue;
        }
        memset(ioInfo, 0, sizeof(PER_IO_DATA));
        ioInfo->wsaBuf.len = BUF_SIZE;
        ioInfo->wsaBuf.buf = ioInfo->buffer;
        ioInfo->rwMode = READ;
        ioInfo->handleInfo = handleInfo;  // 反向关联

        // 投递第一个 WSARecv（其实不需要recvBytes参数）
        flags = 0;
        int ret = WSARecv(hClntSock, &ioInfo->wsaBuf, 1, (LPDWORD)&recvBytes,
                          (LPDWORD)&flags, &ioInfo->overlapped, NULL);
        if (ret == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING) {
                // 投递失败，释放资源
                printf("WSARecv() failed for %d, error %d\n", hClntSock, err);
                closesocket(hClntSock);
                free(handleInfo);
                free(ioInfo);
            }
        }
        // 若返回 0 或 WSA_IO_PENDING，都是正常情况
    }

    // 清理（实际不会执行到，因为上面无限循环）
    WSACleanup();
    return 0;
}

// 工作者线程主函数
unsigned int WINAPI EchoThreadMain(LPVOID CompletionPortIO) {
    HANDLE hComPort = (HANDLE)CompletionPortIO;
    DWORD bytesTrans;
    LPPER_HANDLE_DATA handleInfo;
    LPPER_IO_DATA ioInfo;
    DWORD flags = 0;
    int ret;

    while (1) {
        // 1. 从完成端口取完成包
        BOOL success = GetQueuedCompletionStatus(
            hComPort,
            &bytesTrans,
            (PULONG_PTR)&handleInfo,
            (LPOVERLAPPED*)&ioInfo,
            INFINITE
        );

        if (!success) {
            // 错误处理：可能是连接断开或端口关闭
            if (ioInfo == NULL) {
                // 没有关联的 I/O 上下文，可能是 PostQueuedCompletionStatus 发来的退出信号
                printf("Worker thread exiting...\n");
                break;
            }
            // 该连接出错
            printf("I/O error on socket %d\n", handleInfo->hClntSock);
            closesocket(handleInfo->hClntSock);
            free(handleInfo);
            free(ioInfo);
            continue;
        }

        // 2. 处理完成包
        if (ioInfo->rwMode == READ) {
            // ----- READ 完成 -----
            if (bytesTrans == 0) {
                // 客户端正常断开
                printf("Client %d disconnected\n", handleInfo->hClntSock);
                closesocket(handleInfo->hClntSock);
                free(handleInfo);
                free(ioInfo);
                continue;
            }

            printf("Received %d bytes from %d\n", bytesTrans, handleInfo->hClntSock);

            // 准备发送回显（复用同一个 ioInfo）
            memset(&ioInfo->overlapped, 0, sizeof(OVERLAPPED));
            ioInfo->wsaBuf.len = bytesTrans;      // 发送长度 = 实际接收长度
            ioInfo->rwMode = WRITE;

            ret = WSASend(handleInfo->hClntSock, &ioInfo->wsaBuf, 1,
                          NULL, 0, &ioInfo->overlapped, NULL);
            if (ret == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSA_IO_PENDING) {
                    // 投递失败，释放资源
                    printf("WSASend() failed for %d, error %d\n", handleInfo->hClntSock, err);
                    closesocket(handleInfo->hClntSock);
                    free(handleInfo);
                    free(ioInfo);
                }
            }
            // 发送已投递，完成时会再次进入这个线程的 GetQueuedCompletionStatus
        }
        else if (ioInfo->rwMode == WRITE) {
            // ----- WRITE 完成 -----
            // 发送完成，释放发送用的 ioInfo
            free(ioInfo);

            // 分配新的 ioInfo 用于下一次接收
            LPPER_IO_DATA newIoInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
            if (newIoInfo == NULL) {
                // 内存不足，断开连接
                printf("Memory allocation failed, closing %d\n", handleInfo->hClntSock);
                closesocket(handleInfo->hClntSock);
                free(handleInfo);
                continue;
            }
            memset(newIoInfo, 0, sizeof(PER_IO_DATA));
            newIoInfo->wsaBuf.len = BUF_SIZE;
            newIoInfo->wsaBuf.buf = newIoInfo->buffer;
            newIoInfo->rwMode = READ;
            newIoInfo->handleInfo = handleInfo;

            flags = 0;
            ret = WSARecv(handleInfo->hClntSock, &newIoInfo->wsaBuf, 1,
                          NULL, &flags, &newIoInfo->overlapped, NULL);
            if (ret == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSA_IO_PENDING) {
                    printf("WSARecv() failed for %d, error %d\n", handleInfo->hClntSock, err);
                    closesocket(handleInfo->hClntSock);
                    free(handleInfo);
                    free(newIoInfo);
                }
            }
        }
        else {
            // 未知模式
            free(ioInfo);
        }
    }
    return 0;
}

void ErrorHandling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
