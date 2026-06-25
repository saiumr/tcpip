// waitpid.c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    int status;
    pid_t pid = fork();

    // 子进程：休眠15秒后退出，返回值24
    if(pid == 0)
    {
        sleep(15);
        return 24;
    }
    // 父进程：非阻塞轮询回收子进程
    else
    {
        // waitpid返回0时，循环继续；返回子进程PID时，循环结束
        while(!waitpid(-1, &status, WNOHANG))
        {
            sleep(3);
            puts("sleep 3sec.");
        }

        // 解析子进程退出状态
        if(WIFEXITED(status))
            printf("Child send %d \n", WEXITSTATUS(status));
    }

    return 0;
}
