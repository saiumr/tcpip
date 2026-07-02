# Linux C Web Server 常见崩溃原因总结

> 项目类型：多线程 HTTP Server（仅处理 GET 请求）
>
> 平台：Linux
>
> 通信方式：Socket + pthread + recv/send

---

# 1. SIGPIPE —— 最容易导致服务器直接退出（★★★★★）

## 原因

Linux 中，当向一个**已经关闭的 TCP Socket**调用 `send()` 时：

```
Client
    │
    │ 关闭连接（RST）
    ▼
Server
    │
    │ send()
    ▼
Kernel
    │
    │ 发送 SIGPIPE
    ▼
默认行为：终止整个进程
```

例如：

```c
send(sock, buf, len, 0);
```

如果客户端已经关闭：

```
send()

↓

SIGPIPE

↓

程序退出
```

很多人误以为程序崩溃，其实是被 Linux 杀掉了。

---

## 如何修复

### 方法一（推荐）

程序启动时忽略 SIGPIPE：

```c
#include <signal.h>

int main()
{
    signal(SIGPIPE, SIG_IGN);

    ...
}
```

之后：

```
send()

↓

返回 -1

errno == EPIPE
```

服务器不会退出。

---

### 方法二

Linux 支持：

```c
send(sock, buf, len, MSG_NOSIGNAL);
```

发送失败不会产生 SIGPIPE。

---

## 需要检查 send 返回值

例如：

```c
if (send(sock, buf, len, 0) < 0)
{
    perror("send");
}
```

---

# 2. recv() 不会自动补 '\0'（★★★★★）

## 原因

很多新人都会误认为：

```c
recv(sock, buf, sizeof(buf), 0);
```

得到的是字符串。

实际上不是。

例如收到：

```
GET / HTTP/1.1\r\n...
```

内存实际可能是：

```
G E T ... \r \n ? ? ? ? ? ?
```

**没有 '\0'。**

如果直接：

```c
strstr(buf, "HTTP/");
```

或者

```c
strtok(buf, " ");
```

这些字符串函数会一直向后扫描内存。

严重时：

```
Segmentation Fault
```

---

## 如何修复

缓冲区多开一个字节：

```c
char buf[BUF_SIZE + 1];
```

recv 后：

```c
int recv_len = recv(sock, buf, BUF_SIZE, 0);

if (recv_len <= 0)
    ...

buf[recv_len] = '\0';
```

这样才能安全作为字符串处理。

---

# 3. 一个连接创建一个线程（★★★★★）

## 原因

当前代码：

```
accept()

↓

pthread_create()

↓

pthread_detach()
```

意味着：

**每个客户端都会创建一个线程。**

如果：

```
1000 个连接

↓

1000 个线程
```

Linux 默认线程栈：

```
8MB
```

所以：

```
1000 × 8MB

≈8GB 虚拟内存
```

大量请求后：

```
pthread_create()

↓

失败

↓

服务器无法继续工作
```

---

## 如何修复

推荐：

### 方法一

线程池

例如：

```
8 个线程

↓

不断处理请求
```

不会无限创建线程。

---

### 方法二

epoll

Nginx、Redis、Node.js 都采用事件驱动模型。

适合高并发。

---

# 4. send() 不保证一次发送完成（★★★★☆）

## 原因

很多人误认为：

```c
send(sock, buf, len, 0);
```

一定发送：

```
len 字节
```

实际上：

```
需要发送

4096

↓

send()

↓

只发送

1500
```

返回：

```c
1500
```

剩余：

```
2596
```

仍需要继续发送。

否则文件会损坏。

---

## 如何修复

循环发送：

```c
size_t sent = 0;

while (sent < bytes)
{
    int n = send(sock,
                 buf + sent,
                 bytes - sent,
                 0);

    if (n <= 0)
        break;

    sent += n;
}
```

---

# 5. strcpy() 存在缓冲区溢出（★★★★☆）

## 原因

例如：

```c
char method[100];

strcpy(method, token);
```

如果请求：

```
AAAAAAAAAAAAAAAAAAAAAAAAAAAA...
```

超过：

```
100 字节
```

会直接：

```
覆盖栈空间

↓

Segmentation Fault
```

---

## 如何修复

推荐：

```c
snprintf(method,
         sizeof(method),
         "%s",
         token);
```

或者：

```c
strncpy(method,
        token,
        sizeof(method)-1);
```

---

# 6. 路径穿越漏洞（★★★★☆）

## 原因

当前代码：

```c
snprintf(filename,
         sizeof(filename),
         "./%s%s",
         WEB_ROOT_PATH,
         temp);
```

如果用户访问：

```
GET /../../etc/passwd
```

