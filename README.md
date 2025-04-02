# proxy
# TCP端口转发工具
# 使用方法：
```ssh
mkdir build
cd build
cmake ..
make
./tcpzhuanfa 80 remote_ip remote_port
```

ex:
本地80端口转发到192.168.137.2端口：
```ssh
./tcpzhuanfa 80 192.168.137.2 80
```
