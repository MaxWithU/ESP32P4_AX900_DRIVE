# 候选版交付与维护

版本号存放在 `release/VERSION`。`0.1.0-rc.1` 面向已验证硬件组合，不扩大为其他 AX900 型号或所有 ESP32-P4 板卡的兼容承诺。

## 包含什么

- 官方 UserDemo 加 AX900/Tab5Keyboard 界面的应用镜像；网卡运行固件嵌入应用。
- 分区表副本用于识别布局，工具不写入该副本。
- `manifest.json`：版本、源码提交、官方基线、IDF 版本、兼容目标、文件大小与 SHA-256。
- 本地更新/恢复工具、固定 Python 依赖、中文安装说明、验证范围和许可证/来源说明。

不包含设备备份、NVS 导出、账号密码、实际 CA、原始日志、截图、开发环境、ELF 或串口抓包。依赖许可证通过文件名白名单打包。发布仅上传生成的 ZIP 和 ZIP 校验文件；仓库不跟踪固件二进制。

## 生成

在 ESP-IDF 5.5.2 环境中，官方源码须基于 `b4e356bc491ca070d54004718dad789c07d5fc93`，已应用仓库两个补丁并获取依赖。源码修改、测试和验证结论先提交，再生成发布包。

```sh
python3 tools/update_official_patch.py --official ../Tab5/M5Tab5-UserDemo --check
python3 tools/test_local.py --official ../Tab5/M5Tab5-UserDemo --mbedtls "$IDF_PATH/components/mbedtls/mbedtls"
# 使用 release/VERSION 中的实际版本
idf.py -C ../Tab5/M5Tab5-UserDemo/platforms/tab5 -DPROJECT_VER=0.1.0-rc.1 build
python3 tools/package_release.py --official ../Tab5/M5Tab5-UserDemo
```

打包会拒绝未提交源码、与补丁不一致的官方文件、待重新构建的应用、版本不匹配、故障注入开启以及缺少 DNS/证书校验配置的构建。`dist/` 为本地忽略目录。先在临时目录解压，执行 `python tab5_flash.py verify`，核对 `SHA256SUMS`；上传前复核包内文件。

UI 源码修改后，使用 `tools/update_official_patch.py --official …` 重建补丁。脚本只读取官方工程，不替该工程提交本地修改。

## 发布和升级约束

发布为 GitHub **Pre-release**，标明实际验证范围；不能把历史固件实测写成候选版已验收。源码提交固定后再创建对应标签，保留各版本独立校验值，避免覆盖同名 ZIP。

当前为 factory 单应用分区，通过 USB 更新/恢复，不支持 OTA 双分区自动回滚。升级不迁移或擦除 NVS；未来修改保存格式时，须设计显式版本迁移和降级兼容性，不能沿用此版本的保留配置承诺。

自动检查只能确认芯片、容量、安全配置及分区，不能证明物理板卡型号；用户仍须确认 Tab5 V3。只支持当前普通 Flash 配置，不修改安全启动、加密或 eFuse。

发布前的实机验收仍需补齐：实际更新、启动、已保存网络重连、应用恢复，以及错误提示的屏幕检查。首个候选版只记录主机测试、构建与打包验证，原始诊断始终留在本地。
