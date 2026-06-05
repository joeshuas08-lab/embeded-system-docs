# MYD-YR3562 快速啟動二次優化

## 背景

一轮优化后上电到 LVGL ~7s（内核时间 ~3.5s）。对标 RK3506（ARM32, ~5.8s），差距主要在 ARM64 架构开销。

## 优化记录

### 1. U-Boot：去 Android boot + AVB

**文件**: `u-boot/configs/myd_yr3562_ub_defconfig`

```diff
-CONFIG_ANDROID_AVB=y
-CONFIG_ANDROID_BOOT_IMAGE_HASH=y
+# CONFIG_ANDROID_AVB is not set
+# CONFIG_ANDROID_BOOT_IMAGE_HASH is not set

-CONFIG_CMD_BOOT_ANDROID=y
+# CONFIG_CMD_BOOT_ANDROID is not set

-CONFIG_AVB_LIBAVB=y
-CONFIG_AVB_LIBAVB_AB=y
-CONFIG_AVB_LIBAVB_ATX=y
-CONFIG_AVB_LIBAVB_USER=y
-CONFIG_RK_AVB_LIBAVB_USER=y
+# CONFIG_AVB_LIBAVB is not set
...
```

效果: 跳过 Android boot 尝试，省 ~5ms（开销本就不大）

### 2. U-Boot：去 OPTEE (BL32)

**文件**: `rkbin/RKTRUST/RK3562TRUST.ini`

```diff
 [BL32_OPTION]
-SEC=1
+SEC=0
```

**文件**: `u-boot/configs/myd_yr3562_ub_defconfig`

```diff
-CONFIG_OPTEE_CLIENT=y
-CONFIG_OPTEE_V2=y
-CONFIG_OPTEE_ALWAYS_USE_SECURITY_PARTITION=y
+# CONFIG_OPTEE_CLIENT is not set
+# CONFIG_OPTEE_V2 is not set
+# CONFIG_OPTEE_ALWAYS_USE_SECURITY_PARTITION is not set
```

效果: 启动链 SPL→ATF→U-Boot（无 BL32），省 ~400ms

### 3. 内核：CPU performance governor

**文件**: `kernel-6.1/arch/arm64/configs/myd_yr3562_tb_defconfig`

```diff
+CONFIG_CPU_FREQ_GOV_PERFORMANCE=y
+CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE=y
```

效果: 启动全程满频，省 ~100ms

### 4. 内核：去 Bluetooth

```diff
-CONFIG_BT=y (10行)
+# CONFIG_BT is not set
```

效果: 省 ~100ms

### 5. 内核：去 WiFi + RFKILL

```diff
-CONFIG_WL_ROCKCHIP=y
-CONFIG_WIFI_BUILD_MODULE=y
-CONFIG_AP6XXX=m
-CONFIG_RFKILL=y
-CONFIG_RFKILL_RK=y
+（全部 # not set）
```

效果: 几乎可忽略（<10ms）

### 6. 用户态：砍 11 个 init 服务

**文件**: `buildroot/board/rockchip/rk3506/post-build-fast-display.sh`

```bash
rm -f $TARGET/etc/init.d/S36wifibt-init.sh
rm -f $TARGET/etc/init.d/S40bluetoothd
rm -f $TARGET/etc/init.d/S40network
rm -f $TARGET/etc/init.d/S41dhcpcd
rm -f $TARGET/etc/init.d/S49chrony
rm -f $TARGET/etc/init.d/S50dropbear
rm -f $TARGET/etc/init.d/S50usbdevice.sh
rm -f $TARGET/etc/init.d/S80dnsmasq
rm -f $TARGET/etc/init.d/S99autorun.sh
rm -f $TARGET/etc/init.d/S99input-event-daemon
rm -f $TARGET/etc/init.d/S30MEasyListen-DEV
```

效果: 用户态更快进入稳定状态

### 7. DTS：loglevel 优化

```diff
-bootargs = "console=ttyFIQ0,115200 loglevel=7 ignore_loglevel root=..."
+bootargs = "console=ttyFIQ0,115200 loglevel=3 quiet root=..."
```

效果: 减少内核串口输出，省 ~50ms

### 8. 失败尝试

- **去掉 U-Boot DRM 显示**: 编译成功但屏幕不亮——U-Boot 显示初始化是显示链的必要环节
- **去掉 ATF**: ARM64 硬性要求，无法绕过
- **暴力砍 U-Boot USB/SPI/MTD**: 编译失败，U-Boot Kconfig 依赖太复杂
- **暴力砍内核 NET/USB/INPUT/RTC**: 编译失败，内核 Kconfig 依赖链未梳理清楚

## 最终时间线

| 里程碑 | 一轮优化 | 二轮优化 | 改善 |
|--------|---------|---------|------|
| SPL | 142ms | 129ms | -13ms |
| U-Boot Total | 2164ms | 2137ms | -27ms |
| LVGL starting | 6.1s | **5.6s** | **-500ms** |
| disp_init | 6.2s | 5.7s | -500ms |
| DRM mapped | 7.5s | 7.0s | -500ms |

**上电到 LVGL 出画面: ~5.6s（内核时间 ~3.5s）**

## 无法突破的瓶颈

| 阶段 | 时间 | 原因 |
|------|------|------|
| DDR 训练 | 1.5s | 硬件固定，预编译固件 |
| ATF (BL31) | 0.4s | ARM64 必须，CPU 特权级切换 |
| U-Boot DRM | 0.5s | 显示初始化 + Logo 显示 |
| 内核基础 init | 2.7s | ARM64 MMU/GIC/SCMI 初始化 |

四项共计 ~5.1s，这是 ARM64 平台的物理极限。

## 仓库提交

| 仓库 | 分支 | 最终 commit |
|------|------|------------|
| kernel-6.1 | myd-fast-boot | `e9006e` (BT+WiFi stripped, perf governor) |
| u-boot | detached HEAD | `754501c` (OPTEE, Android, AVB removed) |
| buildroot | myd-fast-boot | `83e5cc` (11 services removed) |
| device/rockchip | myd-fast-boot | `d58e29c` (SPL restored) |
| rkbin | — | RK3562TRUST.ini BL32 SEC=0 |

---

## 复现步骤

### 环境

```bash
cd /home/joes/work/rk/rk3562J/MYD-YR3562
```

### 1. 切分支

```bash
git -C kernel-6.1 checkout myd-fast-boot
git -C buildroot checkout myd-fast-boot  
git -C device/rockchip checkout myd-fast-boot
# u-boot: detached HEAD, already at b573b73e6f
```

### 2. 修改 rkbin (去 OPTEE)

```bash
# 编辑 rkbin/RKTRUST/RK3562TRUST.ini
# [BL32_OPTION] SEC=1 → SEC=0
```

### 3. 编译

```bash
./build.sh device/rockchip/.chips/myir_yr3562/myd_yr3562_tb_defconfig
./build.sh          # 全量编译
```

### 4. 测试

```bash
# 串口连 RK3562J(192.168.1.188) /dev/ttyACM0
# 启动后确认: LVGL demo starting/ready, disp_init 1200x1920, 无 OPTEE 输出
```

### 验证命令

```bash
# 确认 OPTEE 已移除
dmesg | grep -c OP-TEE    # 应为 0

# 确认 BT/WiFi 已禁用  
dmesg | grep -ic bluetooth # 应为 0

# 确认 LVGL 运行
ps | grep rk_demo
```
