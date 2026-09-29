# ESP32P4 AX900 Driver

面向 ESP-IDF 的 AX900 USB Wi-Fi 驱动移植，已在 **M5Stack Tab5 / ESP32-P4** 上验证双频扫描和 5 GHz 企业网络连接。

**实机已通过 5 GHz PEAP/MSCHAPv2 认证、WPA2 密钥协商、AX900 DHCP 获取及网关 ICMP 通信验证。驱动仍处于实验阶段，互联网访问、长期稳定性和吞吐尚未验证。**

## 已实现与验证

- USB 模式切换：`a69c:5721` 存储模式 → `a69c:8d80` 固件加载 → `a69c:8d81` 无线运行。
- 固件上传、补丁表加载、射频初始化、双频被动扫描和重复扫描。
- 网卡未烧录有效 MAC 时，基于 ESP32-P4 硬件地址派生稳定的本地单播地址；不写入网卡 eFuse。连接前在选定信道定向探测目标 AP。
- 最多缓存 64 个 BSSID，提供 SSID、频率、RSSI 和加密标志。
- 官方 M5Tab5 UserDemo 接入补丁：5 GHz 优先列表、密码/企业账号输入、连接/断开和 USB 串口诊断。
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

// 状态结构约 22 KB，建议使用静态或堆存储。
static ax900_status_t status;
ax900_get_status(&status);
if (status.ready && !status.scanning) {
    esp_err_t result = ax900_request_scan();
    // result == ESP_OK 表示请求已排队；完成后读取 scan_generation / aps。
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

字符串会复制到驱动 RAM，不写入 NVS 或日志，断开或失败时清除。CA 校验还要求设备有正确时间。
若用户明确选择不校验服务器，省略 CA/域名并显式设置 `allow_unverified_server=true`；这种模式无法验证服务器身份，账号信息可能被冒充热点窃取。官方示例窗口提供有明确标记的“不校验证书”选项；使用 CA/域名的应用需通过 API 提供配置。

当前限制：CCMP、TLS 1.2，不支持 WPA3、TKIP、强制 PMF、EAP-TLS、自动漫游/预认证或密码变更。扫描期间不能连接，连接期间须先断开才能扫描。
上游认证核心尚未统一加符号前缀，当前配置关闭内置 Espressif Wi-Fi supplicant；不要同时链接另一套 Hostap 符号。

`associated` 仅表示无线关联；`authenticated` 表示 WPA2 密钥安装/控制端口开放；只有 `has_ip` 才表示 AX900 获得 DHCP 地址。官方演示中另一个 Wi-Fi AP 的 `192.168.4.1` 不能作为 AX900 已联网的证据。
`ax900 associate` 只测试无线关联，不发送用户名或密码，不开放数据端口，成功后 10 秒自动断开。
`ax900 probe` 通过 AX900 接口向 DHCP 网关发送 3 次 ICMP 请求。

## 官方界面与诊断

[官方 M5Tab5 UserDemo 接入说明](docs/official-userdemo.md) 提供固定基线提交及接入补丁。补丁包含 LVGL 界面和以下串口命令：

```text
ax900 status
ax900 scan
ax900 open
ax900 select <SSID>
ax900 associate [frequency_MHz] <SSID>
ax900 disconnect
ax900 probe
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

macOS 重开 CDC 串口可能使设备复位；脚本默认等待 16 秒以避开启动动画。日志和快照含附近网络信息，默认保存在 Git 忽略的 `logs/` 目录。

## 主机测试

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I components/ax900 tests/frame_test.c -o /tmp/ax900-frame-test
/tmp/ax900-frame-test
```

企业网络实测使用用户显式选择的不校验证书模式；CA/域名校验通过 TLS 主机测试，尚未在真实企业网络验证。WPA2-Personal/开放网络实机连接、互联网 DNS/HTTP、重新认证、长期热插拔稳定性与吞吐仍待验证。协议及固件兼容性说明见 [固件说明](components/ax900/firmware/README.md)。

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
