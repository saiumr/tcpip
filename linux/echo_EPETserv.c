// echo_EPETserv.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE   4
#define EPOLL_SIZE 50

void setnonblockingmode(int fd);
void error_handling(char* message);

int main(int argc, char* argv[]) {
  int serv_sock, clnt_sock;
  struct sockaddr_in serv_addr, clnt_addr;
  socklen_t addr_size;
  int str_len, i;
  char buf[BUF_SIZE] = {0,};

  struct epoll_event* ep_events;
  struct epoll_event event;
  int epfd, event_cnt;

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

  epfd = epoll_create1(0);  // 现代函数，传0即可
  ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

  //
  // 原书有这行，把serv_sock设置非阻塞，但没必要
  // 如果 serv_sock 采用 EPOLLIN|EPOLLET，就必须：
  //   1. 设置 serv_sock 为非阻塞
  //   2. 在事件处理中循环 accept 直到返回 EAGAIN。
  //
  // setnonblockingmode(serv_sock);
  event.events = EPOLLIN;  // 读取数据的事件
  event.data.fd = serv_sock;
  epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

  while (1) {
    event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
    if (event_cnt == -1) {
      puts("epoll_wait() error!\n");
      break;
    }

    puts("return epoll_wait");
    for (i = 0; i < event_cnt; ++i) {
      // 遍历检查触发事件的fd
      if (ep_events[i].data.fd == serv_sock) {
        // 有client连接
        addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &addr_size);
        setnonblockingmode(clnt_sock);  // 数据可能一次无法读全，避免阻塞
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = clnt_sock;
        epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
        printf("connected client: %d\n", clnt_sock);
      } else {
        while (1) {
          str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);
          if (str_len == 0) {  // close request
            epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
            close(ep_events[i].data.fd);
            printf("closed client: %d\n", ep_events[i].data.fd);
            break;
          } else if (str_len == -1) {
            if (errno == EAGAIN) {
              break;
            }
          } else {
            write(ep_events[i].data.fd, buf, str_len);  // echo!
          }

        }
      }
    }
  }

  close(serv_sock);
  close(epfd);

  return 0;
}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void setnonblockingmode(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
      error_handling("Get flags failed!");
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
      error_handling("Set flags failed!");
    }
}