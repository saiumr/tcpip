// udp_client_echo.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      // write  read  声明的地方
#include <arpa/inet.h>
#include <sys/socket.h>
#define BUF_SIZE 32
void error_handling(char* message);

int main(int argc, char** argv) {
  int clnt_sock;
  char message[BUF_SIZE];
  int str_len;
  socklen_t addr_size;
  struct sockaddr_in serv_addr, from_addr;

  if (argc != 3) {
  printf("Usage: %s <ip> <port>\n", argv[0]);
  exit(1);
  }

  clnt_sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (clnt_sock == -1)
  error_handling("socket() error!");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = PF_INET;
  serv_addr.sin_port   = htons(atoi(argv[2]));
  if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) == 0)  // 会自动转换字节序
  error_handling("inet_pton() error!");

  while(1) {
    fputs("Insert message(q to quit): ", stdout);
    fgets(message, sizeof(message), stdin);
    if (!strcmp(message, "q\n") || !strcmp(message, "Q\n")) break;
    sendto (clnt_sock, message, strlen(message), 0,
            (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    addr_size = sizeof(serv_addr);
    str_len = recvfrom(clnt_sock, message, BUF_SIZE, 0,
                (struct sockaddr*)&from_addr, &addr_size);
    message[str_len] = 0;
    printf("Message from server: %s\n", message);
    }
    close(clnt_sock);
    return 0;
  }

void error_handling(char* message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}