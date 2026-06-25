#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8888
#define MAX_EVENTS 1024

// 1）水平触发（LT）—— 默认模式（本代码）
// 缓冲区有数据就一直通知你
// 最稳定、最好用
// 适合新手
// 2）边缘触发（ET）—— 高性能（采用边缘触发需循环read/recv直到接收到EAGAIN）
// 只在状态变化时通知一次
// 必须非阻塞 + 循环读
// Nginx 用的就是 ET

// epoll_create 创建一个 epoll 实例
// epoll_ctl 向 epoll 注册(ADD) / 修改(MOD) / 删除(DEL) fd
// epoll_wait 等待就绪事件
// epoll_event 事件结构体
// EPOLLIN / EPOLLOUT 事件类型
int main(int argc, char* argv[]) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(PORT);
  bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));

  listen(server_fd, 128);

  // 创建epoll实例
  int epoll_fd = epoll_create1(0);

  // 编码和select类似，把服务器fs加入（监听新连接）
  struct epoll_event event;
  event.events = EPOLLIN;     // 监听可读事件
  event.data.fd = server_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

  // 事件数组（fd就绪之后存在此处，对服务端来说是有新连接，对客户端是发消息）
  struct epoll_event events[MAX_EVENTS];

  while (1) {
    // 等待就绪事件
    int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

    for (int i = 0; i < n; ++i) {
      int fd = events[i].data.fd;

      // 新客户端连接
      if (fd == server_fd) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);

        // 加入epoll
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);

        printf("新客户端连接：fd %d\n", client_fd);
      } else {
        // 客户端发来数据
        char buf[1024];
        int n = read(fd, buf, sizeof(buf));
        
        if (n <= 0) {
          // 客户端断开
          close(fd);
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
          printf("客户端断开：%d\n", fd);
        } else {
          // 收到数据
          buf[n] = '\0';
          printf("收到：%s\n", buf);
          // 回显
          send(fd, buf, sizeof(buf), 0);
        }
      }
    }
  }

  return 0;
}