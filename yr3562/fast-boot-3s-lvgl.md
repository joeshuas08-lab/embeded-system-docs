# MYD-YR3562 3秒快速启动到LVGL

## 目标

从 U-Boot 启动内核到 LVGL 显示界面，目标 3 秒内完成。

## 参考来源

学习了 YR3506 (`zrm@192.168.1.177:/home/zrm/YR3506/MYD-YR3506`) 的快速启动实现方案：
- 内核配置: `myd_yr3506_tb_defconfig` (TB = Tiny Boot)
- LVGL 显示: `myd-yr3506-display.config` (显示片段, TB + display 组合)
- 快速 init: `board/rockchip/rk3506/fast-display-overlay/`
- 后处理脚本: `board/rockchip/rk3506/post-build-fast-display.sh`

## YR3506 TB 快速启动策略

### 内核层面 (388行 vs BSP 413行)

| 优化项 | 作用 |
|--------|------|
| `CONFIG_KERNEL_LZ4=y` | 解压速度比 gzip 快 3-5x |
| `CONFIG_CC_OPTIMIZE_FOR_SIZE=y` | 内核体积减小 ~15% |
| `CONFIG_LD_DEAD_CODE_DATA_ELIMINATION=y` | 移除未引用代码 |
| `CONFIG_ROCKCHIP_MINI_KERNEL=y` | 跳过部分内核初始化 |
| `# CONFIG_VT is not set` | 无 VT 控制台切换开销 |
| `# CONFIG_BUG is not set` | 移除 BUG/WARN 支持 |
| `# CONFIG_BASE_FULL is not set` | 精简内核基础功能 |
| `# CONFIG_SLUB_DEBUG is not set` | 移除内存调试 |
| `# CONFIG_FTRACE is not set` | 移除函数追踪 |
| `# CONFIG_SCHED_DEBUG is not set` | 移除调度调试 |
| `CONFIG_MMC_QUEUE_DEPTH=1` | 减少 MMC 队列 |
| `CONFIG_PREEMPT=y` | 全抢占 (BSP 用 VOLUNTARY) |
| `CONFIG_LOG_BUF_SHIFT=14` | 减小日志缓冲区 |
| 移除 MEDIA/CAMERA/DRM | TB 内核不含显示驱动 |
| 移除 CGROUPS | 无 cgroup 开销 |

### Init 机制 (pre_init)

YR3506 使用自定义 inittab + `rcS.early` 实现分层启动:

```
inittab:
  ::sysinit:/etc/init.d/rcS.early    ← 先运行 pre_init 脚本
  ::sysinit:/etc/init.d/rcS          ← 再运行正常 init

rcS.early → 遍历 /etc/init.d/pre_init/S??*
  ├── S00async-commit.sh  ← GPU 显存异步提交优化
  └── S05lv_demo.sh       ← LVGL 应用启动
```

`post-build-fast-display.sh` 将 `S05lv_demo.sh` 和 `S05async-commit.sh` 从正常 init 移到 pre_init 目录。

### 文件系统

- SquashFS ZSTD 压缩 rootfs — 快速读取
- `# BR2_TARGET_GENERIC_REMOUNT_ROOTFS_RW is not set` — RO rootfs, 免 fsck

### LVGL 配置

```
BR2_LV_DRIVERS_USE_DRM=y    ← DRM backend
BR2_LV_DRIVERS_USE_RGA=y    ← Rockchip RGA 加速
```

## YR3562 实现方案

### 与 YR3506 的关键差异

| 项目 | YR3506 | YR3562 |
|------|--------|--------|
| 架构 | ARM32 (Cortex-A7) | ARM64 (Cortex-A55) |
| TB 内核 | 不含 DRM/显示 | **必须保留** DRM/DSI (LVGL 需要) |
| 内核路径 | `arch/arm/configs/` | `arch/arm64/configs/` |
| Soc 芯片 | RK3506 | RK3562 |
| 显示接口 | RGB/MIPI DSI | MIPI DSI (mipi101c 面板) |

### 创建的文件

#### 1. 内核 defconfig
**路径**: `kernel-6.1/arch/arm64/configs/myd_yr3562_tb_defconfig`
**大小**: 520 行 (BSP: 699 行, 减少 26%)

基于 `myd_yr3562_bsp_defconfig` 应用所有 YR3506 TB 优化 + 保留:
- DRM/ROCKCHIP/DW_MIPI_DSI (LVGL 显示)
- DRM_PANEL_SIMPLE (DSI 面板)
- Mali GPU + RGA (加速)
- EMMC/以太网/USB/PCIe/WiFi

