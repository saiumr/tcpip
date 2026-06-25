// chat_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024+2
#define MAX_CLNT 512
void* handle_clnt(void* arg);
void  send_msg(char* msg, int len);
void error_handling(char* message);

int gClnt_cnt = 0;
int gClnt_socks[MAX_CLNT];
pthread_mutex_t mutex;

int main(int argc, char* argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);

    pthread_t t_id;

    if (argc !=2 ) {
      printf("Usage: %s <port>\n", argv[0]);
      exit(1);
    }

    pthread_mutex_init(&mutex, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
      error_handling("bind() error.\n");
    if (listen(serv_sock, 5) == -1)
      error_handling("listen() error.\n");

    while(1) {
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_len);

        pthread_mutex_lock(&mutex);
        gClnt_socks[gClnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutex);

        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_addr.sin_addr));

    }
    
    close(serv_sock);
    return 0;
}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void* handle_clnt(void* arg) {
    int clnt_sock = *((int*)arg);
    int str_len = 0;
    int i;
    char msg[BUF_SIZE];

    while((str_len = read(clnt_sock, msg, BUF_SIZE)) != 0) {
        send_msg(msg, str_len);
    }

    // disconnected, remove this client
    pthread_mutex_lock(&mutex);
    for (i = 0; i < gClnt_cnt; i++) {
        if (clnt_sock == gClnt_socks[i]) {
            while (i++ < gClnt_cnt - 1) {
                gClnt_socks[i] = gClnt_socks[i+1];
            }
            break;
        }
    }
    gClnt_cnt--;
    pthread_mutex_unlock(&mutex);

    close(clnt_sock);
    return NULL;
}

void send_msg(char* msg, int len) {
    int i;
    pthread_mutex_lock(&mutex);
    for(i = 0; i < gClnt_cnt; i++) {
        write(gClnt_socks[i], msg, len);
    }
    pthread_mutex_unlock(&mutex);
}

