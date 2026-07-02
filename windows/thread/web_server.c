// web_server.c
// 制作web服务端（只处理GET请求）
// 客户端先发送请求给服务端，服务端再返回数据，然后断开连接
// 这种模式下，使用多线程并发处理大量请求，使用recv/read同步读写即可
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <ctype.h>   // for tolower
#include <time.h>

#if _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef WEB_ROOT_PATH
#define WEB_ROOT_PATH ""    // value defined in CMakeLists.txt
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

unsigned WINAPI RequestHandler(void* arg);
char* ContentType(const char* filename);
void SendData(SOCKET sock, const char* ct, char* filename);
void SendErrorMsg(SOCKET sock);
void ErrorHandling(char* message);
void get_file_size(FileSize *fs, const char* filepath);
void log_message(SOCKET sock, const char *message);

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET hServSocket, hClntSocket;
    SOCKADDR_IN servAddr, clntAddr;
    int clntAddrSize;

    HANDLE hThread;
    DWORD  dwThreadID;

    // 如果要部署，默认80端口访问，这样浏览器访问时不用指定端口
    if (argc > 2) {
        printf("Usage: %s <port> (port is optional, default value is %d)\n", argv[0], DEFAULT_PORT);
        exit(1);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    ErrorHandling("WSAStartup() error!");

    hServSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (hServSocket == INVALID_SOCKET)
        ErrorHandling("socket() error!");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    // Windows 无 inet_aton，替换为 inet_addr（功能一致）
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (argc == 1) {
        servAddr.sin_port = htons(DEFAULT_PORT);
    } else {
        servAddr.sin_port = htons(atoi(argv[1]));
    }

    if (bind(hServSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
        ErrorHandling("bind() error!");
    if (listen(hServSocket, 5) == SOCKET_ERROR)
        ErrorHandling("listen() error!");

    while (1) {
        clntAddrSize = sizeof(clntAddr);
        hClntSocket = accept(hServSocket, (SOCKADDR*)&clntAddr, &clntAddrSize);
        log_message(hClntSocket, "[info]: connection request.");
        hThread = (HANDLE)_beginthreadex(NULL, 0, RequestHandler, (void*)hClntSocket, 0, (unsigned *)&dwThreadID);
        CloseHandle(hThread);   // 立即释放句柄
    }
    
    closesocket(hServSocket);
    WSACleanup();
    return 0;
}

void ErrorHandling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void get_file_size(FileSize *fs, const char* filepath) {
#ifdef _WIN32
  // Windows 标准方法
  struct __stat64 statbuf;
  int result = _stat64(filepath, &statbuf);
#else
  // POSIX (Linux/macOS) 标准方法
  struct stat statbuf;
  int result = stat(filepath, &statbuf);
#endif

  if (result != 0) {
    perror("check file failed");
    exit(1);
  }

  long long filesize = statbuf.st_size;

  fs->size = filesize;
  int unit_index = 0;
  if (filesize < 1<<10) {  // B
    fs->visual_size = (double)filesize;
    fs->uint = kUnit[0];
  } else if (filesize < 1<<20) {  // KB
    fs->visual_size = (double)filesize / (1<<10);
    fs->uint = kUnit[1];
  } else if (filesize < 1<<30) {  // MB
    fs->visual_size = (double)filesize / (1<<20);
    fs->uint = kUnit[2];
  }  else if (filesize < (long long)1<<40) {  // GB
    fs->visual_size = (double)filesize / (1<<30);
    fs->uint = kUnit[3];
  } else {  // TB
    fs->visual_size = (double)filesize / ((long long)1<<40);
    fs->uint = kUnit[4];
  }
}

// AI 用来写这种硬编码真是太爽了
char* ContentType(const char* filename) {
    // 提取文件扩展名（最后一个点后面的部分）
    const char* ext = strrchr(filename, '.');
    if (!ext) return "text/plain";  // 无扩展名，默认纯文本

    ext++;  // 跳过点

    // 扩展名转小写（便于比较）
    char ext_lower[16] = {0};
    size_t i;
    for (i = 0; i < sizeof(ext_lower) - 1 && ext[i]; i++) {
        ext_lower[i] = tolower((unsigned char)ext[i]);
    }
    ext_lower[i] = '\0';

    // 常见 MIME 类型映射
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

    // 图片类型 —— 你想要的拼接效果
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

    // 字体
    if (strcmp(ext_lower, "woff") == 0)
        return "font/woff";
    if (strcmp(ext_lower, "woff2") == 0)
        return "font/woff2";
    if (strcmp(ext_lower, "ttf") == 0)
        return "font/ttf";

    // 视频 / 音频
    if (strcmp(ext_lower, "mp4") == 0)
        return "video/mp4";
    if (strcmp(ext_lower, "mp3") == 0)
        return "audio/mpeg";

    // 默认二进制流（让浏览器下载）
    return "application/octet-stream";
}

void log_message(SOCKET sock, const char *message) {
    struct sockaddr_in addr;
    int len = sizeof(addr);
    char ip[INET_ADDRSTRLEN] = "unknown";

    // 获取客户端 IP
    if (getpeername(sock, (struct sockaddr*)&addr, &len) == 0) {
        // Windows Vista 及以上支持 inet_ntop，若兼容老系统可用 inet_ntoa
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        // 或使用 inet_ntoa（非线程安全，但单线程调用时无妨）
        // strcpy(ip, inet_ntoa(addr.sin_addr));
    }

    // 获取当前本地时间
    SYSTEMTIME st;
    GetLocalTime(&st);

    // 输出到 stderr（无缓冲，实时显示）
    fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] (%s) \"%s\"\n",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond,
            ip, message);
    fflush(stderr);   // 确保立即写入（可选）
}

