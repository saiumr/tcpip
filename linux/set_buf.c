#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

void error_handling(const char* msg);

int main(int argc, char** argv) {
	int send_buf = 1024*3, recv_buf = 1024 * 3, state;
	int sock_tcp;
	socklen_t len;

	sock_tcp = socket(PF_INET, SOCK_STREAM, 0);
	state = setsockopt(sock_tcp, SOL_SOCKET, SO_SNDBUF, (void*)&send_buf, sizeof(send_buf));
	if (state)
		error_handling("setsockopt(snd) error!");

	state = setsockopt(sock_tcp, SOL_SOCKET, SO_RCVBUF, (void*)&recv_buf, sizeof(recv_buf));
	if (state)
		error_handling("setsockopt(rcv) error!");

  send_buf = 0;
  recv_buf = 0;
	len = sizeof(send_buf);
	state = getsockopt(sock_tcp, SOL_SOCKET, SO_SNDBUF, (void*)&send_buf, &len);
	if (state)
		error_handling("getsockopt(snd) error!");

	len = sizeof(recv_buf);
	state = getsockopt(sock_tcp, SOL_SOCKET, SO_RCVBUF, (void*)&recv_buf, &len);
	if (state)
		error_handling("getsockopt(rcv) error!");

	printf("Set input buffer  size: %d (B)\n", recv_buf);
	printf("Set output buffer size: %d (B)\n", send_buf);

	return 0;
}

void error_handling(const char* msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
