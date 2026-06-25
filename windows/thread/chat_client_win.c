// chat_client_win.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>

#define BUF_SIZE 100
#define NAME_SIZE 20

unsigned WINAPI SendMsg(void* arg);
unsigned WINAPI RecvMsg(void* arg);
void ErrorHandling(char* message);

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

int main(int argc, char** argv) {
    WSADATA wsaData;
    SOCKET hSocket;
    SOCKADDR_IN servAddr;

    HANDLE hSendThread, hRecvThread;

    if (argc != 4) {
        printf("Usage: %s <ip> <port> <name>\n", argv[0]);
        exit(1);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    ErrorHandling("WSAStartup() error!");

    sprintf(name, "[%s]", argv[3]);
    hSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (hSocket == INVALID_SOCKET)
        ErrorHandling("socket() error!");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    // Windows 无 inet_aton，替换为 inet_addr（功能一致）
    servAddr.sin_addr.s_addr = inet_addr(argv[1]);
    servAddr.sin_port = htons(atoi(argv[2]));

    if (connect(hSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
        ErrorHandling("connect() error!");

    hSendThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&hSocket, 0,NULL);
    hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&hSocket, 0,NULL);

    WaitForSingleObject(hSendThread, INFINITE);
    WaitForSingleObject(hRecvThread, INFINITE);

    closesocket(hSocket);
    WSACleanup();
    return 0;
}

void ErrorHandling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// send thread main
unsigned WINAPI SendMsg(void* arg) {
    SOCKET hSock = *((SOCKET*)arg);
    char nameMsg[NAME_SIZE + BUF_SIZE];
    while (1) {
        fgets(msg, BUF_SIZE, stdin);
        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n"))  {
            closesocket(hSock);
            exit(0);
        }
        sprintf(nameMsg, "%s %s", name, msg);
        send(hSock, nameMsg, strlen(nameMsg), 0);
    }

    return 0;
}

// read thread main
unsigned WINAPI RecvMsg(void* arg) {
    int hSock = *((SOCKET*)arg);
    char nameMsg[NAME_SIZE + BUF_SIZE];
    int strLen;

    while(1) {
        strLen = recv(hSock, nameMsg, NAME_SIZE + BUF_SIZE - 1, 0);
        if (strLen == -1) {
            return -1;
        }
        nameMsg[strLen] = 0;
        fputs(nameMsg, stdout);
    }

    return 0;
}
