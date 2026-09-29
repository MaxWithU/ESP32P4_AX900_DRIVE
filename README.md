# ESP32P4 AX900 Driver

面向 ESP-IDF 的 AX900 USB Wi-Fi 驱动移植，已在 **M5Stack Tab5 / ESP32-P4** 上验证双频扫描和 5 GHz 企业网络连接。

**实机已通过 5 GHz PEAP/MSCHAPv2 认证、WPA2 密钥协商、AX900 DHCP 获取及网关 ICMP 通信验证，实体键盘输入、Wi-Fi 配置记忆和重启自动重连也已实机通过。三轮共 9 次受控故障恢复均重新获得 DHCP，恢复后网关 Ping 为 43/45；USB 恢复约 8 秒，手动断开保持离线通过。另已验证 5 GHz WPA2-Personal 热点的四次握手、DHCP、配置保存和网关 5/5 回复。驱动仍处于实验阶段，长期稳定性与兼容性覆盖尚未完成。**

## 已实现与验证

- USB 模式切换：`a69c:5721` 存储模式 → `a69c:8d80` 固件加载 → `a69c:8d81` 无线运行。
- 固件上传、补丁表加载、射频初始化、双频被动扫描和重复扫描。
- 网卡未烧录有效 MAC 时，基于 ESP32-P4 硬件地址派生稳定的本地单播地址；不写入网卡 eFuse。连接前在选定信道定向探测目标 AP。
- 最多缓存 64 个 BSSID；界面按原始 SSID 和安全策略合并重复 Wi-Fi，显示频段、AP 数和 RSSI，优先选择 5 GHz。隐藏网络及不同认证策略保持独立。
- 官方 M5Tab5 UserDemo 接入补丁：5 GHz 优先列表、密码/企业账号输入、连接/断开和 USB 串口诊断。
- Tab5Keyboard 实体键盘：I²C 0x6D、Aa / Sym、字母数字符号、退格、Tab 焦点切换、方向键和 Enter 操作。
- 成功联网后记忆最多 4 个网络，启动和意外掉线自动重连；支持逐个删除和自动连接开关。可从 SD 导入企业 CA，保护加密存储为需提前配置的可选项。
- **Connection test** 面板：USB / 关联 / 认证 / DHCP 状态、IP / 网关 / 子网掩码 / DNS、收发计数及 5 次网关 Ping 的逐次延迟、平均延迟和丢包率。
- 上游 Hostap WPA2/EAP 状态机、mbedTLS PEAP TLS 1.2 桥接、USB 数据队列及独立 AX900 `esp_netif`。
- 在 5260 MHz（信道 52）完成 PEAP/MSCHAPv2 认证、单播/组播密钥安装及独立 AX900 接口 DHCP 获取；通过绑定该接口的 ICMP 请求收到网关回复。
- 两次连续无账号关联测试通过，覆盖断开后接口重建和重新扫描。实机扫描曾缓存 64 个热点，其中 45 个为 5 GHz；数量随环境变化，64 为缓存上限。
- USB 报文解析通过 ASan / UBSan 检查，包括实机报文、截断、聚合及 100,000 组异常输入。

环境：ESP-IDF **5.5.2**、ESP32-P4 rev v1.3、16 MB Flash、Tab5 V3。AX900 芯片寄存器为 `0xe1078820`，运行固件为 `0x06090100`。其他同名 AX900 产品、USB ID 和芯片版本尚未验证；产品名称或 USB ID 不能单独确定芯片子型号。详见 [验证记录](VALIDATION.json)。

## 获取与构建

先安装并激活 [ESP-IDF 5.5.2](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/get-started/index.html)，然后：

```sh
git clone https://github.com/MaxWithU/ESP32P4_AX900_DRIVE.git tab5-ax900
cd tab5-ax900
python3 tools/fetch_firmware.py
idf.py set-target esp32p4
idf.py build
```

原厂固件从上游固定提交下载，逐文件校验长度和 SHA-256；清单见 [firmware/SOURCE.json](components/ax900/firmware/SOURCE.json)。已有匹配文件会直接复用。离线构建前可自行放入清单中的文件，再执行 `python3 tools/fetch_firmware.py --verify-only`。

仓库根目录是一个可独立构建的 **Tab5 串口扫描示例**。启动后会初始化 USB-A 供电、USB Host 和 AX900，并自动扫描。此示例没有屏幕界面；官方界面接入见下节。

用于专用测试板、允许替换当前分区布局时：

```sh
# 按系统修改端口，例如 Linux 的 /dev/ttyACM0。
idf.py -p /dev/cu.usbmodem1101 flash monitor
```

完整 `flash` 会写入本示例的 bootloader 和分区表。需要保留已有固件时，应先备份完整 Flash；本示例不应通过完整 `flash` 覆盖正在使用的官方分区布局。设备识别、备份和恢复步骤见 [烧录说明](docs/flashing.md)。

