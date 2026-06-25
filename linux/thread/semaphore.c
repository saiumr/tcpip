// semaphore.c
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

// 线程函数声明
void *read_num(void *arg);
void *accu(void *arg);

// 全局信号量 + 共享变量
static sem_t sem_one;
static sem_t sem_two;
static int num;

int main(int argc, char *argv[])
{
    pthread_t id_t1, id_t2;

    // 初始化信号量：sem_one初始值0，sem_two初始值1
    sem_init(&sem_one, 0, 0);
    sem_init(&sem_two, 0, 1);

    // 创建线程
    pthread_create(&id_t1, NULL, read_num, NULL);
    pthread_create(&id_t2, NULL, accu, NULL);

    // 等待线程结束
    pthread_join(id_t1, NULL);
    pthread_join(id_t2, NULL);

    // 销毁信号量
    sem_destroy(&sem_one);
    sem_destroy(&sem_two);

    return 0;
}

// 输入数字的线程
void *read_num(void *arg)
{
    int i;
    for (i = 0; i < 5; i++)
    {
        fputs("Input num: ", stdout);
        sem_wait(&sem_two);    // 等待sem_two可用（初始化为1，所以一开始就可用）
        scanf("%d", &num);    // 输入数字
        sem_post(&sem_one);   // 通知累加线程
    }
    return NULL;
}

// 累加求和的线程
void *accu(void *arg)
{
    int sum = 0, i;
    for (i = 0; i < 5; i++)
    {
        sem_wait(&sem_one);   // 等待输入完成（初始化为0，需要等到另一个线程先跑给他资源）
        sum += num;           // 累加数字
        sem_post(&sem_two);   // 通知输入线程
    }
    printf("RESULT: %d\n", sum);  // 输出结果
    return NULL;
}
