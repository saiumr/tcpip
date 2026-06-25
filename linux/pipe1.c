#include <stdio.h>
#include <unistd.h>
#define BUF_SIZE 30

int main(int argc, char *argv[])
{
    int fds[2];
    char str[] = "Who are you?";
    char buf[BUF_SIZE];
    pid_t pid;

    pipe(fds);       // 创建管道
    pid = fork();    // 创建子进程

    if(pid == 0)
    {
        // 子进程：向管道写端写入数据
        write(fds[1], str, sizeof(str));
    }
    else
    {
        // 父进程：从管道读端读取数据
        read(fds[0], buf, BUF_SIZE);
        puts(buf);
    }
    return 0;
}
