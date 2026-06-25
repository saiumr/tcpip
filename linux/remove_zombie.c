// remove_zombie.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// SIGCHLD信号处理函数：自动回收子进程
void read_childproc(int sig){
    int status;
    pid_t id = waitpid(-1, &status, WNOHANG);
    if(WIFEXITED(status))
    {
        printf("Removed proc id: %d\n", id);
        printf("Child send: %d\n", WEXITSTATUS(status));
    }
}

int main(int argc, char *argv[])
{
    pid_t pid;
    struct sigaction act;
    act.sa_handler = read_childproc;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, 0);

    // 创建第一个子进程
    pid = fork();
    if(pid == 0)
    {
        puts("Hi! I'm child process");
        sleep(10);
        return 12;
    }
    else
    {
        printf("Child proc id: %d\n", pid);
        // 创建第二个子进程
        pid = fork();
        if(pid == 0)
        {
            puts("Hi! I'm child process");
            sleep(10);
            exit(24);
        }
        else
        {
            int i;
            printf("Child proc id: %d\n", pid);
            for(i=0; i<5; i++)
            {
                puts("wait...");
                sleep(5);
            }
        }
    }
    return 0;
}