## 在其他 ESP-IDF 工程使用组件

将 `components/ax900` 放入工程组件目录，或通过 `EXTRA_COMPONENT_DIRS` 引用。先获取其 `firmware/` 依赖，再构建。

应用负责板级 USB 供电、安装 USB Host，以及调用 `usb_host_lib_handle_events()` 的 Host 事件任务。驱动只注册自己的 USB 客户端，可与已有 HID 客户端共用 Host。

```c
#include "ax900.h"

// 在 USB Host 和板级供电准备好后调用一次。
ESP_ERROR_CHECK(ax900_start());

// 周期轮询使用紧凑快照，扫描完成后再单独读取 AP 列表。
ax900_link_status_t status;
ax900_get_link_status(&status);
if (status.ready && !status.scanning) {
    esp_err_t result = ax900_request_scan();
    // result == ESP_OK 表示请求已排队；完成后使用 ax900_get_scan_results() 读取扫描结果。
}
```

Tab5 USB-A 供电涉及 GPIO31/32 上的 I2C 扩展器 `0x44`、P3；独立示例通过读改写保留其他引脚状态。其他 ESP32-P4 板需替换板级初始化。默认使用 CN 信道集合：2.4 GHz 1–13；5 GHz 36/40/44/48/52/56/60/64/149/153/157/161/165，采用被动扫描。

## 连接接口（实验阶段）

`ax900_connect(&ap, password)` 支持开放网络和 WPA2-PSK/CCMP；必须从扫描结果选择 BSSID。
`ax900_connect_peap(&ap, &config)` 支持 WPA2 Enterprise 的 PEAP/MSCHAPv2：

```c
ax900_peap_config_t config = {
    .username = locally_entered_username,
    .password = locally_entered_password,
    .ca_cert_pem = trusted_ca_pem,
    .server_name = authentication_server_dns_name,
    .allow_unverified_server = false,
};
esp_err_t result = ax900_connect_peap(&ap, &config);
```

字符串会复制到驱动 RAM；认证并获得 DHCP 地址后，在设备 `ax900_wifi` NVS 命名空间保存最多 4 个网络的账号、密码和证书策略。失败尝试不会覆盖已保存配置，诊断关联不保存；断开后清除 RAM 中的连接请求。启动扫描后优先重连最近成功的网络，同名网络优先选 5 GHz。手动断开后保持断开；临时故障有限退避重试，认证失败暂停自动尝试。CA 校验还要求设备有正确时间。

`ax900_connect_saved(&ap)` 使用已保存配置连接，`ax900_has_saved(&ap)` 只返回是否已保存，`ax900_forget_saved()` 清除 AX900 保存记录。没有导出密码的接口。官方 NVS 默认未启用 Flash/NVS 加密，因此记录是设备本地持久存储，并非加密保险库；日志、快照和 Git 均不记录凭据。
若用户明确选择不校验服务器，省略 CA/域名并显式设置 `allow_unverified_server=true`；这种模式无法验证服务器身份，账号信息可能被冒充热点窃取。官方示例窗口提供有明确标记的“不校验证书”选项；界面支持 SD 卡 `/sd/` 下的 PEM CA 文件（最多 8192 字节）和服务器 DNS 名；API 同样支持。

当前限制：CCMP、TLS 1.2，不支持 WPA3、TKIP、强制 PMF、EAP-TLS、自动漫游/预认证或密码变更。扫描期间不能连接，连接期间须先断开才能扫描。
上游认证核心已统一使用组件私有符号前缀，`tools/supplicant_symbols.py --check` 可检查构建产物是否遗漏隔离。官方内置 Wi-Fi 并行传输场景仍需单独实测。

`associated` 仅表示无线关联；`authenticated` 表示 WPA2 密钥安装/控制端口开放；只有 `has_ip` 才表示 AX900 获得 DHCP 地址。官方演示中另一个 Wi-Fi AP 的 `192.168.4.1` 不能作为 AX900 已联网的证据。
`ax900 associate` 只测试无线关联，不发送用户名或密码，不开放数据端口，成功后 10 秒自动断开。
`ax900 probe` 与界面 **Run test** 使用同一个异步测试模块，通过 AX900 接口向 DHCP 网关发送 5 次 ICMP 请求。测试期间禁止重复启动；断开或重连后旧结果标记为过期。网关 Ping 只验证局域网可达性。官方图形固件通过 `xTaskCreateWithCaps()` 将 Ping 任务栈放入 PSRAM，避免显示和 PEAP 占用内部 RAM 后创建任务失败；使用仓库内固定到 ESP-IDF 5.5.2 的 ICMP socket 实现，不再包含 SDK 私有源文件。

