// sep_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define BUF_SIZE 1024
void error_handling(char* message);

int main(int argc, char** argv) {
    int sock;
    char buf[BUF_SIZE] = {0,};
    struct sockaddr_in serv_addr;

    FILE* readfp;
    FILE* writefp;

    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(1);
    }
    
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error!");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_port   = htons(atoi(argv[2]));
    if (inet_aton(argv[1], &serv_addr.sin_addr) == 0)  // 会自动转换字节序
        error_handling("inet_aton() error!");

    // 建立连接
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error!");
    else
        puts("Connected........");

    readfp = fdopen(sock, "r");
    writefp = fdopen(dup(sock), "w");
    while(1) {
        if (fgets(buf, BUF_SIZE, readfp) == NULL) {
          break;
        }

        fputs(buf, stdout);
    }
    
    fputs("FROM CLIENT: Thank you! \n", writefp);
    fflush(writefp);

    fclose(writefp);
    fclose(readfp);
    return 0;
}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

