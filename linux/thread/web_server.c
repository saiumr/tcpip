// web_server.c (Linux 版本)
// 制作web服务端（只处理GET请求）
// 多线程并发处理，使用 recv/send 同步读写
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>          // for tolower
#include <sys/socket.h>     // socket, bind, listen, accept, send, recv, shutdown
#include <netinet/in.h>     // sockaddr_in, htons, htonl, INADDR_ANY
#include <arpa/inet.h>      // inet_ntoa
#include <unistd.h>         // close, read, write
#include <pthread.h>        // 多线程
#include <sys/stat.h>       // stat
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#ifndef WEB_ROOT_PATH
#define WEB_ROOT_PATH ""    // 可编译时定义，如 -DWEB_ROOT_PATH="res/"
#endif

#define BUF_SIZE 2048
#define BUF_SMALL 100
#define DEFAULT_PORT 80

static const char* kUnit[5] = {"B", "KB", "MB", "GB", "TB"};
typedef struct {
    long long size;
    double visual_size;
    const char* uint;
} FileSize;

// 函数声明
void* RequestHandler(void* arg);
char* ContentType(const char* filename);
void SendData(int sock, const char* ct, char* filename);
void SendErrorMsg(int sock);
void ErrorHandling(char* message);
void get_file_size(FileSize *fs, const char* filepath);
void log_message(int sock, const char *message);

int main(int argc, char* argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;
    pthread_t tid;

    if (argc > 2) {
        printf("Usage: %s <port> (port is optional, default %d)\n", argv[0], DEFAULT_PORT);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);

    // 创建 socket
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        ErrorHandling("socket() error!");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (argc == 1)
        serv_addr.sin_port = htons(DEFAULT_PORT);
    else
        serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        ErrorHandling("bind() error!");
    if (listen(serv_sock, 5) == -1)
        ErrorHandling("listen() error!");

    printf("Web server started on port %d\n", ntohs(serv_addr.sin_port));

    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if (clnt_sock == -1) {
            perror("[error]: accept failed, try again.");
            continue;
        }
        log_message(clnt_sock, "[info]: connection request.");

        // 创建线程处理请求，并分离（自动回收资源）
        if (pthread_create(&tid, NULL, RequestHandler, (void*)(intptr_t)clnt_sock) != 0) {
            log_message(clnt_sock, "[error]: pthread_create failed");
            close(clnt_sock);
            continue;
        }
        pthread_detach(tid);   // 线程结束后自动回收
    }

    close(serv_sock);
    return 0;
}

void ErrorHandling(char* message) {
    perror(message);
    exit(1);
}

void get_file_size(FileSize *fs, const char* filepath) {
    struct stat statbuf;
    if (stat(filepath, &statbuf) != 0) {
        perror("check file failed");
        exit(1);
    }

    long long filesize = statbuf.st_size;
    fs->size = filesize;
    if (filesize < (1LL << 10)) {
        fs->visual_size = (double)filesize;
        fs->uint = kUnit[0];
    } else if (filesize < (1LL << 20)) {
        fs->visual_size = (double)filesize / (1LL << 10);
        fs->uint = kUnit[1];
    } else if (filesize < (1LL << 30)) {
        fs->visual_size = (double)filesize / (1LL << 20);
        fs->uint = kUnit[2];
    } else if (filesize < (1LL << 40)) {
        fs->visual_size = (double)filesize / (1LL << 30);
        fs->uint = kUnit[3];
    } else {
        fs->visual_size = (double)filesize / (1LL << 40);
        fs->uint = kUnit[4];
    }
}