## 断线与 USB 恢复

意外断线、USB 拔出再接入、USB 接收/发送故障及 DHCP 最终超时，会使用已保存的登录信息尝试恢复。选定网络后只重连相同原始 SSID 和安全类型，不会切换到另一个已保存网络。每次恢复先扫描，优先匹配的 5 GHz AP。

- Wi-Fi 重试间隔为 1、2、4、8、16 秒，最多 5 次；扫描和认证耗时另计，启动时的自动连接也计入本轮次数。连接连续稳定 60 秒后重置次数，短时间反复掉线不会无限重试。
- 认证失败或认证阶段超时暂停自动重试，重新选择网络才恢复。手动 Disconnect、Forget saved 和仅关联诊断也会禁止自动重连；USB 热插拔不会解除这个选择。重启后恢复正常的已保存网络启动策略。
- DHCP 首次等待 30 秒，随后最多重启 DHCP 两次；仍无地址时断开并进入上述已保存网络恢复流程。未成功保存的新网络不会在失败后持久化凭据。
- USB 传输等待最多 3 秒，取消额外等待最多 500 毫秒。尚未返回的回调继续持有堆上的传输对象；工作任务继续处理事件，不会释放 USB 主机仍在使用的内存。若底层一直不归还传输，提示重新插拔或重启，最多保留一个设备的未完成传输。
- USB 初始化/传输恢复最多 3 次，间隔 1、2、4 秒；普通取消/事务错误保留端点数据序号；设备 STALL 使用标准 CLEAR_FEATURE 并重新同步端点。运行中恢复跳过已经完成的固件栈启动步骤，无线层与网络接口全部初始化成功后才公布 ready。连续运行 60 秒重置 USB 次数，达到上限需重新插拔。

`ax900_status_t` 提供重连开关、等待状态、次数、下一次等待毫秒数，以及 USB 故障/恢复计数和最近错误。上限或暂停状态通过状态文本显示。

开发固件可在 menuconfig 的 AX900 菜单启用 `CONFIG_AX900_FAULT_INJECTION`，增加本地命令 `ax900 fault-rx`、`ax900 fault-link`、`ax900 fault-dhcp`。默认关闭；RX 测试实际取消 USB IN 传输，link 测试主动断开后进入恢复流程，DHCP 测试触发最终超时分支。它们不等价于物理拔插、AP 断电或真实 DHCP 服务器故障。

```sh
# 在本机已安装 pyserial 的环境运行，仅适用于启用故障注入的开发固件
python3 tools/test_recovery_hardware.py --port /dev/cu.usbmodem1101 --rounds 3
```

该脚本记录本机 `logs/` 下的原始串口输出，终端只显示无账号、SSID、MAC、IP 的汇总；最后验证手动断开并保持离线。运行日志、截图、抓包和本机验证明细不上传 GitHub 或其他云端。Git 忽略 `logs/`、`*.log`、`*.log.*`、`*.pcap`、`*.pcapng`；不要强制添加这些文件。

## 官方界面与诊断

[官方 M5Tab5 UserDemo 接入说明](docs/official-userdemo.md) 提供固定基线提交及接入补丁。补丁包含 LVGL 界面和以下串口命令：

```text
ax900 status
ax900 scan
ax900 open
ax900 select <SSID>
ax900 associate [frequency_MHz] <SSID>
ax900 disconnect
ax900 saved <SSID>
ax900 stop
ax900 start
ax900 check <kind:0..5> <host> <port> [DNS_IP]
ax900 check-status
ax900 check-cancel
ax900 probe
ax900 test
ax900 test-run
ax900 test-status
ax900 keyboard
ax900 snapshot
```

`capture_serial.py` 用于日志采集；`serial_diag.py` 可发送命令并解码设备生成的 LVGL 快照。后者仅用于应用了接入补丁的官方固件，独立串口示例不实现这些命令。依赖可安装到单独的 Python 环境：

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
mkdir -p logs
.venv/bin/python serial_diag.py logs/ui.log --port /dev/cu.usbmodem1101 \
  --command 'ax900 open' --command 'ax900 status' --command 'ax900 snapshot' \
  --seconds 48 --png logs/screen.png
```

macOS 重开 CDC 串口可能使设备复位；脚本默认等待 16 秒以避开启动动画。日志和快照含附近网络信息，默认保存在 Git 忽略的 `logs/` 目录。凭据输入窗口打开时拒绝截图；串口键盘诊断只返回是否连接，不输出按键内容。

## 主机测试

统一执行本地 ASan/UBSan 回归（无需设备、无日志上传）：

```sh
python3 tools/test_local.py \
  --official ../Tab5/M5Tab5-UserDemo \
  --mbedtls "$IDF_PATH/components/mbedtls/mbedtls"
