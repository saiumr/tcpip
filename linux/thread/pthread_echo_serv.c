// pthread_echo_serv.c
/*
多线程echo服务器

1. 主线程创建监听 socket，等待客户端连接。
2. 每来一个客户端，就创建一个新线程去处理它（读取数据并原样发回）。
3. 客户端断开后线程自动结束。（客户端使用client_echo2）
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
void error_handling(char* message);
void *handle_client(void* arg);

int main(int argc, char* argv[]) {
  int serv_sock, clnt_sock;
  struct sockaddr_in serv_addr, clnt_addr;
  socklen_t clnt_addr_len = sizeof(clnt_addr);

  if (argc !=2 ) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(atoi(argv[1]));

  if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("bind() error.\n");
  if (listen(serv_sock, 5) == -1)
    error_handling("listen() error.\n");

  while (1) {
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_len);

    // 为每个客户端创建一个线程
    pthread_t tid;
    // 动态分配空间，传递clnt_sock
    int *pclient = malloc(sizeof(int));
    *pclient = clnt_sock;
// 传递clnt_sock必须要新的地址空间，请看下面的时间线
/*
主线程                           子线程（尚未运行）
----------                       ----------------
client_fd = 100
pthread_create(&tid, &client_fd)   [参数记录了地址 &client_fd]
                                   （线程就绪，但未跑）
client_fd = 200（下一次 accept）
                                   （终于被调度）
                                   int fd = *(int*)arg;
                                   // 读到 200，而不是 100
*/
    if (pthread_create(&tid, NULL, handle_client, pclient) != 0) {
      perror("pthread_create");
      close(clnt_sock);
      free(pclient);
    } else {
      pthread_detach(tid);  // 标记线程为可分离状态，不阻塞代码执行，线程结束后自动回收资源，不需要join
    }
  }

  close(serv_sock);
  return 0;
}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void *handle_client(void* arg) {
  int client_fd = *(int*)arg;
  free(arg);
  char buf[BUF_SIZE];
  ssize_t n;

  printf("New Client: %d\n", client_fd);

  while( (n = read(client_fd, buf, sizeof(buf))) > 0 ) {
    write(client_fd, buf, n);
  }

  close(client_fd);
  printf("Client %d Closed.\n", client_fd);
  return NULL;  // 正常退出
}