移除 (vs BSP):
- CGROUPS, NAMESPACES, XFRM, CAN
- MEDIA_SUPPORT, CAMERA 所有 sensor 驱动
- PPP, 80% NET_VENDOR
- XFS, JFFS2, NTFS, NFS, UBIFS, ISO9660
- 大量声卡 codec, HID 驱动
- I3C, GPIO_SYSCON, 多余 regulator
- DEBUG_CREDENTIALS, DYNAMIC_DEBUG, FTRACE, SCHED_DEBUG
- 多余 ARM errata workaround

#### 2. Device 配置
**路径**: `device/rockchip/.chips/myir_yr3562/myd_yr3562_tb_defconfig`

```ini
RK_KERNEL_CFG="myd_yr3562_tb_defconfig"   # 只用 TB config, 不加 docker fragment
RK_BUILDROOT_BASE_CFG="myd_yr3562_tb"
```

#### 3. Buildroot 配置
**路径**: `buildroot/configs/myd_yr3562_tb_defconfig`

```ini
#include "gui/lvgl/lvgl_drm.config"       # LVGL + DRM backend
#include "gui/lvgl/rk_demo.config"        # RK LVGL demo
BR2_LV_DRIVERS_USE_DRM=y
BR2_LV_DRIVERS_USE_RGA=y
BR2_TARGET_ROOTFS_SQUASHFS=y             # SquashFS rootfs
BR2_TARGET_ROOTFS_SQUASHFS_ZSTD=y        # ZSTD 压缩
# BR2_TARGET_GENERIC_REMOUNT_ROOTFS_RW   # RO 根文件系统
BR2_ROOTFS_POST_BUILD_SCRIPT+="board/rockchip/rk3506/post-build-fast-display.sh"
```

## 编译方法

```bash
cd /home/joes/work/rk/rk3562J/MYD-YR3562
./build.sh device/rockchip/.chips/myir_yr3562/myd_yr3562_tb_defconfig
```

## 启动流程

```
U-Boot → 加载 FIT Image (LZ4 内核 + ZSTD squashfs)
       → 内核解压/初始化 (LZ4 + MINI_KERNEL)
       → 挂载 squashfs rootfs (免 fsck)
       → /init (busybox)
         → inittab: rcS.early
           → pre_init/S00async-commit  (GPU 优化)
           → pre_init/S05lv_demo       (LVGL 启动, ~3s)
         → inittab: rcS (正常服务, 异步进行)
```

## 进一步优化方向

1. **U-Boot 层面**: 去掉 logo 显示, 减少 delay, 优化 MMC 初始化
2. **内核 cmdline**: 添加 `quiet loglevel=0` 减少串口输出
3. **LVGL 应用**: 静态链接, 避免动态库加载时间
4. **initramfs**: 可将 LVGL 打包进 initramfs，完全跳过 rootfs 挂载

## 相关分支

- 内核: `kernel-6.1` → `myd-fast-boot` 分支
- Device: `device/rockchip` → `myd-fast-boot` 分支
- Buildroot: `buildroot` → `myd-fast-boot` 分支

## Git 仓库位置

- 环境: `/home/joes/work/rk/rk3562J/MYD-YR3562/`
- 远程参考: `zrm@192.168.1.177:/home/zrm/YR3506/MYD-YR3506/`

---

**日期**: 2026-05-28
**平台**: Rockchip RK3562 (MYD-YR3562)
**内核版本**: Linux 6.1

---

## 实施结果 (2026-06-01)

### 最终启动时间

| 里程碑 | 上电时间 | 内核时间 |
|--------|---------|---------|
| DDR | 1.5s | — |
| U-Boot SPL | 2.9s | — |
| 内核启动 | 4.2s | 0s |
| Booting Linux | 4.4s | 2.70s |
| disp_init (LVGL) | ~5.2s | ~3.5s |
| LVGL 出画面 | **~5s** | **~3.5s** |
| Run /sbin/init | 8.1s | 5.25s |

**上电到 LVGL 出画面: ~5 秒**

### 内核配置: 489 行 (BSP: 699 行, -30%)

### 可复现的修复补丁

#### 1. 显示修复 — MIPI DTS 配置

**文件**: `kernel-6.1/arch/arm64/boot/dts/rockchip/myd-yr3562-mipi101c.dtsi`

