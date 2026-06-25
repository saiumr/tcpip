// SyncCS_win.c
#include <windows.h>
#include <process.h>   // _beginthreadex  _endthreadex
#include <stdio.h>

#define NUM_THREAD 50
unsigned WINAPI ThreadInc(void* arg);
unsigned WINAPI ThreadDes(void* arg);
long long num = 0;
CRITICAL_SECTION cs;

int main(int argc, char* argv[]) {
    HANDLE tHandles[NUM_THREAD];
    int i;

    // 初始化资源
    InitializeCriticalSection(&cs);
    for (i = 0; i < NUM_THREAD; ++i) {
        if (i % 2) {
            tHandles[i] = (HANDLE)_beginthreadex(NULL, 0, ThreadInc, NULL, 0, NULL);
        } else {
            tHandles[i] = (HANDLE)_beginthreadex(NULL, 0, ThreadDes, NULL, 0, NULL);
        }
    }
    WaitForMultipleObjects(NUM_THREAD, tHandles, TRUE, INFINITE);
    // 释放资源
    DeleteCriticalSection(&cs);
    
    printf("result: %lld\n", num);

    return 0;
}

unsigned WINAPI ThreadInc(void* arg) {
    int i;
    // 持有key，逻辑类似加锁
    EnterCriticalSection(&cs);
    for (i = 0; i < 500000; ++i) {
        num += 1;
    }
    // 上交key，逻辑类似解锁
    LeaveCriticalSection(&cs);
    return 0;
}

unsigned WINAPI ThreadDes(void* arg) {
    int i;
    EnterCriticalSection(&cs);
    for (i = 0; i < 500000; ++i) {
        num -= 1;
    }
    LeaveCriticalSection(&cs);

    return 0;
}