```

测试覆盖扫描分组、DNS、国家信道、包池、配置及受保护存储、测试 API、USB 生命周期、实体键盘、TLS 与真实 Hostap WPA2 握手；缺少可选依赖会明确显示 SKIP。独立固件和官方固件构建仍须分别执行。新增 API、性能测试步骤及验证边界见 [开发与测试说明](docs/driver-development.md)。

恢复测试直接编译生产 USB 工作任务，模拟 USB 主机的传输所有权，覆盖迟到回调、取消不归还、RX 错误/提交失败、初始化清理、重试上限、网络选择和用户取消：

```sh
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-misleading-indentation \
  -fsanitize=address,undefined -I tests/recovery_host -I tests/profile_host \
  -I tests/probe_host -I components/ax900/include -I components/ax900 \
  tests/recovery_test.c components/ax900/ax900_metrics.c -o /tmp/ax900-recovery-test
/tmp/ax900-recovery-test
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-misleading-indentation \
  -fsanitize=address,undefined -I tests/recovery_host -I tests/probe_host \
  -I components/ax900/include -I components/ax900 \
  tests/wifi_recovery_test.c -o /tmp/ax900-radio-recovery-test
/tmp/ax900-radio-recovery-test
```

本地记忆与键盘映射测试（键盘需先应用官方补丁）：

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I tests/profile_host -I tests/probe_host -I components/ax900/include -I components/ax900 \
  tests/profile_test.c components/ax900/ax900_profile.c -o /tmp/ax900-profile-test
/tmp/ax900-profile-test
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I ../Tab5/M5Tab5-UserDemo/platforms/tab5/main/hal/components \
  tests/keyboard_test.cpp -o /tmp/tab5-keyboard-test
/tmp/tab5-keyboard-test
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I tests/keyboard_host -I tests/probe_host \
  -I ../Tab5/M5Tab5-UserDemo/platforms/tab5/main/hal/components \
  tests/keyboard_input_test.cpp \
  ../Tab5/M5Tab5-UserDemo/platforms/tab5/main/hal/components/tab5_keyboard.cpp \
  -o /tmp/tab5-keyboard-input-test
/tmp/tab5-keyboard-input-test
```

测试模块回归测试覆盖接口绑定、重复请求、部分/全部丢包、零毫秒延迟、断线结果失效与失败清理：

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I tests/probe_host -I components/ax900/include \
  tests/probe_test.c components/ax900/ax900_probe.c -o /tmp/ax900-probe-test
/tmp/ax900-probe-test
```

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I components/ax900 tests/frame_test.c -o /tmp/ax900-frame-test
/tmp/ax900-frame-test
```

企业网络实测使用用户显式选择的不校验证书模式；CA/域名校验通过 TLS 主机测试，尚未在真实企业网络验证。5 GHz WPA2-Personal 热点已通过 RSNXE 修复后的握手、DHCP 和网关测试；开放网络、重新认证和长期热插拔稳定性仍待验证。协议及固件兼容性说明见 [固件说明](components/ax900/firmware/README.md)。

官方动画库修复的回归测试（先按接入说明应用依赖补丁）：

```sh
c++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
  -I ../Tab5/M5Tab5-UserDemo/dependencies/smooth_ui_toolkit/src \
  tests/spring_initialization_test.cpp \
  ../Tab5/M5Tab5-UserDemo/dependencies/smooth_ui_toolkit/src/animation/generators/spring/spring.cpp \
  -o /tmp/ax900-spring-test
/tmp/ax900-spring-test
```

接收解码测试：

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I components/ax900 tests/rx_test.c -o /tmp/ax900-rx-test
/tmp/ax900-rx-test
```

TLS 桥接测试（本机需 CMake、C 编译器、OpenSSL 和 mbedTLS 3.6.x 源码；已在 macOS 上运行）：

```sh
MBEDTLS_SOURCE="$IDF_PATH/components/mbedtls/mbedtls" sh tests/run_tls_test.sh
```

测试与真实内存 TLS 服务端交换分片握手和双向应用数据，比较 EAP 密钥导出，并确认错误域名、不受信任 CA 会失败。临时证书和测试密钥仅在临时目录生成。它不替代完整 PEAP/MSCHAPv2、WPA2 或实机无线测试。

## 许可与来源

Hostap 认证核心采用 BSD-3-Clause，原始许可随源码保留。主机驱动、mbedTLS 适配及独立示例采用 Apache-2.0，协议结构参考 `canmv-k230/rtsmart`，保留 [NOTICE](NOTICE) 和来源提交。官方 UserDemo 接入补丁遵循 MIT，见 [patches/LICENSE.MIT](patches/LICENSE.MIT)。原厂固件及配置遵循各自的供应商条款，通过下载脚本获取，不属于上述源码许可。
