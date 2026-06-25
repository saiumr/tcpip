#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8888
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// 使用 select 实现多客户端 echo 服务器
// 缺点：一般最多也就1024个fd，遍历检查，每次都要将整个fd_set从用户态拷贝到内核态
int main(int argc, char* argv[]) {
  int server_fd, new_socket, client_socket[MAX_CLIENTS], max_sd, sd, activity, valread;
  struct sockaddr_in address;
  fd_set readfds;
  char   buffer[BUFFER_SIZE];

  for (int i = 0; i < MAX_CLIENTS; ++i) client_socket[i] = 0;

  // 创建新监听套接字
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(1);
  }
  // 允许ip复用，tcp连接断开后不必强制等待1-2分钟再用同一端口开启服务端，可以立刻开
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(address));

  address.sin_family = AF_INET;
  address.sin_port = htons(PORT);
  address.sin_addr.s_addr = INADDR_ANY;  // 监听所有地址8888端口
  bind(server_fd, (struct sockaddr*)&address, sizeof(address));
  listen(server_fd, 3);   // 等待区至多3个客户端连接，多出则拒绝连接，accept之后等待区才会空闲（一般写128,1024大值）
  printf("服务器监听端口 %d\n", PORT);

  while (1) {
    // FD_ZERO(&readfds)	把所有开关全部关掉	所有位 → 0
    // FD_SET(fd, &readfds)	把编号为 fd 的那个开关，打开	第 fd 位 → 1
    // FD_ISSET(fd, &readfds)	看看编号 fd 的开关是不是打开的	判断第 fd 位是否为 1
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    max_sd = server_fd;

    // 将客户端套接字加入集合
    for (int i = 0; i < MAX_CLIENTS; ++i) {
      sd = client_socket[i];
      if (sd > 0) FD_SET(sd, &readfds);
      if (sd > max_sd) max_sd = sd;
    }
    
    // 阻塞等待活动（最后一个time_out参数为NULL）
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
    if (activity < 0) {
      perror("select error");
    }

    // 检查监听套接字
    if (FD_ISSET(server_fd, &readfds)) {
      int addrlen = sizeof(address);
      new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
      printf("新连接: fd %d, IP %s, 端口 %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

      // 将新的套接字存入数组
      for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (client_socket[i] == 0) {
          client_socket[i] = new_socket;
          break;
        }
      }
    }

    // 检查客户端套接字
    for (int i = 0; i < MAX_CLIENTS; ++i) {
      sd = client_socket[i];
      if (FD_ISSET(sd, &readfds)) {
        valread = read(sd, buffer, BUFFER_SIZE);
        if (valread == 0) {
          struct sockaddr_in dc_address;
          int dc_addrlen = sizeof(dc_address);
          getpeername(sd, (struct sockaddr*)&dc_address, (socklen_t*)&dc_addrlen);  // 获取已连接客户端的地址和端口号
          printf("客户端断开：IP %s, 端口 %d\n", inet_ntoa(dc_address.sin_addr), ntohs(dc_address.sin_port));
          close(sd);
          client_socket[i] = 0;
        } else {
          buffer[valread] = '\0';
          printf("收到：%s\n", buffer);
          send(sd, buffer, strlen(buffer), 0);
        }
      }
    }

  }


  return 0;
}
