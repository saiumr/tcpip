// zombie.c
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main()
{
	pid_t pid = fork();

	if (pid == 0)
	{
		printf("子进程(%d) 已终止，等待父进程回收...\n", getpid());
		return 0;
	}

	printf("父进程(%d) 休眠中，未回收子进程资源\n", getpid());
  printf("父进程中打印子进程pid：%d \n", pid);
	sleep(10);

	return 0;
}