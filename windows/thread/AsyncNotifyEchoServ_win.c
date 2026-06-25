// AsyncNotifyEchoServ_win.c
#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#define BUF_SIZE 100

void CompressSockets(SOCKET hSocketArr[], int idx, int total);
void CompressEvents(WSAEVENT hEventArr[], int idx, int total);
void ErrorHandling(char* message);

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET hServSock, hClntSock;
    SOCKADDR_IN servAddr, clntAddr;

    SOCKET hSockArr[WSA_MAXIMUM_WAIT_EVENTS];
    WSAEVENT hEventArr[WSA_MAXIMUM_WAIT_EVENTS];
    WSAEVENT newEvent;
    WSANETWORKEVENTS netEvents;

    int numOfClntSock = 0;
    int strLen, i;
    int posInfo, startIdx;
    int szClntAddr;
    char msg[BUF_SIZE];

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    hServSock = socket(PF_INET, SOCK_STREAM, 0);
    if (hServSock == INVALID_SOCKET)
        ErrorHandling("socket() error!");

    memset(&servAddr, 0, sizeof(servAddr));
    printf("servaddr size: %zu\n", sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(atoi(argv[1]));

    if (bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
        ErrorHandling("bind() error!");

    if (listen(hServSock, 5) == SOCKET_ERROR)
        ErrorHandling("listen() error!");

    // WSAEventSelect注册异步通知事件（echo服务端注册接受连接事件即可，有新连接来到时将会触发）
    // 内核在事件发生时自动把事件置为signaled
    newEvent = WSACreateEvent();
    if ( WSAEventSelect(hServSock, newEvent, FD_ACCEPT) == SOCKET_ERROR)
        ErrorHandling("WSAEventSelect() error!");

    // 事件对象和套接字是配套的
    hSockArr[numOfClntSock] = hServSock;
    hEventArr[numOfClntSock] = newEvent;
    numOfClntSock++;

    while (1) {
        // 阻塞等待事件发生（第三个参数WaitForAll为FALSE，等到第一个就返回）
        posInfo = WSAWaitForMultipleEvents(numOfClntSock, hEventArr, FALSE, WSA_INFINITE, FALSE);
        startIdx = posInfo - WSA_WAIT_EVENT_0;

        for (i = startIdx; i < numOfClntSock; ++i) {
            // 上面检测到第一个事件发生返回了，后面的还是需要检测，于是遍历检测进行后续处理
            int sigEventidx = WSAWaitForMultipleEvents(1, &hEventArr[i], TRUE, 0, FALSE);
            if (sigEventidx == WSA_WAIT_FAILED || sigEventidx == WSA_WAIT_TIMEOUT) {
                continue;
            }
            else {
                sigEventidx = i;
                // 获取具体事件类型，把事件置为non-signaled
                WSAEnumNetworkEvents(hSockArr[sigEventidx], hEventArr[sigEventidx], &netEvents);

                // 服务端接受连接事件发生 -> 请求连时处理（添加并监听客户端）
                if (netEvents.lNetworkEvents & FD_ACCEPT) {
                    if (netEvents.iErrorCode[FD_ACCEPT_BIT] != 0) {
                        puts("Accept Error!");
                        break;
                    }
                    szClntAddr = sizeof(clntAddr);
                    hClntSock = accept(hSockArr[sigEventidx], (SOCKADDR*)&clntAddr, &szClntAddr);
                    if (hClntSock == INVALID_SOCKET)
                        ErrorHandling("accept() error!");
                    // 使用异步事件对象监听新的客户端（对客户端监测读和关闭事件）
                    newEvent = WSACreateEvent();
                    WSAEventSelect(hClntSock, newEvent, FD_READ|FD_CLOSE);

                    hSockArr[numOfClntSock] = hClntSock;
                    hEventArr[numOfClntSock] = newEvent;
                    numOfClntSock++;
                    puts("connected new client...");
                }

                // 接收数据时处理
                if (netEvents.lNetworkEvents & FD_READ) {
                    if (netEvents.iErrorCode[FD_READ_BIT] != 0) {
                        puts("Read Error");
                        break;
                    }

                    strLen = recv(hSockArr[sigEventidx], msg, sizeof(msg), 0);
                    send(hSockArr[sigEventidx], msg, strLen, 0);
                }

                // 断开连接时处理
                if (netEvents.lNetworkEvents & FD_CLOSE) {
                    if (netEvents.iErrorCode[FD_CLOSE_BIT] != 0) {
                        puts("Close Error!");
                        break;
                    }
                    WSACloseEvent(hEventArr[sigEventidx]);
                    closesocket(hSockArr[sigEventidx]);

                    numOfClntSock--;
                    CompressSockets(hSockArr, sigEventidx, numOfClntSock);
                    CompressEvents(hEventArr, sigEventidx, numOfClntSock);
                }    
            }
        }
    }

    WSACleanup();

    return 0;
}

void ErrorHandling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void CompressSockets(SOCKET hSocketArr[], int idx, int total) {
    int i;
    for (i = idx; i < total; ++i)
        hSocketArr[i] = hSocketArr[i+1];
}

void CompressEvents(WSAEVENT hEventArr[], int idx, int total) {
    int i;
    for (i = idx; i < total; ++i)
        hEventArr[i] = hEventArr[i+1];
}