DTS 使用 Android BSP 版本 (来自 `\\192.168.1.117\loh_media\rockchip\yr3562-android14\kernel-6.1\arch\arm64\boot\dts\rockchip\myd-yr3562-mipi101c.dtsi`), 关键属性:
```dts
dsi_panel: panel@0 {
    compatible = "simple-panel-dsi";
    backlight = <&backlight>;
    bpc = <8>;                           // 必须: bits per color
    bus-format = <0x1017>;               // 必须: MEDIA_BUS_FMT_RGB888_1X24
    dsi,flags = <(MIPI_DSI_MODE_VIDEO |  // 必须: VIDEO_BURST (非 SYNC_PULSE)
                  MIPI_DSI_MODE_VIDEO_BURST |
                  MIPI_DSI_MODE_LPM |
                  MIPI_DSI_MODE_NO_EOT_PACKET)>;
    dsi,format = <MIPI_DSI_FMT_RGB888>;
    dsi,lanes = <4>;
    panel-init-sequence = [
        23 00 02 B0 5A
        23 00 02 B1 00
        23 00 02 89 01
        23 00 02 2C 28
        23 00 02 00 F1
        05 78 01 11     // sleep_out, 120ms
        05 14 01 29     // display_on, 20ms
    ];
    ...
};
pwms = <&pwm3 0 2000 0>;  // PWM period 2000ns (Android BSP 值, 非 25000ns)
```
**文件**: `kernel-6.1/arch/arm64/boot/dts/rockchip/myd-yr3562.dts`
```dts
#include "myd-yr3562-mipi101c.dtsi"     // 必须取消注释
// #include "myd-yr3562-lt8912-hdmi.dtsi" // HDMI 与 MIPI 互斥
```
**注意**: RK618 驱动不需要 — Android BSP 有但实际未使用。

#### 2. Recovery 自动重启修复

**根因**: `buildroot/configs/rockchip/base/recovery.config` 强制启用 `BR2_PACKAGE_RECOVERY=y`, 且 `S40recovery` init 脚本调用 `/usr/bin/recovery &` 触发重启。

**修复**:
```bash
# kernel-6.1/arch/arm64/configs/myd_yr3562_tb_defconfig (已禁用 8250DW)
# CONFIG_SERIAL_8250_DW is not set

# buildroot/configs/myd_yr3562_tb_defconfig
# BR2_PACKAGE_RECOVERY is not set
```

**文件**: `buildroot/board/rockchip/rk3506/post-build-fast-display.sh`  
在 post-build 中额外删除 recovery 脚本:
```bash
rm -f $TARGET/etc/init.d/S99lunch_recovery
rm -f $TARGET/etc/init.d/S15linkmount_recovery
rm -f $TARGET/usr/bin/RkLunch.sh
```
**注意**: 必须 `rm -rf buildroot/output/myd_yr3562_tb` clean rebuild, 否则旧 `S40recovery` 残留。

#### 3. LVGL 自动启动 (pre_init)

**文件**: `buildroot/board/rockchip/rk3506/fast-display-overlay/etc/init.d/S10lv_demo`
```sh
start() {
    # Wait for DRM to be ready (最多 3s)
    for i in $(seq 1 30); do
        if [ -e /sys/class/drm/card0-DSI-1/enabled ] && \
           [ "$(cat /sys/class/drm/card0-DSI-1/enabled)" = "enabled" ]; then
            break
        fi
        sleep 0.1
    done
    ulimit -n 1024
    rk_demo &    # 注意: 不要设置 LV_DRIVERS_SET_PLANE=CURSOR!
    sleep 1
}
```

**文件**: `buildroot/board/rockchip/rk3506/post-build-fast-display.sh` 末尾:
```bash
chmod +x $TARGET/etc/init.d/pre_init/S* 2>/dev/null || true
```

#### 4. 内核编译修复

**SAMSUNG_DCPHY 编译失败** (MEDIA_SUPPORT not set):
```diff
- CONFIG_PHY_ROCKCHIP_SAMSUNG_DCPHY=y
+ # CONFIG_PHY_ROCKCHIP_SAMSUNG_DCPHY is not set
```

**Mali GPU 编译失败** (PM_DEVFREQ not set → MALI_DEVFREQ 被丢弃):
```diff
+ CONFIG_PM_DEVFREQ=y
```

**内核 panic** (8250DW 与 FIQ debugger 抢 UART0):
```diff
- CONFIG_SERIAL_8250_DW=y
+ # CONFIG_SERIAL_8250_DW is not set
```

#### 5. Deferred probe 消除

禁用 SOUND/PCIe/ATA 及相关 PHY:
```diff
- CONFIG_SOUND=y ... (28行)
+ # CONFIG_SOUND is not set
- CONFIG_PCI=y ... (6行)
+ # CONFIG_PCI is not set
- CONFIG_ATA=y ... (5行)
+ # CONFIG_ATA is not set
```
效果: kernel deferred probe 从 ~10s → 0

### 仓库分支状态

全部在 `myd-fast-boot` 分支, 已 commit:
- kernel-6.1: `4d08f02e59f1` (6 commits)
- buildroot: `a0ffbadd24` (5 commits)
- device/rockchip: `89738bc` (1 commit)