unsigned WINAPI RequestHandler(void* arg) {
    SOCKET hClntSocket = (SOCKET)arg;
    char buf[BUF_SIZE];
    char method[BUF_SMALL];  // GET/POST
    char ct[BUF_SMALL];      // ct is Content-Type, text/html;
    char filename[BUF_SMALL];
    char temp[BUF_SMALL];

    int recv_len = recv(hClntSocket, buf, BUF_SIZE, 0);
    if (recv_len <= 0) {
        log_message(hClntSocket, "[error]: receive length <= 0.");
        closesocket(hClntSocket);
        return 1;
    }
    if (strstr(buf, "HTTP/") == NULL) {
        log_message(hClntSocket, "[error]: req is not HTTP.");
        SendErrorMsg(hClntSocket);
        return 1;
    }

    // delims 任意一个碰到就切分，原书是" /"，意即匹配到" "或"/"
    // 改用线程安全版本的strtok_r
    char* saveptr;
    char* token = strtok_r(buf, " ", &saveptr);
    if (token == NULL) {
        log_message(hClntSocket, "[error]: could not decode first token.");
        SendErrorMsg(hClntSocket);
        return 1;
    }
    strcpy(method, token);
    if (strcmp(method, "GET") != 0) {  // not GET req
        log_message(hClntSocket, "[error]: req is not GET.");
        SendErrorMsg(hClntSocket);
        return 1;
    }

    token = strtok_r(NULL, " ", &saveptr);  // 接着分割
    if (token == NULL) {
        log_message(hClntSocket, "[error]: could not decode second token.");
        SendErrorMsg(hClntSocket);
        return 1;
    }
    strcpy(temp, token);
    if (strcmp(temp, "/") == 0) {
        sprintf(filename, "./%s/index.html", WEB_ROOT_PATH);
    } else {
        sprintf(filename, "./%s%s", WEB_ROOT_PATH, temp);
    }
    strcpy(ct, ContentType(filename));  // 区分文件类型
    SendData(hClntSocket, ct, filename);

    closesocket(hClntSocket);
    return 0;
}

void SendData(SOCKET sock, const char* ct, char* filename) {
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char server_name[] = "Server: :D server\r\n";
    // char content_len[] = "Content-length:2048\r\n";
    char content_type[BUF_SMALL];
    char buf[BUF_SIZE];
    FILE* send_file_fp = NULL;

    sprintf(content_type, "Content-type: %s\r\n\r\n", ct);
    // 以二进制打开，确保传输时不会因为特殊字符传输错误，完整传输
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

    // 传输头信息
    send(sock, protocol, strlen(protocol), 0);
    send(sock, server_name, strlen(server_name), 0);
    send(sock, content_len, strlen(content_len), 0);
    send(sock, content_type, strlen(content_type), 0);

    // 传输请求数据
    // 不用fgets，防止特殊字符造成读取终结，用fread放心发二进制文件
    size_t bytes;
    while ((bytes = fread(buf, 1, BUF_SIZE, send_file_fp)) > 0) {
        send(sock, buf, bytes, 0);
    }
    
    // closesocket(sock);  // http响应后断开
    // 不使用暴力关闭，防止缓冲区还有数据没发送
    // 发送FIN包通知浏览器发送完毕
    shutdown(sock, SD_SEND);
    fclose(send_file_fp);
}

void SendErrorMsg(SOCKET sock) {
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char server_name[] = "Server: :D server\r\n";
    // char content_len[] = "Content-length:2048\r\n";
    char content_type[] = "Content-type: text/html\r\n\r\n"; // 头和正文要用空行分隔开
    char content[] = 
    "<html><head>"
    "<meta charset=\"utf-8\">"  // HTML内声明编码，没有的话中文会乱码
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

    closesocket(sock);
}
