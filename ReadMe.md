# tcpip project  

Learn TCP/IP programming on linux and windows.  

Blog: [tcp/ip programming](https://saiumr.github.io/2026/04/13/tcp-ip%E7%BC%96%E7%A8%8B/)  

## build && clean  

```bash
cd linux/windows
cmake -S . -B build
cd build
make
```

in `build` directory, execute  

```bash
make clean
```

clean it.  

## deploy web server  

在windows下：  

```bash
cd windows
cmake -S . -B build -G "MinGW Makefiles"
cd build
make
./web_server.exe
```

打开浏览器，访问`127.0.0.1`（如果失败，确认开头是`http`而不是`https`）。  

或者使用linux版本部署到云服务器上：  

```bash
cd windows
cmake -S . -B build
cd build
make
nohup ./web_server > log.txt 2>&1 &
```

随后便可使用**服务器公网ip**访问`web server`主页。  
