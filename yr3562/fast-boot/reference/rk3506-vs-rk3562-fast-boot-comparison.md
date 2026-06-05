# RK3506 vs RK3562 快速启动方案对比

## 硬件差异

| 项目 | RK3506 | RK3562 |
|------|--------|--------|
| CPU 架构 | ARM32 Cortex-A7 | ARM64 Cortex-A55 |
| 内核路径 | `arch/arm/configs/` | `arch/arm64/configs/` |
| 指令密度 | Thumb2 高密度 | AArch64 标准密度 |
| MMU | 2级页表 | 4级页表 |
| 内核体积 | 更小 | 更大 |
| 显示接口 | RGB/MIPI DSI | MIPI DSI |
| GIC/中断 | GICv2 | GICv2 + SCMI |
| TB 内核行数 | 388 | 488 |

**结论**：ARM32 天然比 ARM64 启动更快。RK3562 不可能直接用 RK3506 的方案，需要适配。

## 内核配置对比

### 相同优化（已实施 ✅）
| 优化项 | 3506 | 3562 | 状态 |
|--------|------|------|------|
| KERNEL_LZ4 | ✅ | ✅ | 一致 |
| PREEMPT | ✅ | ✅ | 一致 |
| MINI_KERNEL | ✅ | ✅ | 一致 |
| 禁用 CGROUPS | ✅ | ✅ | 一致 |
| 禁用 MEDIA/CAMERA | ✅ | ✅ | 一致 |
| 禁用 SOUND | ✅ | ✅ | 一致 |
| 禁用 PCIe | ✅ | ✅ | 一致 |
| SquashFS ZSTD | ✅ | ✅ | 一致 |
| 禁用 FTRACE/SCHED_DEBUG | ✅ | ✅ | 一致 |
| FIQ debugger + ttyFIQ0 | ✅ | ✅ | 一致 |
| SERIAL_8250_DW disabled | ✅ | ✅ | 一致 |

### 3506 有、3562 缺失（待实施 ❌）
| 优化项 | 作用 | 预期节省 |
|--------|------|---------|
| CC_OPTIMIZE_FOR_SIZE | 内核 -Os 编译，缩小 15% | ~100ms 加载时间 |
| LD_DEAD_CODE_DATA_ELIMINATION | 链接时去除死代码 | ~50ms |
| CMA_INACTIVE + CMA_SIZE=0 | 惰性 CMA 初始化 | ~200ms |
| CONFIG_BUG=n | 移除 BUG/WARN | ~50ms |
| CONFIG_BASE_FULL=n | 精简基础功能 | ~100ms |
| 仅 ondemand CPU governor | 少加载调速器 | ~50ms |

### 3562 有、但不需要（待砍 ❌）
| 多余项 | 作用 | 影响 |
|--------|------|------|
| ATA + NVMe | SATA/NVMe 驱动 | ~100ms probe |
| SCSI disk | SCSI 磁盘 | ~50ms |
| 4种 CPU governor | 多余的调速器 | ~100ms |
| ARM SMMU v3 | 系统 MMU | ARM64 特有 |
| SCMI | 系统控制接口 | ARM64 特有 |
| AHCI_DWC | SATA 控制器 | 不需要 |

## Init 机制对比

| 特性 | RK3506 | RK3562 (当前) |
|------|--------|---------------|
| 分层 init | rcS.early → pre_init → rcS | ❌ 未实施 |
| LVGL 提前启动 | S05lv_demo.sh → pre_init | ❌ 未实施 |
| GPU 异步提交 | S00async-commit.sh | ❌ 未实施 |
| Recovery 处理 | post-build 删除 | ✅ 已处理 |

## 启动时间对比

| 阶段 | RK3506 | RK3562 (优化前) | RK3562 (当前) |
|------|--------|-----------------|---------------|
| DDR | ~1.2s | ~1.5s | ~1.5s |
| U-Boot | ~1.6s | ~2.2s | ~2.2s |
| 内核到 init | ~2.2s | ~2.7s | ~2.7s |
| 内核最后消息 | ~3s | ~16s | ~5.3s |
| 到 login 提示符 | ~6s | ~18-22s | ~9s |
| **LVGL 出画面** | **~3s** (内核时间) | N/A | N/A |

## 剩余差距分析

RK3562 距离 3s 出 LVGL 画面还需要：

1. **内核精简** (预期省 0.5-1s)
   - 加 SIZE 优化、死代码消除、CMA_INACTIVE
   - 砍 ATA/NVMe/SCSI/多余 governor

2. **pre_init LVGL** (预期提前 1-2s 出画面)
   - 实现 rcS.early → pre_init 机制
   - 在 init 启动同时启动 LVGL，不等后台服务

3. **U-Boot 优化** (预期省 0.5-1s)
   - 精简显示初始化
   - 移除多余外设探测

4. **DDR 训练** (~1.5s 固定)
   - 硬件限制，难以进一步减少

**预计最终能达到的 LVGL 出画面时间：~4-5 秒内核时间（~7 秒 power-on）**

## 架构限制

- RK3562 的 ARM64 架构意味着内核更大、MMU 更复杂、启动天生比 ARM32 慢 0.5-1s
- RK3562 需要完整的 DRM/DSI 管线来驱动 MIPI 显示——这是 LVGL 显示所需要的，不能移除
- DDR 容量 1GB vs RK3506 可能更小，DDR 训练时间更长

RK3562 不可能达到 RK3506 的绝对 3s 内核时间，但可以达到 ~4-5s 内核时间出 LVGL 画面。