最终：

```
./../../etc/passwd
```

直接：

```c
fopen()
```

即可读取系统文件。

属于：

```
Directory Traversal
```

漏洞。

---

## 如何修复

最简单：

禁止：

```
..
```

例如：

```c
if (strstr(temp, ".."))
{
    SendErrorMsg(sock);
    return;
}
```

更推荐：

```
realpath()

↓

判断是否仍位于 WebRoot
```

---

# 7. recv() 一次不一定收到完整 HTTP 请求（★★★☆☆）

## 原因

TCP：

```
没有消息边界
```

例如：

第一次：

```
GET /
```

第二次：

```
HTTP/1.1
Host:
...
```

你的代码：

```c
recv()

↓

立即解析
```

是不可靠的。

---

## 如何修复

持续接收：

```
直到出现：

\r\n\r\n
```

表示：

HTTP Header 接收完成。

---

# 8. listen(backlog) 太小（★★★☆☆）

当前：

```c
listen(sock, 5);
```

意味着：

最多：

```
5 个等待 accept 的连接
```

高并发：

```
队列满

↓

客户端连接失败
```

---

## 如何修复

建议：

```c
listen(sock, SOMAXCONN);
```

或者：

```c
listen(sock, 128);
```

---

# 9. 工具函数内部直接 exit()（★★★☆☆）

例如：

```c
if (stat(...) != 0)
{
    exit(1);
}
```

如果：

```
某个文件不存在
```

整个服务器：

```
退出
```

这是服务器开发的大忌。

---

## 如何修复

不要：

```c
exit()
```

应该：

```
返回错误

↓

SendErrorMsg()

↓

继续运行服务器
```

例如：

```c
if (stat(...) != 0)
{
    return -1;
}
```

---

# 10. 未检查 recv()/send()/fread() 返回值（★★★☆☆）

所有系统调用：

```
recv()

send()

read()

write()

fread()
```

都有可能失败。

例如：

```c
send()

↓

返回

-1
```

如果不检查：

程序逻辑可能异常。

---

## 如何修复

始终检查：

```c
int n = send(...);

if (n < 0)
{
    perror("send");
}
```

---

# 11. HTTP 请求解析不够健壮（★★☆☆☆）

例如：

```
GET
```

或者：

```
GET /abc
```

没有：

```
HTTP/1.1
```

当前：

```
strtok()

↓

NULL

↓

异常
```

虽然已有部分判断，

但建议：

统一校验：

```
Method

URL

Version
```

是否全部存在。

---

# 12. 没有限制最大请求大小（★★☆☆☆）

例如：

攻击者：

```
发送

100MB Header
```

当前：

```
recv()

↓

2048

↓

直接解析
```

应该：

限制：

```
Header 最大长度

例如：

8KB
```

超过：

```
400 Bad Request
```

---

# 13. 没有处理 Keep-Alive（★★☆☆☆）

当前：

```
HTTP/1.0

↓

发送

↓

close()
```

浏览器：

```
重新建立连接
```

效率较低。

虽然对于学习项目没有问题。

---

# 推荐修改优先级

| 优先级 | 建议 | 是否必须 |
|---------|------|-----------|
| ⭐⭐⭐⭐⭐ | 忽略 SIGPIPE | ✅ 必须 |
| ⭐⭐⭐⭐⭐ | recv 后补 '\0' | ✅ 必须 |
| ⭐⭐⭐⭐⭐ | send 检查返回值 | ✅ 必须 |
| ⭐⭐⭐⭐ | 修复 strcpy | ✅ 必须 |
| ⭐⭐⭐⭐ | 禁止目录穿越 | ✅ 必须 |
| ⭐⭐⭐⭐ | 去掉 exit() | ✅ 必须 |
| ⭐⭐⭐ | listen 改成 SOMAXCONN | 推荐 |
| ⭐⭐⭐ | recv 循环读取 Header | 推荐 |
| ⭐⭐⭐ | 线程池代替无限创建线程 | 推荐 |
| ⭐⭐ | Keep-Alive | 可选 |

---

# 一个成熟 Linux HTTP Server 至少应该具备

- ✅ 忽略 SIGPIPE
- ✅ 检查所有系统调用返回值
- ✅ 正确处理部分发送（Partial Send）
- ✅ 正确处理部分接收（Partial Receive）
- ✅ 防止目录穿越（Directory Traversal）
- ✅ 防止缓冲区溢出（Buffer Overflow）
- ✅ 不在业务函数中调用 `exit()`
- ✅ 合理的并发模型（线程池或 epoll）
- ✅ 完整解析 HTTP 协议
- ✅ 完善的错误处理与日志记录
