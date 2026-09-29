# 驱动开发与本地测试

## 状态与生命周期

`ax900_get_link_status()` 提供紧凑状态。`ax900_get_scan_results()` 单独读取原始 BSSID；`ax900_group_networks()` 按原始 SSID 与安全策略合并显示，选择更优的 5 GHz/RSSI 代表。原始扫描上限仍为 64 个 AP。

应用负责 USB Host、板级供电与 Host 事件任务。`ax900_start()` 不提前占用应用的默认 ESP 事件循环；注册 `AX900_EVENTS / AX900_EVENT_STATE_CHANGED` 可监听无账号、SSID、地址的状态变化。队列满时不阻塞 USB 工作任务，事件丢弃计数可查 `ax900_get_metrics()`。

`ax900_stop()` 是异步操作：先关闭应用 socket，等待 `AX900_STOPPED` 后才能重新 start。驱动等待 USB 回调、测试结束和上层 RX pbuf 归还；不会释放主机还拥有的缓冲。停止会释放网络接口、包池和已归还的传输缓冲，但保留 USB 客户端、工作任务及当前适配器身份以继续监听拔插。恢复同一设备使用热初始化，物理移除会使旧状态失效；新设备才进行冷初始化，避免重复射频校准超时。首次注册会枚举已连接设备。此接口不是整个 USB Host 的反初始化，Host 卸载需由应用整体关闭流程处理。底层取消一直不完成时会保持 STOPPING，需物理重新插拔或设备重启。

## 包池与指标

RX 32、TX 24、EAPOL 8 个独立预分配槽，优先使用 PSRAM；EAPOL 有独立队列，普通数据不会耗尽其预留空间。RX 原缓冲交给 esp_netif，其 free 回调归还包池。数据 TX 使用 4 个可复用 USB DMA 槽，认证/控制消息保留同步发送。统计区分队列满、池耗尽、旧连接数据、非法接收及 USB 发送失败。

## 测试目标与吞吐

```sh
# 在同一局域网电脑，绑定该电脑的实际 LAN 地址。合成数据，不记录请求。
python3 tools/benchmark_peer.py --bind <LAN_IP> --port 5201
# 单独占用串口，避免与其他 monitor 同时运行。
python3 tools/benchmark_device.py --host <LAN_IP> --rounds 3 --label local-benchmark
```

DNS/TCP/HTTP/上传/下载/UDP 由 `ax900_test.h` 暴露，目标、端口、DNS、路径仅存 RAM。每次测试总时限 1–60 秒，吞吐数据 1 KiB–8 MiB。上传只有收到对端实际字节数确认才算成功，下载验证合成内容；超时结果不能当作完整吞吐。UDP 是 100 个 1200 字节包的往返回复率，不能当作单向丢包率。内部 RAM 最低值为启动以来的历史最低值，不是单次测试独占的最低值。DNS 使用 AX900 的 UDP socket，所有模式同时绑定接口及源地址。

自动 DNS 要求 `CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF=y`，避免内置 Wi-Fi 的 DHCP 替换全局 DNS 后影响 AX900 测试。自定义集成缺少该选项时不读取全局 DNS，须显式填写 DNS IPv4 才能测试域名；字面 IPv4 目标不受影响。

HTTP 检查最终状态行，跳过最多 8 个 1xx 响应及其头部；最终 2xx/3xx 通过，4xx/5xx 失败，未请求的 101 协议升级也失败。读取受总时限、8 KiB 总头部预算及 511 字节单行上限约束，不验证网页内容或 HTTPS。网关 Ping 则为每个请求设置 1500 ms 单调时钟总截止时间，无关 ICMP 不延长等待；停止检查间隔最多 100 ms 加调度延迟。

比较优化前后必须使用同一网络、电脑对端和测试大小；记录 AP/无线环境变化可能带来的偏差。原始串口、构建、烧录和测试明细只放本地忽略目录 `logs/`；仓库仅记录脱敏结论。

## 保存网络与可选保护存储

`ax900_list_saved()` 只导出 SSID/认证类型等元数据；`ax900_forget_network()` 和 `ax900_set_auto_connect()` 异步排队，结果从 `profile_error` 与再次读取元数据确认。全部关闭 Auto connect 时启动不触发自动恢复扫描。删除当前记录不切断当前连接，也不会立即把它保存回去。

默认沿用设备的 `nvs` 分区，不自动升级安全配置、不清空历史记录。`ax900_profiles_encrypted()` 仅确认下述专用保护模式，返回 false 不等于整个应用必然未加密。

保护模式要求应用已完成 Flash Encryption 配置、存在已配置密钥且受 flash 加密保护的 NVS keys 分区，以及专供驱动的 `ax900_nvs` 数据分区。启用 `CONFIG_NVS_ENCRYPTION` 与 `CONFIG_AX900_ENCRYPTED_PROFILES`，设置 `CONFIG_AX900_PROFILE_PARTITION` 和 `CONFIG_AX900_PROFILE_KEYS_PARTITION`。专用数据分区必须尚未被应用初始化，禁止将默认 `nvs` 用作保护分区，避免 ESP-IDF 对已初始化明文存储返回成功造成误判。

驱动只读现有密钥并初始化专用加密分区，不生成密钥、不烧 eFuse、不擦除/迁移配置，缺少条件时拒绝保存，无明文回退。现有设备没有执行加密部署；旧普通配置不会自动复制到专用分区，部署和备份需另行设计。CA 导入和 TLS 信任校验的主机回归通过，真实企业 CA 网络尚未验证。

## 兼容性边界

`ax900_configure_radio()` 仅在 STOPPED 状态接受 CN/US/EU/JP 与 DFS 扫描开关，使用原厂已包含的信道子集；扫描和连接使用同一过滤规则。国家配置不代表已获得当地射频认证，其他信道未开放。

`ax900_get_capabilities()` 区分固件声明和驱动已实现能力。当前固件报告 MFP 位，但驱动仍不实现 PMF、WPA3-SAE 和无缝漫游，不能仅据固件位对外声称支持。支持 WPA2-Personal/CCMP、PEAP/MSCHAPv2、开放网络 API；开放网络实机用例尚待可用测试路由器。RSNXE 扫描信息现会传给 Hostap，避免 WPA2 热点第三次握手一致性检查失败。

2026-09-29 物理拔插测试已捕获两次 USB 重新枚举，均在不重启 Tab5 的情况下，自动使用已保存配置恢复认证和 DHCP；从首次重新枚举至 DHCP 分别为 10.162 秒和 9.948 秒，不包含网卡拔出等待时间。之后 Mac 诊断连接中断，同次拔插后的网关测试尚待补齐；重新启动固件后的 Ping 不计入热插拔验证。

长期热插拔、AP 断电、真实 DHCP 故障、企业周期重新认证及 24 小时持续运行仍需独立实机记录。模拟故障通过不等于这些场景已通过。
