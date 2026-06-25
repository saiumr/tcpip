#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    int addr_len = sizeof(serv_addr);

    // 1. 创建服务端socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket 创建失败");
        exit(EXIT_FAILURE);
    }

    // 端口复用（解决重启报错问题）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // 2. 绑定端口
    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("绑定失败");
        exit(EXIT_FAILURE);
    }

    // 3. 监听
    if (listen(server_fd, 3) < 0) {
        perror("监听失败");
        exit(EXIT_FAILURE);
    }
    printf("服务端启动成功，监听端口 %d，等待客户端连接...\n", PORT);

    // 4. 接收客户端连接
    if ((client_fd = accept(server_fd, (struct sockaddr *)&serv_addr, (socklen_t*)&addr_len)) < 0) {
        perror("接收连接失败");
        exit(EXIT_FAILURE);
    }
    printf("客户端已连接！\n");

    // 5. 循环接收消息 + 回显（核心逻辑）
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        // 读取客户端消息
        int len = read(client_fd, buffer, BUFFER_SIZE);
        
        // 客户端断开或出错
        if (len <= 0) {
            printf("客户端断开连接\n");
            break;
        }
        
        printf("收到客户端消息: %s\n", buffer);
        
        // 回显给客户端
        send(client_fd, buffer, strlen(buffer), 0);
    }

    // 6. 关闭套接字
    close(client_fd);
    close(server_fd);
    return 0;
}