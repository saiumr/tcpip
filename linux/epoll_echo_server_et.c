#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
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

// 设置非阻塞模式
void set_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

int main(int argc, char* argv[]) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return -1;
    }

    // 端口复用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 128) == -1) {
        perror("listen failed");
        close(server_fd);
        return -1;
    }

    // 创建epoll实例
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(server_fd);
        return -1;
    }

    // 服务端fd非阻塞（ET模式必须）
    set_nonblocking(server_fd);

    // 添加服务端fd到epoll（ET模式）
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;  // 边缘触发
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl add server failed");
        close(server_fd);
        close(epoll_fd);
        return -1;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("Epoll ET 服务端启动成功，监听端口 %d\n", PORT);

    while (1) {
        // 等待事件
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait failed");
            continue;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            // 处理新连接（ET模式：必须循环accept）
            if (fd == server_fd) {
                // 循环accept，直到EAGAIN
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);

                    if (client_fd == -1) {
                        // 没有新连接了，退出循环
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        } else {
                            perror("accept failed");
                            break;
                        }
                    }

                    printf("新客户端连接：fd %d\n", client_fd);
                    set_nonblocking(client_fd);

                    // 添加客户端到epoll（ET模式）
                    event.events = EPOLLIN | EPOLLET;
                    event.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                }
            }

            // 处理客户端消息（ET模式：循环read）
            else {
                char buf[1024];
                while (1) {
                    // 每次read前清空缓冲区（防止乱码）
                    memset(buf, 0, sizeof(buf));
                    ssize_t read_len = read(fd, buf, sizeof(buf));

                    if (read_len > 0) {
                        printf("收到客户端%d数据：%s\n", fd, buf);

                        // send只发送有效数据长度，不发空字符
                        send(fd, buf, read_len, 0);
                    }
                    // 客户端断开连接
                    else if (read_len == 0) {
                        printf("客户端%d断开连接\n", fd);
                        // 注意此处，先删epoll，再关闭fd
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        break;
                    }
                    // 读取完毕（EAGAIN）或错误
                    else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // printf("数据读取完成\n");
                            break;
                        } else {
                            perror("read error");
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                            break;
                        }
                    }
                }
            }
        }
    }

    // 资源释放（理论上到不了这里）
    close(server_fd);
    close(epoll_fd);
    return 0;
}