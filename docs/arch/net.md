# 网络栈

## 分层

| 层 | 职责 | 本项目的验证目标 |
|---|---|---|
| 链路层 | 帧收发、MAC 寻址 | 能与 QEMU 用户模式网络互通 |
| ARP | IP → MAC 解析 | 能解析网关 MAC |
| IP | 寻址与路由 | 能构造并校验首部校验和 |
| ICMP | 控制报文 | 能回应 ping |
| UDP | 无连接传输 | 能收发包 |
| TCP | 可靠传输 | 三次握手、数据有序、重传 |

## 网卡驱动

QEMU 默认提供 `e1000` 或 `rtl8139`，都是 PCI 设备，走前面 [驱动模型](./drivers) 的 probe/init 流程。

关键结构：

- **收发环（ring）**：物理地址连续的描述符数组，每个描述符指向一个缓冲区
- **寄存器基址**：从 BAR 取 MMIO 地址
- **中断或轮询**：教学阶段用轮询最省事，代价是 CPU 空转

::: tip 先跑通"收到任何一帧"
第一步不要急着解析协议。只要 DMA 能把网卡收到的原始字节搬进内存并打印前 14 个字节（以太网首部），链路就通了。之后再逐层加上 ARP、IP。
:::

## 校验和

IP 首部校验和是最容易写错的一处：

```c
uint16_t checksum(const void *data, size_t len) {
    uint32_t sum = 0;
    const uint16_t *p = data;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}
```

三个常见错误：

| 错误 | 表现 |
|---|---|
| 忘记把结果取反 | 对方全部丢包 |
| 字节序搞错（`htons` 漏加） | 校验和随机校验失败 |
| 奇数长度时忘记补一个字节 | 最后 1 字节被丢弃 |

## TCP 状态机

TCP 的复杂度全在状态迁移。最小实现需要覆盖：

```
CLOSED → SYN_SENT → ESTABLISHED → FIN_WAIT_1 → FIN_WAIT_2 → TIME_WAIT → CLOSED
                 ↘ SYN_RCVD ↗
```

必须实现的部分：

- 序号与确认号管理
- 接收窗口（`window` 字段不能恒为 0）
- 超时重传（哪怕只是一个固定 RTO）
- 乱序报文处理（最简单：缓存或不缓存但正确 ACK）
- 校验和覆盖伪首部（源 IP、目的 IP、协议号、TCP 长度）

## 调试手段

没有抓包工具时，内核侧打印就够了。建议打印格式：

```
[eth] rx len=98 dst=52:54:00:12:34:56 src=... type=0x0800
[ip ] src=10.0.2.15 dst=10.0.2.2 proto=1 ttl=64 csum=ok
[icmp] type=8 code=0 seq=1
```

QEMU 侧可以同时抓包交叉验证：

```bash
qemu-system-i386 -drive format=raw,file=cunos.img \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -object filter-dump,id=f1,netdev=n0,file=nettrace.pcap
```

生成的 `nettrace.pcap` 可以直接用 Wireshark 打开，与内核日志逐帧对照。

## 验证清单

- [ ] 收到第一帧并正确解析以太网首部
- [ ] ARP 请求发送后收到网关回应
- [ ] `ping 10.0.2.2` 能收到 ICMP 回显应答
- [ ] UDP 回环收发一致
- [ ] TCP 三次握手完成，且能正确关闭
