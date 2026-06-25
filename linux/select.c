#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUF_SIZE 30

int main(int argc, char *argv[])
{
    fd_set reads, temps;    // reads=原始监视集合，temps=select调用用的备份集合
    int result, str_len;    
    char buf[BUF_SIZE];     
    struct timeval timeout; // 超时结构体

    // 1. 初始化监视集合（清空）
    FD_ZERO(&reads);
    // 2. 注册：监视 标准输入(文件描述符=0)
    FD_SET(0, &reads);

    while(1)
    {
        // 关键：每次调用select前，必须备份原始集合（select会修改集合）
        temps = reads;

        // 3. 设置超时：5秒0微秒
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // 4. 调用select：监视读集合，超时5秒
        result = select(1, &temps, NULL, NULL, &timeout);

        if(result == -1) {
            puts("select() error!");
            break;
        }
        else if(result == 0) {
            puts("Time-out!"); // 5秒无输入，超时
        }
        else {
            // 5. 判断：标准输入是否就绪（有数据输入）
            if(FD_ISSET(0, &temps)) {
                str_len = read(0, buf, BUF_SIZE);
                buf[str_len] = 0;
                printf("message from console: %s", buf);
            }
        }
    }
    return 0;
}
