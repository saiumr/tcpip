// SyncMutex_win.c
#include <windows.h>
#include <process.h>   // _beginthreadex  _endthreadex
#include <stdio.h>

#define NUM_THREAD 50
unsigned WINAPI ThreadInc(void* arg);
unsigned WINAPI ThreadDes(void* arg);

long long num = 0;
HANDLE hMutex;

int main(int argc, char* argv[]) {
    HANDLE tHandles[NUM_THREAD];
    int i;

    hMutex = CreateMutex(NULL, FALSE, NULL);
    for (i = 0; i < NUM_THREAD; ++i) {
        if (i % 2) {
            tHandles[i] = (HANDLE)_beginthreadex(NULL, 0, ThreadInc, NULL, 0, NULL);
        } else {
            tHandles[i] = (HANDLE)_beginthreadex(NULL, 0, ThreadDes, NULL, 0, NULL);
        }
    }
    WaitForMultipleObjects(NUM_THREAD, tHandles, TRUE, INFINITE);
    CloseHandle(hMutex);
    
    printf("result: %lld\n", num);

    return 0;
}

unsigned WINAPI ThreadInc(void* arg) {
    int i;
    WaitForSingleObject(hMutex, INFINITE);
    for (i = 0; i < 500000; ++i) {
        num += 1;
    }
    ReleaseMutex(hMutex);
    return 0;
}

unsigned WINAPI ThreadDes(void* arg) {
    int i;
    WaitForSingleObject(hMutex, INFINITE);
    for (i = 0; i < 500000; ++i) {
        num -= 1;
    }
    ReleaseMutex(hMutex);

    return 0;
}
