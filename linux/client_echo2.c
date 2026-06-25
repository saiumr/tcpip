// client_echo2.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      // write  read  声明的地方
#include <arpa/inet.h>
#include <sys/socket.h>
#define BUF_SIZE 1024
void error_handling(char* message);

int main(int argc, char** argv) {
    int clnt_sock;
    char message[BUF_SIZE];
    int str_len, recv_total_len, recv_one_len;
    struct sockaddr_in serv_addr;

    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(1);
    }
    
    clnt_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (clnt_sock == -1)
        error_handling("socket() error!");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_port   = htons(atoi(argv[2]));
    if (inet_aton(argv[1], &serv_addr.sin_addr) == 0)  // 会自动转换字节序
        error_handling("inet_aton() error!");

    // 建立连接
    if (connect(clnt_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error!");
    else
        puts("Connected........");

    while(1) {
        fputs("Input message(Q to quit): ", stdout);
        fgets(message, BUF_SIZE, stdin);

        if (!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;

        str_len = write(clnt_sock, message, strlen(message));      // 通过clnt_sock发送数据到服务端
        recv_total_len = 0;
        while (recv_total_len < str_len) {
            recv_one_len = read(clnt_sock, message + recv_total_len, BUF_SIZE-1);  // 通过clnt_sock从服务端接收数据
            if (recv_one_len == -1)
                error_handling("read() error!");
            recv_total_len += recv_one_len;
        }
        message[str_len] = 0;
        printf("Message from server: %s", message);
    }
    
    close(clnt_sock);
    return 0;
}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
