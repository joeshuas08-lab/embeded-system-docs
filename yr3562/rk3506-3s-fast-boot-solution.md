# RK3506 3秒快速启动方案

## 架构概况

- **SoC**: Rockchip RK3506
- **CPU**: ARM32 Cortex-A7
- **内核**: Linux 6.1
- **文件系统**: SquashFS ZSTD
- **显示**: LVGL via DRM/RGA

## 内核配置 (388行)

### 压缩与体积优化
```
CONFIG_KERNEL_LZ4=y                     # LZ4 解压，比 gzip 快 3-5x
CONFIG_CC_OPTIMIZE_FOR_SIZE=y          # -Os 编译，内核缩小 ~15%
CONFIG_LD_DEAD_CODE_DATA_ELIMINATION=y # 链接时移除未引用代码
```

### 精简启动
```
CONFIG_ROCKCHIP_MINI_KERNEL=y          # 跳过部分内核初始化
CONFIG_CMA_INACTIVE=y                  # CMA 惰性初始化
CONFIG_CMA_SIZE_MBYTES=0               # 零 CMA，省内存初始化
# CONFIG_VT is not set                 # 无虚拟终端开销
# CONFIG_BUG is not set                # 移除 BUG/WARN 支持
# CONFIG_BASE_FULL is not set          # 精简内核基础
# CONFIG_SLUB_DEBUG is not set         # 无内存调试
# CONFIG_FTRACE is not set             # 无函数追踪
# CONFIG_SCHED_DEBUG is not set        # 无调度调试
CONFIG_LOG_BUF_SHIFT=14                # 小日志缓冲区
```

### 去除的子系统
- **CGROUPS**: 全部禁用
- **NAMESPACES**: 禁用
- **MEDIA/CAMERA**: 全部禁用
- **SOUND/ALSA**: 全部禁用
- **DRM 之外的显示**: 仅保留 DRM + DSI
- **多余文件系统**: 仅保留 ext4, squashfs, vfat

### CPU 精简
```
CONFIG_PREEMPT=y                        # 全抢占
CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND=y  # 仅 ondemand 调速器
CONFIG_MMC_QUEUE_DEPTH=1               # 减少 MMC 队列
```

## Init 机制 —— 核心创新

### 分层启动
```
inittab:
  ::sysinit:/etc/init.d/rcS.early    ← 先运行 pre_init 脚本
  ::sysinit:/etc/init.d/rcS          ← 再运行正常 init
```

### pre_init 目录
`/etc/init.d/pre_init/` 包含两个脚本：
```
S00async-commit.sh    ← GPU 显存异步提交优化
S05lv_demo.sh         ← LVGL 应用启动（~3s 出画面）
```

`post-build-fast-display.sh` 将 LVGL 和 async-commit 从正常 init 移到 pre_init 目录。

### 效果
- `rcS.early` 遍历 `pre_init/` 脚本，不等 `rcS` 完成
- LVGL 在 init 启动后立即开始运行
- 用户看到界面时，后台服务还在初始化

## 文件系统

```
BR2_TARGET_ROOTFS_SQUASHFS=y           # SquashFS
BR2_TARGET_ROOTFS_SQUASHFS_ZSTD=y      # ZSTD 压缩
# BR2_TARGET_GENERIC_REMOUNT_ROOTFS_RW is not set  # RO 根文件系统
```

- SquashFS ZSTD: 快速读取，免 fsck
- RO 根文件系统: 跳过检查/修复

## LVGL 配置

```
BR2_LV_DRIVERS_USE_DRM=y    ← DRM 后端
BR2_LV_DRIVERS_USE_RGA=y    ← Rockchip RGA 加速
```

## 启动时序

```
DDR(1.2s) → U-Boot SPL(0.14s) → ATF+OPTEE(0.8s)
→ U-Boot(0.7s) → Kernel 解压+启动(2.2s)
→ /sbin/init(2.7s) → rcS.early → pre_init/S05lv_demo → LVGL 出画面(~3s)
```

总启动到 LVGL 出画面：**~3 秒内核时间**（约 6 秒从 power-on）

## 关键文件

| 文件 | 作用 |
|------|------|
| `arch/arm/configs/myd_yr3506_tb_defconfig` | 精简内核 (388行) |
| `buildroot/configs/myd_yr3506_tb_defconfig` | TB Buildroot 配置 |
| `board/rockchip/rk3506/fast-display-overlay/etc/inittab` | 分层 init |
| `board/rockchip/rk3506/fast-display-overlay/etc/init.d/rcS.early` | pre_init 启动器 |
| `board/rockchip/rk3506/post-build-fast-display.sh` | 后处理：移脚本到 pre_init |

## 恢复/Reboot 处理

在 `post-build-fast-display.sh` 中删除 recovery 相关 init 脚本，防止启动恢复循环：
```bash
rm -f $TARGET/etc/init.d/S99lunch_recovery
rm -f $TARGET/etc/init.d/S15linkmount_recovery
rm -f $TARGET/usr/bin/RkLunch.sh
```
