# PCIe 调试：Pangu EP 驱动打印清理 + ASPM 省电优化

## 问题现象

- kernel log 被 PCIe 驱动统计信息刷屏（每 5 秒一次）
- dmesg 无法看到其他模块的有效日志
- PCIe 功耗在待机时偏高

## 排查路径

```
printk 刷屏 → 找源头（debug_timer_cbk + fpga_stop_dma）→ printk 改 pr_debug
功耗偏高    → 查 ASPM 配置 → POWERSAVE 改成 POWER_SUPERSAVE (L1.2)
```

## 根因分析

### 1. 驱动日志刷屏

Pangu 驱动实现的 debug timer 回调函数 `debug_timer_cbk` 每 5 秒执行一次，用 `printk` 直接输出：

```c
void debug_timer_cbk(struct timer_list *arg)
{
    printk("irq count : %llu interval : %llu us, total data len : %llu\n", ...);
    printk("timeout count : %llu, irq last time : %llu us \n", ...);
    mod_timer(&debug_timer, jiffies + msecs_to_jiffies(5000));  // 重调度
}
```

`printk` 的特性：无论 loglevel 如何，只要 console 没过滤掉，一定会输出。在量产内核中应该使用 `pr_debug`，它只在 `DEBUG` 或 `CONFIG_DYNAMIC_DEBUG` 开启时才输出。

**改动**：
```
printk → pr_debug（debug_timer_cbk + fpga_stop_dma 两处）
```

**如果刷屏问题再次出现**：
```bash
# 找刷屏源头
dmesg | tail -20
# 看到规律性每 5 秒的 irq/timeout 统计 → go to driver/pci/driver/

# 临时压制（不改代码）
echo "file driver.c -p" > /sys/kernel/debug/dynamic_debug/control
```

### 2. ASPM 省电模式

ASPM 通过在链路空闲时关闭时钟 / 电源来省电。三种策略：

| 策略 | config 选项 | 时钟状态 | 电源状态 | 退出延迟 |
|------|-----------|---------|---------|---------|
| 关闭 | ASPM 关 | 运行 | 运行 | 0 |
| POWERSAVE | L1.1 | 停止 | 运行 | ~1μs |
| POWER_SUPERSAVE | L1.2 | 停止 | 关闭 | ~10μs |

选择 L1.2 的依据：FPGA EP 设备的数据传输是间歇性的（非实时流），链路大部分时间空闲。L1.2 在空闲时关闭主电源，省电效果显著。代价是退出延迟 ~10μs，但 FPGA 驱动中的 DMA buffer 足够大，可以容忍这个延迟。

## 验证清单

```bash
# 1. 确认无 printk 刷屏
dmesg | grep -c "irq count"  # 应为 0

# 2. 需要调试时动态开启
echo "file driver.c +p" > /sys/kernel/debug/dynamic_debug/control

# 3. 确认 ASPM 策略
cat /sys/module/pcie_aspm/parameters/policy
# 输出应含 powersupersave

# 4. 确认 PCIe 设备 link 正常
lspci -vvv | grep -E "LnkSta|ASPM"

# 5. 测量待机功耗
# 对比更换前后功耗差
```

## 典型失败模式

| 症状 | 最可能原因 | 第一检查点 |
|------|-----------|-----------|
| 每 5 秒 kernel log 跳固定输出 | debug_timer 用 printk | grep printk driver/pci/driver |
| PCIe 设备 link up 失败 | PERST# 时序 / REFCLK 不准 | 示波器测 REFCLK |
| 数据传输丢包 | L1.2 退出太慢导致 buffer underrun | 关 ASPM 对比测试 |
| probe 到 -ENODEV | 供电 GPIO 未使能 | cat /sys/kernel/debug/regulator |
| EP 设备枚举成 Unknown | BAR 空间映射失败 | lspci -vvv 查 BAR |

## Commit 索引

- `e12e44085c9` — Pangu 驱动 printk 改 pr_debug（driver.c + pangu.c）
- `28356cc3b85` — rockchip_linux_defconfig: POWERSAVE → POWER_SUPERSAVE (L1.2)
