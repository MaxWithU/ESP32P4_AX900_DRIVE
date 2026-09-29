# 接入官方 M5Tab5 UserDemo

已验证基线：`m5stack/M5Tab5-UserDemo` 提交 `b4e356bc491ca070d54004718dad789c07d5fc93`，ESP-IDF 5.5.2。补丁保留官方界面与 HID，添加 AX900 USB 客户端、扫描窗口及本地串口诊断。

目录关系需为：

```text
workspace/
├── tab5-ax900/             # 本仓库
└── Tab5/
    └── M5Tab5-UserDemo/    # 官方工程
```

从本仓库根目录执行：

```sh
python3 tools/fetch_firmware.py
mkdir -p ../Tab5
git clone https://github.com/m5stack/M5Tab5-UserDemo.git ../Tab5/M5Tab5-UserDemo
cd ../Tab5/M5Tab5-UserDemo
git switch --detach b4e356bc491ca070d54004718dad789c07d5fc93
git apply --check ../../tab5-ax900/patches/m5tab5-userdemo-ax900.patch
git apply ../../tab5-ax900/patches/m5tab5-userdemo-ax900.patch
python3 fetch_repos.py
git apply --check --directory=dependencies/smooth_ui_toolkit ../../tab5-ax900/patches/smooth-ui-toolkit-spring-init.patch
git apply --directory=dependencies/smooth_ui_toolkit ../../tab5-ax900/patches/smooth-ui-toolkit-spring-init.patch
idf.py -C platforms/tab5 build
```

`fetch_repos.py` 是官方依赖获取脚本，按官方 `repos.json` 获取固定标签。已有开发目录时，应先确认本地修改，避免在其他版本上直接应用补丁。若目录布局不同，修改 `platforms/tab5/CMakeLists.txt` 中的 AX900 组件路径。

第二个补丁修复 `smooth_ui_toolkit v2.0.0` 的弹簧动画速度未初始化问题。实机调试确认该值可能成为 NaN，使启动动画无限等待；补丁将初始速度设为零，不改变正常动画。它必须在获取官方依赖后应用。

完成 [完整 Flash 备份](flashing.md) 后，首次采用官方分区布局时：

```sh
idf.py -C platforms/tab5 -p /dev/cu.usbmodem1101 flash
```

已经安装相同官方分区布局时，可仅更新 `0x10000` 处的应用镜像 `platforms/tab5/build/m5stack_tab5.bin`。

启动后点击右上角 **AX900 Wi-Fi**。列表显示 SSID、频段、信道和 RSSI；**Scan again** 重新扫描。点击网络可输入个人密码，或 PEAP/MSCHAPv2 的企业用户名和密码；连接后使用 **Disconnect** 断开。企业窗口按用户选择提供明确标记的“不校验证书”模式，API 也支持 PEM CA 与服务器域名。已实机验证 5 GHz PEAP/MSCHAPv2、DHCP 和网关通信，验证范围见根目录 README 与 VALIDATION.json。

官方 `sdkconfig` 已启用 `CONFIG_LV_USE_SNAPSHOT=y`，补丁将此设置也加入 `sdkconfig.defaults`，以支持 `ax900 snapshot`；保留官方非阻塞 USB Serial/JTAG VFS 行为。串口命令和主机截图解码脚本见根目录 README。

切换至 **Connection test** 查看四个连接阶段、地址信息及数据收发计数。连接并取得 DHCP 地址后，点击 **Run test** 运行 5 次网关 Ping；测试过程会逐格更新延迟，结束后显示平均值和丢包率。未连接时按钮不可用，切换标签或关闭窗口不会访问已释放的界面对象。重新连接后需重新测试。

串口 `ax900 test` 打开面板，`ax900 test-run` 通过同一个界面按钮触发测试，`ax900 test-status` 输出结果；底层 API 为 `ax900_probe_start()` / `ax900_probe_get_result()`，定义见 `ax900_probe.h`。

补丁源代码遵循 [MIT 许可](../patches/LICENSE.MIT)，AX900 驱动组件遵循 Apache-2.0。

实体键盘使用量产 Tab5Keyboard 的 I²C 0x6D 协议（SDA=0、SCL=1），使用 ESP32-P4 的独立 LP I²C 控制器，Port A 继续使用原引脚 53/54，支持热接入。未使用旧的 TCA8418 测试程序；没有按键日志。`Aa` 按住为大写/Shift，`Sym` 按住使用符号层；Tab / Aa+Tab 切换焦点，Enter 从用户名移至密码、从密码移至 Connect，再按 Enter 连接；Esc 关闭输入窗口。可用 Tab 选中 Run test 再按 Enter。窗口状态栏显示键盘连接情况。

认证且 DHCP 成功后自动保存最多 4 个网络，窗口显示 Wi-Fi saved。重启扫描后自动尝试最近成功的网络，优先同名 5 GHz AP；失败一次后留给用户操作。已保存的网络提供 Use saved login，也可直接输入新密码。Forget saved 只删除 AX900 命名空间，当前连接继续可用且不会立即重新保存。没有账号/密码串口输入或导出命令；凭据窗口禁止截图。NVS 默认未加密，详见 README。

Ping 使用 SDK 自带实现，在启用 CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM 的 Tab5 固件上将任务栈放入 PSRAM；删除使用对应的 vTaskDeleteWithCaps。其他未启用外部任务栈的平台仍使用 SDK 默认分配方式。
