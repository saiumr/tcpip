#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// SIGALRM: 超时
// SIGINT: 用户中断
// SIGCHLD: 子进程终止

void timeout(int sig) {
  if (sig = SIGALRM)
    puts("Time out!");
  alarm(2);  // 预约2秒后产生SIGALRM信号
}

void keycontrol(int sig) {
  if (sig == SIGINT) 
    puts("CTRL+C pressed!");
}

int main(int argc, char* argv[]) {
  int i;
  // 注册信号
  signal(SIGALRM, timeout);
  signal(SIGINT, keycontrol);
  // 预约2秒后产生SIGALRM信号，开始信号
  alarm(2);

  for (i = 0; i < 3; ++i) {
    puts("wait...");
    sleep(100);
  }

  return 0;
}

/*
不干预执行，会输出：
wait...
Time out!
wait...
Time out!
wait...
Time out!

干预执行：
wait...
Time out!
wait...
^CCTRL+C pressed!
wait...
Time out!
*/
