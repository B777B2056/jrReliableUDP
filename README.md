# jrReliableUDP ![](https://img.shields.io/badge/C%2B%2B-11-brightgreen) ![](https://img.shields.io/badge/OS-Linux-yellowgreen)
参考TCP协议，基于UDP实现的应用层可靠传输协议，传送的是包而非字节流；目的是学习TCP协议工作机制。  
该传输协议实现了如下功能：  
1. 重传机制，包括超时重传和快速重传；  
2. 流量控制，采用滑动窗口和窗口通告机制实现； 
3. 拥塞控制，包括慢启动、拥塞避免和快速恢复。      

## 1 建立连接与断开连接  
### 1.1 建立连接——三次握手
![建立连接](pic/conn.png)  
在代码实现中，为了方便所以把中间的SYN和ACK分开发送的：S端先回复ACK再发送SYN。  
### 1.2 断开连接  ——四次挥手
![断开连接](pic/disconn.png)
## 2 重传机制
### 2.1 超时重传
![超时重传](pic/timeout_retrans.png)  
RTT(Round-Trip Time):发送一个数据包后再收到ACK所经过的时间；  
RTO(Retransmission Timeout):超时重传时间。  
在代码实现中，RTT由ACK报文中携带的时间戳和接收到ACK报文时的时间戳确定，RTO的估计公式取自RFC6298。  
### 2.2 快速重传
![快速重传](pic/fast_retrans.png)  
在代码实现中，当发生快速重传时，发送窗口将回退到丢失的包处，重新发送该包后的所有包。
## 3 流量控制
### 3.1 发送窗口

### 3.2 接收窗口

### 3.3 发送窗口如何根据接收窗口大小进行动态调整  

## 4 拥塞控制
### 4.1 慢启动

### 4.2 拥塞避免

### 4.3 快速恢复

## 5 测试
### 5.1 正常传送1000个包
客户端  
![client1](pic/normal_1000pkgs/client1.png)  
...   
![client2](pic/normal_1000pkgs/client2.png)  
服务端    
![server1](pic/normal_1000pkgs/server1.png)  
...   
![server2](pic/normal_1000pkgs/server2.png)  
### 5.2 快速重传——接收方故意丢弃包2
客户端  
![client](pic/pkg_miss_no2/client.png)
服务端  
![server](pic/pkg_miss_no2/server.png)  
### 5.3 超时重传——接受方第一次收到包2后主线程睡眠5s
客户端  
![client](pic/pk2_delay5s_no2/client.png)
服务端  
![server](pic/pk2_delay5s_no2/server.png) 
