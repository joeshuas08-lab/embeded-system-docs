# Type-C / USB / DP 调试笔记 (RK3576 + Android 14)

## 概述

RK3576 平台 Type-C 接口支持 USB 2.0/3.0、DisplayPort Alt Mode 和充电功能。调试涵盖 Type-C 控制器移植、USB 3.0 信号完整性、DP 显示模式、MTP 配置等。

---

## 一、Type-C 控制器移植

### 1.1 基础移植 (`52dab8c711f`)

Type-C port 控制器使用 RK3576 内置 TCPC（Type-C Port Controller），通过 DTS 配置：

```c
&usbdrd3_0 {
    status = "okay";
    extcon = <&usb2phy0_grf>;
};
```

### 1.2 Type-C DP 功能 (`1ede7ee5e12`, `8eb29f706ee`)

**问题**：Type-C 接口的 USB 3.0 信号存在硬件问题，导致 USB 3.0 速度下 DP Alt Mode 无法正常工作。

**修复过程**：

1. 硬件排查发现 USB 3.0 TX/RX 差分对存在阻抗不匹配
2. 临时方案：断开 USB 3.0 连接线后 DP 恢复正常
3. 沟通确认：该硬件问题已修复，DTS 中移除 workaround

```diff
- // disable USB 3.0 for DP function fix
- &usbdrd_phy3_0 {
-     status = "disabled";
- };
```

**经验**：Type-C DP Alt Mode 需要 USB 3.0 和 DisplayPort 共享高速通道（MUX 切换），任何 USB 3.0 信号质量问题都会影响 DP 功能。

---

## 二、USB 模式配置

### 2.1 MTP 默认模式 (`79921e20d2d`)

```makefile
# device.mk
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp \
    ro.adb.secure=0
```

### 2.2 USB 目录可见 (`1a3bde1933a`)

允许 MTP 连接时显示 data 分区目录结构：

```makefile
# system.prop
persist.sys.usb.config=mtp
```

### 2.3 USB 名称修改 (`63120e823a8`)

```makefile
PRODUCT_PROPERTY_OVERRIDES += \
    ro.product.usbManufacturer=MYIR \
    ro.product.usbProduct=LR3576
```

---

## 三、调试命令

```bash
# 查看 USB 设备
lsusb
lsusb -t   # USB 树形结构

# 查看 Type-C 状态
cat /sys/class/extcon/extcon*/state

# 查看 USB 控制器寄存器
cat /sys/kernel/debug/usb/*/registers

# MTP 测试
# Windows/Mac 连接后应显示设备存储
# 确认 /data 分区可读写

# 测试 USB 3.0 速率
dd if=/dev/zero of=/mnt/usb/test bs=1M count=100
```

---

## 四、经验总结

1. **USB 3.0 信号完整性**：Type-C DP Alt Mode 依赖于 USB 3.0 通道复用，信号质量直接影响 DP 输出
2. **MTP 权限**：确保 adbd 和 storage 服务的 SELinux 权限正确配置
3. **Extcon 驱动**：Type-C 插拔状态依赖 extcon 驱动上报，必须与 charger 驱动正确联动
4. **硬件调试**：Type-C CC 引脚的电压状态是判断连接方向的关键信号
5. **DP 调试**：如果 DP 不工作，先排除 USB 3.0 信号问题，再检查 MUX 芯片（如果有）配置
