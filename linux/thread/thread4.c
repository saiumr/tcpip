// thread4.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// 线程总数
#define NUM_THREAD 100

// 全局共享变量（多线程竞争访问）
long long num = 0;

// 线程函数声明
void *thread_inc(void *arg);  // 累加线程
void *thread_des(void *arg);  // 递减线程

int main(int argc, char *argv[])
{
    pthread_t thread_id[NUM_THREAD];
    int i;

    // 打印long long类型大小（8字节=64位）
    printf("sizeof long long: %lu\n", sizeof(long long));

    // 创建线程：奇数创建累加线程，偶数创建递减线程
    for (i = 0; i < NUM_THREAD; i++)
    {
        if (i % 2)
            pthread_create(&thread_id[i], NULL, thread_inc, NULL);
        else
            pthread_create(&thread_id[i], NULL, thread_des, NULL);
    }

    // 等待所有线程执行完毕
    for (i = 0; i < NUM_THREAD; i++)
        pthread_join(thread_id[i], NULL);

    // 打印最终结果（无锁，结果一定错误）
    printf("result: %lld\n", num);
    return 0;
}

// 累加线程函数：num +1 执行50万次
void *thread_inc(void *arg)
{
    int i;
    for (i = 0; i < 500000; i++)
        num += 1;
    return NULL;
}

// 递减线程函数：num -1 执行50万次
void *thread_des(void *arg)
{
    int i;
    for (i = 0; i < 500000; i++)
        num -= 1;
    return NULL;
}
