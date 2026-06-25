#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char send_buffer[BUFFER_SIZE] = {0};
    char buffer[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket 创建失败\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 将服务器 IP 地址从点分十进制转换为二进制
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("地址无效\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("连接失败\n");
        return -1;
    }
    printf("成功连接服务端！输入消息发送，输入 exit 退出\n");

    // 发送消息
    while (1) {
        // 清空缓冲区
        memset(send_buffer, 0, BUFFER_SIZE);
        memset(buffer, 0, BUFFER_SIZE);

        // 从控制台读取输入
        printf("请输入消息：");
        fgets(send_buffer, BUFFER_SIZE, stdin);

        // 去掉fgets自动读取的换行符
        send_buffer[strcspn(send_buffer, "\n")] = '\0';

        // 输入exit，退出循环
        if (strcmp(send_buffer, "exit") == 0) {
            printf("客户端退出\n");
            break;
        }

        // 发送消息
        send(sock, send_buffer, strlen(send_buffer), 0);
        
        // 接收服务端回显
        int len = read(sock, buffer, BUFFER_SIZE);
        if (len <= 0) {
            printf("服务端断开连接\n");
            break;
        }
        printf("服务端回显: %s\n", buffer);
    }

    close(sock);
    return 0;
}