char* ContentType(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "text/plain";
    ext++;

    char ext_lower[16] = {0};
    size_t i;
    for (i = 0; i < sizeof(ext_lower) - 1 && ext[i]; i++) {
        ext_lower[i] = tolower((unsigned char)ext[i]);
    }
    ext_lower[i] = '\0';

    if (strcmp(ext_lower, "html") == 0 || strcmp(ext_lower, "htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(ext_lower, "css") == 0)
        return "text/css; charset=utf-8";
    if (strcmp(ext_lower, "js") == 0)
        return "application/javascript; charset=utf-8";
    if (strcmp(ext_lower, "json") == 0)
        return "application/json; charset=utf-8";
    if (strcmp(ext_lower, "xml") == 0)
        return "application/xml; charset=utf-8";

    if (strcmp(ext_lower, "png") == 0)
        return "image/png";
    if (strcmp(ext_lower, "jpg") == 0 || strcmp(ext_lower, "jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext_lower, "gif") == 0)
        return "image/gif";
    if (strcmp(ext_lower, "svg") == 0)
        return "image/svg+xml";
    if (strcmp(ext_lower, "ico") == 0)
        return "image/x-icon";
    if (strcmp(ext_lower, "webp") == 0)
        return "image/webp";

    if (strcmp(ext_lower, "woff") == 0)
        return "font/woff";
    if (strcmp(ext_lower, "woff2") == 0)
        return "font/woff2";
    if (strcmp(ext_lower, "ttf") == 0)
        return "font/ttf";

    if (strcmp(ext_lower, "mp4") == 0)
        return "video/mp4";
    if (strcmp(ext_lower, "mp3") == 0)
        return "audio/mpeg";

    return "application/octet-stream";
}

void log_message(int sock, const char *message) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    char ip[INET_ADDRSTRLEN] = "unknown";

    // 获取客户端 IP（如果失败则显示 unknown）
    if (getpeername(sock, (struct sockaddr*)&addr, &len) == 0) {
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    }

    // 获取当前时间
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *tm_info = localtime_r(&now, &tm_buf);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    // 输出到 stderr（无缓冲，立即写入）
    fprintf(stderr, "[%s] (%s) \"%s\"\n", time_buf, ip, message);
}

void* RequestHandler(void* arg) {
    int clnt_sock = (int)(intptr_t)arg;
    char buf[BUF_SIZE];
    char method[BUF_SMALL];
    char ct[BUF_SMALL];
    char filename[8192];       // 扩大缓冲区
    char temp[BUF_SMALL];

    int recv_len = recv(clnt_sock, buf, BUF_SIZE, 0);
    if (recv_len <= 0) {
        log_message(clnt_sock, "[error]: receive length <= 0.");
        close(clnt_sock);
        return NULL;
    }
    if (strstr(buf, "HTTP/") == NULL) {
        log_message(clnt_sock, "[error]: req is not HTTP.");
        SendErrorMsg(clnt_sock);
        return NULL;
    }

    // 改用线程安全版本的strtok_r
    char* saveptr;
    char* token = strtok_r(buf, " ", &saveptr);
    if (token == NULL) {
        log_message(clnt_sock, "[error]: could not decode first token.");
        SendErrorMsg(clnt_sock);
        return NULL;
    }
    strcpy(method, token);
    if (strcmp(method, "GET") != 0) {
        log_message(clnt_sock, "[error]: req is not GET.");
        SendErrorMsg(clnt_sock);
        return NULL;
    }

    token = strtok_r(NULL, " ", &saveptr);  // 接着分割
    if (token == NULL) {
        log_message(clnt_sock, "[error]: could not decode second token.");
        SendErrorMsg(clnt_sock);
        return NULL;
    }
    strcpy(temp, token);
    if (strcmp(temp, "/") == 0) {
        snprintf(filename, sizeof(filename), "./%s/index.html", WEB_ROOT_PATH);
    } else {
        snprintf(filename, sizeof(filename), "./%s%s", WEB_ROOT_PATH, temp);
    }

    strcpy(ct, ContentType(filename));
    SendData(clnt_sock, ct, filename);

    close(clnt_sock);
    return NULL;
}

void SendData(int sock, const char* ct, char* filename) {
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char server_name[] = "Server: :D server\r\n";
    char content_type[BUF_SMALL];
    char buf[BUF_SIZE];
    FILE* send_file_fp = NULL;

    sprintf(content_type, "Content-type: %s\r\n\r\n", ct);

    if ((send_file_fp = fopen(filename, "rb")) == NULL) {
        log_message(sock, "[error]: open file failed:");
        printf("+- %s\n", filename);
        SendErrorMsg(sock);
        return;
    }

    FileSize fs;
    char content_len[BUF_SMALL];
    get_file_size(&fs, filename);
    sprintf(content_len, "Content-length: %lld\r\n", fs.size);

    send(sock, protocol, strlen(protocol), 0);
    send(sock, server_name, strlen(server_name), 0);
    send(sock, content_len, strlen(content_len), 0);
    send(sock, content_type, strlen(content_type), 0);

    size_t bytes;
    while ((bytes = fread(buf, 1, BUF_SIZE, send_file_fp)) > 0) {
        send(sock, buf, bytes, 0);
    }

    shutdown(sock, SHUT_WR);
    fclose(send_file_fp);
}

void SendErrorMsg(int sock) {
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char server_name[] = "Server: :D server\r\n";
    char content_type[] = "Content-type: text/html\r\n\r\n";
    char content[] = 
    "<html><head>"
    "<meta charset=\"utf-8\">"
    "<title>NETWORK</title></head>"
    "<body><font size=+5>"
    "<br> Something error... 请求到了外星球...距离一光年... "
    "</font></body></html>";

    char content_len[BUF_SMALL];
    sprintf(content_len, "Content-length: %zu\r\n", strlen(content));

    send(sock, protocol, strlen(protocol), 0);
    send(sock, server_name, strlen(server_name), 0);
    send(sock, content_len, strlen(content_len), 0);
    send(sock, content_type, strlen(content_type), 0);
    send(sock, content, strlen(content), 0);

    close(sock);
}
