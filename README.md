# CS114 (Minnow) 用户态 TCP/IP 协议栈实现

本项目是斯坦福大学 CS144 (Computer Network) 课程实验的完整实现。该项目从零开始构建了一个运行在用户态的 TCP/IP 协议栈（Minnow 库），实现了从底层链路层（ARP/以太网）到高层传输层（TCP）的完整逻辑。

## 🚀 项目核心模块

本项目共分为 7 个实验任务（Lab 0 - Lab 6），涵盖了现代网络栈的关键组件：

### 1. 基础架构 (Lab 0 - 1)
*   **ByteStream**: 实现了带流量控制的内存字节流。
*   **Reassembler**: 报文重组器，负责处理失序、重复和乱序到达的 TCP 段，将其按顺序拼接回字节流，解决 TCP 的“空洞”问题。

### 2. 传输层 TCP 协议 (Lab 2 - 3)
*   **TCP Receiver**: 负责生成确认号（ackno）和接收窗口（window size），处理序列号与绝对序列号的转换。
*   **TCP Sender**: 实现了具有超时重传（RTO）、流量控制和拥塞窗口管理能力的发送端。
*   **TCPPeer**: 将发送端与接收端组合，实现了完整的 TCP 状态机转换（三次握手与四次挥手）。

### 3. 网络层与链路层 (Lab 5 - 6)
*   **Network Interface**: 实现了以太网上的 IPv4 适配。包含了 **ARP 协议** 的完整实现（请求封装、响应处理和 ARP 映射表的自动缓存/过期）。
*   **Router**: 实现了基于 **最长前缀匹配 (LPM)** 算法的路由器转发逻辑，支持 TTL 处理和多接口报文路由。

## 🛠 构建与编译

项目使用 CMake 构建，建议在 Linux 环境下编译（需支持 C++20）：

```bash
# 配置构建系统
cmake -S . -B build

# 编译项目
cmake --build build -j$(nproc)
```

## 🧪 测试与验证

### 自动测试
运行所有 71 个单元测试和集成测试：
```bash
cd build
ctest -V
```

### 实战演示：连接真实服务器
本项目实现的协议栈可以绕过内核，直接通过虚拟网卡（TUN）与外界通信：

1. **创建虚拟网卡**:
   ```bash
   sudo ./scripts/tun.sh start
   ```

2. **运行应用程序**: 使用你自己写的 TCP 栈模拟一个 Web 浏览器连接服务器：
   ```bash
   ./build/apps/tcp_ipv4 stanford.edu 80
   ```
   *连接成功后，你可以手动输入 HTTP 请求并观察协议栈对数据的处理过程。*

## 📈 性能表现
通过 `speed` 目标检查核心组件（ByteStream 和 Reassembler）的吞吐性能：
```bash
cmake --build build --target speed
```
