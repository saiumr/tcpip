#include <stdio.h>
#include <unistd.h>
#define BUF_SIZE 30

int main(int argc,char *argv[])
{
    int fds1[2], fds2[2];
    char str1[]="Who are you?";
    char str2[]="Thank you for your message";
    char buf[BUF_SIZE];
    pid_t pid;

    // 创建两个独立管道
    pipe(fds1);
    pipe(fds2);
    pid = fork();

    if(pid==0)
    {
        // 子进程：管道1 写数据 → 发给父进程
        write(fds1[1], str1, sizeof(str1));
        // 子进程：管道2 读数据 ← 接收父进程回复
        read(fds2[0], buf, BUF_SIZE);
        printf("Child proc output: %s \n",buf);
    }
    else
    {
        // 父进程：管道1 读数据 ← 接收子进程消息
        read(fds1[0],buf, BUF_SIZE);
        printf("Parent proc output: %s \n",buf);
        // 父进程：管道2 写数据 → 回复子进程
        write(fds2[1],str2,sizeof(str2));
        sleep(3);
    }
    return 0;
}
