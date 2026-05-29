# Bluetooth / WiFi 调试笔记 (RK3576 + Android 14)

## 概述

RK3576 平台使用 broadcom 方案实现蓝牙和 WiFi 功能。BT 通过 UART8 连接，WiFi 使用 SDIO 接口。

---

## 一、蓝牙调试

### 1.1 UART8 配置 (`7e60101c689`)

蓝牙默认使用 UART8：

```conf
# bt_vendor.conf
UartPort = /dev/ttyS8
BdAddr = 12:34:56:78:9A:BC
```

### 1.2 蓝牙修复 (`ea4ec01cd49`)

**问题**：蓝牙扫描不到设备或连接不稳定。

**修复内容**：
1. 蓝牙控制器固件加载时序调整
2. UART 波特率匹配（默认 115200 → 3Mbps 或 4Mbps）
3. LPM（低功耗模式）参数优化

### 1.3 通知栏清除 (`62c683e99e0`, `688744b2307`)

CRY 定制需求：在 SystemUI 中移除蓝牙相关通知。

---

## 二、WiFi 调试

### 2.1 禁用 WiFi (`c3381af8893`)

在项目初期禁用了 PCIe WiFi（避免与 Sample Card 冲突）：

```makefile
# kernel config
# CONFIG_PCIE_WIFI is not set
```

### 2.2 WLAN 热点 (`3c594afc077`)

默认热点 IP 配置为 `192.168.43.1`：

```xml
<!-- Android 框架配置 -->
<string name="wifi_tether_configure_ssid_default">MYIR_LR3576</string>
```

### 2.3 WiFi 驱动清理 (`bd619c84b66`, `b5add6656b5`)

删除无用的 `external/wifi_driver` 目录，该目录中的驱动代码未使用。

---

## 三、调试命令

```bash
# 蓝牙调试
# 启动蓝牙
hciattach /dev/ttyS8 bcm43xx 3000000 flow
hciconfig hci0 up

# 扫描蓝牙设备
hcitool scan

# 查看蓝牙 HCI 日志
btmon &

# 查看蓝牙控制器信息
hciconfig -a

# WiFi 调试
# 查看 WiFi 接口
iw dev

# 扫描 WiFi AP
iw dev wlan0 scan

# 查看 WiFi 连接状态
iw dev wlan0 link
```

### 3.1 蓝牙 UART 调试

```bash
# 确认 UART 设备存在
ls -l /dev/ttyS8

# 测试 UART 通信
stty -F /dev/ttyS8 3000000 cs8 -cstopb -parenb
echo -n -e '\x01\x02\x03' > /dev/ttyS8

# 确认蓝牙固件加载日志
dmesg | grep -i bluetooth
dmesg | grep -i hci
```

---

## 四、经验总结

1. **UART 流控**：蓝牙需要硬件流控（RTS/CTS），DTS 中需正确配置
2. **固件加载**：broadcom BT 芯片需在驱动层加载固件（FW），路径在 `/vendor/etc/firmware/`
3. **波特率切换**：初始化先使用 115200，固件加载后切换到 3Mbps 或 4Mbps
4. **共存问题**：2.4G WiFi 和 BT 共享同一频段，确认 MBP（MIMO Band Preference）策略配置
5. **WiFi 热点**：默认 IP 网段固定为 192.168.43.0/24，可在 framework 中修改
