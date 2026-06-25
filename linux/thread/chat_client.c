// chat_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#define BUF_SIZE  1024
#define NAME_SIZE 32

void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(char* message);

char gName[NAME_SIZE] = "[DEFAULT]";
char gMsg[BUF_SIZE];

int main(int argc, char* argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t send_thread, recv_thread;
    void* thread_return;

    if (argc != 4) {
        printf("Usage: %s <ip> <port> <name>\n", argv[0]);
        exit(1);
    }

    if (strlen(argv[3]) > 20) {
      printf("name: %s, len: %ld\n", argv[3], strlen(argv[3]));
      error_handling("Please input name length < 20 bytes.");
    }

    sprintf(gName, "%s", argv[3]);

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

    pthread_create(&send_thread, NULL, send_msg, (void*)&sock);
    pthread_create(&recv_thread, NULL, recv_msg, (void*)&sock);
    pthread_join(send_thread, &thread_return);
    pthread_join(recv_thread, &thread_return);
    
    close(sock);
    return 0;
}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void* send_msg(void* arg) {
    int sock = *(int*)arg;
    char name_msg[NAME_SIZE + BUF_SIZE];
    while (1) {
        fgets(gMsg, BUF_SIZE, stdin);
        if (!strcmp(gMsg, "q\n") || !strcmp(gMsg, "Q\n")) {
            close(sock);
            exit(0);
        }
        // 这里增加了2个bytes在消息中，为了避免字符串截断，长度也+2
        // 实际应该由程序解析消息格式，以此添加额外字符信息
        snprintf(name_msg, sizeof(name_msg) + 4, "%s: %s", gName, gMsg);
        write(sock, name_msg, strlen(name_msg));
    }
    return NULL;
}

void* recv_msg(void* arg) {
    int sock = *(int*)arg;
    char name_msg[NAME_SIZE + BUF_SIZE];
    int str_len;
    while (1) {
        str_len = read(sock, name_msg, NAME_SIZE + BUF_SIZE - 1);
        if (str_len == -1) {
            return (void*)-1;
        }
        name_msg[str_len] = 0;
        fputs("-> ", stdout);
        fputs(name_msg, stdout);
    }
    return NULL;
}
