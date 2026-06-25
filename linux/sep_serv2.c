// sep_serv2.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define BUF_SIZE 1024
void error_handling(char* message);

int main(int argc, char** argv) {
    int serv_sock, clnt_sock;
    FILE* readfp;
    FILE* writefp;

    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;
    
    char buf[BUF_SIZE] = {0, };

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error!");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = PF_INET;
    serv_addr.sin_port        = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error!");

    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error!");

    clnt_addr_size = sizeof(clnt_addr);

    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
    readfp  = fdopen(clnt_sock, "r");
    writefp = fdopen(dup(clnt_sock), "w");  // 复制句柄，现在俩指针所指的句柄不同，句柄引用的套接字相同

    fputs("FROM SERVER: Hi~ client? \n", writefp);
    fputs("FROM SERVER: I love u, listen... \n", writefp);
    fputs("FROM SERVER: 鸡你太美~ \n", writefp);
    fflush(writefp);
    
    // 半关闭写流，表示发完了，释放指针销毁复制的句柄
    shutdown(fileno(writefp), SHUT_WR);  // 发送FIN包(EOF)
    fclose(writefp);

    // 继续从客户端读数据
    fgets(buf, BUF_SIZE, readfp);
    fputs(buf, stdout);
    fclose(readfp);  // 释放readfp，销毁句柄，引用计数为0，套接字销毁

    // 记得close服务端套接字句柄
    close(serv_sock);
    return 0;
}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
