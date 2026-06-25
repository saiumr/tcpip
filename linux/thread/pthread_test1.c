/*
#include <pthread.h>

int pthread_create(pthread_t *thread,           // 输出：线程ID
                   const pthread_attr_t *attr, // 线程属性（一般填NULL）
                   void *(*start_routine)(void*), // 线程要执行的函数
                   void *arg);                 // 传给该函数的参数
*/

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void* print_numbers(void* arg) {
  int start = *(int*)arg;
  for (int i = 0; i < 3; ++i) {
    printf("thread: %d\n", start + i);
    sleep(1);
  }

  return NULL;
}

int main(int argc, char* argv[]) {
  pthread_t tid;
  int val = 100;
  pthread_create(&tid, NULL, print_numbers, &val);
  pthread_join(tid, NULL);    // 阻塞等待线程结束
  return 0;
}

