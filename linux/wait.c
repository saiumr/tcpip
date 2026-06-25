#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    int status;
    pid_t pid = fork();

    // 第一个子进程：return 3
    if(pid == 0)
    {
        return 3;
    }
    else
    {
        printf("CHILD PID: %d \n", pid);
        pid = fork();

        // 第二个子进程：exit(7)
        if(pid == 0)
        {
            exit(7);
        }
        else
        {
            printf("CHILD PID: %d \n", pid);

            // 第一次等待：回收任意子进程，获取状态
            wait(&status);
            if(WIFEXITED(status))
                printf("Child send one: %d \n",WEXITSTATUS(status));

            // 第二次等待：回收另一个子进程，获取状态
            wait(&status);
            if(WIFEXITED(status))
                printf("Child send two: %d \n", WEXITSTATUS(status));

            sleep(10); // 休眠10秒，父进程保持运行
        }
    }
    return 0;
}
