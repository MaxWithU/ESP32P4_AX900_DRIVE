# 烧录与恢复

根目录工程是独立串口扫描示例，官方屏幕界面另见 [接入说明](official-userdemo.md)。使用 ESP-IDF 5.5.2 自带的 esptool，先核对连接设备：

```sh
esptool --port /dev/cu.usbmodem1101 chip_id
```

目标必须是 ESP32-P4；端口名本身不能证明芯片型号。以下命令以 16 MB Flash 的 Tab5 为例，需按实际端口修改。

## 完整备份

```sh
mkdir -p backups
esptool --chip esp32p4 --port /dev/cu.usbmodem1101 --baud 921600 \
  read_flash 0x0 0x1000000 backups/tab5-original.bin
chmod 600 backups/tab5-original.bin
shasum -a 256 backups/tab5-original.bin
```

备份包含 NVS 及原设备状态，保存在 Git 忽略的 `backups/` 中。

## 保留已有官方分区布局

本项目实测的官方布局中，factory 应用位于 `0x10000`、大小 10 MB。确认实际分区表一致且镜像未超出分区后，可只更新应用：

```sh
esptool --chip esp32p4 --port /dev/cu.usbmodem1101 --baud 921600 \
  write_flash 0x10000 build/tab5_ax900.bin
```

该操作会把屏幕界面替换为本项目的串口扫描示例。要使用官方界面，应构建并烧录 UserDemo 中的 `m5stack_tab5.bin`。

## 恢复完整备份

若曾更改 bootloader 或分区表，应恢复完整镜像，而不是仅回写应用分区：

```sh
esptool --chip esp32p4 --port /dev/cu.usbmodem1101 --baud 921600 \
  write_flash 0x0 backups/tab5-original.bin
esptool --chip esp32p4 --port /dev/cu.usbmodem1101 --baud 921600 \
  verify_flash 0x0 backups/tab5-original.bin
```

esptool 5.x 中命令名称使用 `chip-id`、`read-flash`、`write-flash` 和 `verify-flash`。
