# Tab5 AX900 候选版

适用：M5Stack Tab5 V3、ESP32-P4 revision 1.3、16 MB Flash、原官方 UserDemo 分区布局，以及本项目已验证的 AX900 D80 U02 网卡。ROM 只能识别芯片，不能证明板卡型号；请自行核对 Tab5 V3。

这是候选版：构建和主机回归通过，新版尚未完成实机烧录、恢复和长期运行验收。仅下载项目维护者发布的包；SHA-256 用于检测文件损坏，不是发布者的数字签名。

## 一次准备

安装 Python 3.10 或更新版本。解压包后在其目录打开终端；无需安装 ESP-IDF 或自行编译。

```sh
python3 -m venv .venv
# macOS / Linux
source .venv/bin/activate
# Windows PowerShell 改用：.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python tab5_flash.py verify
```

将 Tab5 用 USB-C **数据线**连接电脑，退出串口监视器。以下工具不会访问网络或上传诊断。首次 Python 依赖安装需要联网。

## 安装或升级

```sh
python tab5_flash.py check --board tab5-v3
python tab5_flash.py update --board tab5-v3
```

自动选择唯一的 Espressif USB 串口。有多台设备时增加 `--port /dev/cu.usbmodemXXXX`（macOS）、`--port /dev/ttyACM0`（Linux）或 `--port COM5`（Windows），以实际端口为准。连接检查会重启 Tab5，但不写 Flash。

更新前会检查芯片、容量、安全配置、分区和镜像校验值，读取并校验整个旧应用分区，将备份写入本地 `backups/tab5-…/` 后才开始写入。只更新应用区，不修改 NVS、已保存 Wi-Fi、bootloader、分区表或其他数据。写入中保持供电和数据线连接。

工具拒绝其他芯片、未验证的芯片版本、不同分区以及已启用安全启动/Flash 加密的设备，不提供强制跳过开关。它不能初始化空白芯片，也不能自动迁移其他固件的分区。自定义布局使用仓库的开发者烧录说明和独立完整备份。

更新成功仅表示镜像写入校验通过。启动后插入 AX900，在 **AX900 Wi-Fi → Networks** 选择网络并输入密码；出现 IP 后，在 **Connection test** 运行测试。保存配置可自动重连。界面中的错误标题可点击查看下一步处理建议。

## 恢复旧应用

使用更新时打印的备份目录；不要删除或上传备份。

```sh
python tab5_flash.py restore --board tab5-v3 --backup backups/tab5-实际目录
```

工具核对备份完整性、设备身份和分区，先为当前应用创建另一份备份，再恢复旧应用并校验。恢复保留**当前**网络设置，不回退 NVS 内容。若应用无法启动但芯片仍可进入 ROM 下载模式，依然可以恢复；串口连接失败时，先重新接好数据线并按板卡官方说明进入下载模式，再执行相同命令。若 bootloader 或分区曾被外部工具修改，此恢复流程会拒绝操作，需要你自己的完整 Flash 备份。

本候选版使用 factory 单应用分区，没有 OTA 双分区自动回滚。掉电后的恢复需要电脑和上述本地备份。

## 常见问题

- 找不到设备：确认使用数据线、关闭监视器；多设备时指定端口。
- 布局不匹配：停止，不要擦除 Flash；先确认原固件和完整备份。
- 认证被拒绝：核对账号及网络策略；该提示不能单独证明密码错误。
- 证书日期失败：检查设备时间和服务器证书有效期；不要通过关闭校验规避。
- 无 IP：检查路由器 DHCP 服务和地址池。
- DNS 不可用：在 Target 填写此网络能访问的 DNS IPv4，或检查 DHCP 配置。
- 网关不回 Ping：网关可能禁用 ICMP，可另测 TCP/HTTP；不能仅凭 Ping 判定网络断开。

只支持已说明的 WPA2/CCMP、PEAP/MSCHAPv2 和开放网络接口。WPA3、强制 PMF、EAP-TLS 与无缝漫游未实现。企业 CA 验证需正确时间与可信 CA；详见随包 README 和 VALIDATION。

`logs/`、`backups/` 仅存本地；程序不会自动发送任何文件。供应商网卡固件及第三方组件适用各自条款，见 `licenses/` 和来源清单。
