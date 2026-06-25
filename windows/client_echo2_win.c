// client_echo2_win.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define BUF_SIZE 1024

void error_handling(char* message);

int main(int argc, char** argv) {
    SOCKET clnt_sock;
    char message[BUF_SIZE];
    int str_len, recv_total_len, recv_one_len;
    struct sockaddr_in serv_addr;

    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(1);
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        error_handling("WSAStartup() error!");

    clnt_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (clnt_sock == INVALID_SOCKET)
        error_handling("socket() error!");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    // Windows 无 inet_aton，替换为 inet_addr（功能一致）
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    if (serv_addr.sin_addr.s_addr == INADDR_NONE)
        error_handling("inet_addr() error!");

    // 建立连接（函数名与 Linux 一致）
    if (connect(clnt_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
        error_handling("connect() error!");
    else
        puts("Connected........");

    while (1) {
        fputs("Input message(Q to quit): ", stdout);
        fgets(message, BUF_SIZE, stdin);

        if (!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;

        str_len = strlen(message);
        send(clnt_sock, message, str_len, 0);

        recv_total_len = 0;
        while (recv_total_len < str_len) {
            recv_one_len = recv(clnt_sock, message + recv_total_len, BUF_SIZE - 1, 0);
            if (recv_one_len == SOCKET_ERROR)
                error_handling("recv() error!");
            recv_total_len += recv_one_len;
        }

        message[str_len] = 0;
        printf("Message from server: %s", message);
    }

    closesocket(clnt_sock);
    WSACleanup();
    return 0;
}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

