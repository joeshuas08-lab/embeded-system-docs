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
