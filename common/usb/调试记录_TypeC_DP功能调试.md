# Type-C DP Alt Mode 功能修复

## 问题现象

- 插 Type-C 转 DP/HMDI 线 → 显示器无反应
- dmesg 没有任何 dp/typec 相关输出
- 显示器偶尔闪一下即断开

## 排查路径

```
插线 → 显示器没反应

1. Type-C 进入 Alt Mode 了吗？
   → cat /sys/class/typec/port0/current_mode
   (空 = 没协商, dp = 成功了)

2. PD 协商完成了吗？
   → dmesg | grep -E "tcpm|pd|altmode"
   (没日志 = TCPM/TCPC 没工作)

3. DP 控制器 enabled？
   → cat /sys/kernel/debug/dri/0/status | grep DP
```

## 根因分析

### 1. DP SVID 冲突（主因）

DP Alt Mode 协商依赖 SVID（Standard Vendor ID）。VESA 为 DisplayPort 分配的 SVID 是 `0xff01`。问题是 RK3576 的 Type-C 框架中，USB PD 驱动（tcpm/tcpci）已经内置了 VESA SVID，不需要在 dts 中重复指定。

在 dts 中加 `svid = <0xff01>` 反而导致 PD 策略冲突 —— TCPM 在协商 Alt Mode 时会把 SVID 发两次或发到错误的 SOP，显示器拒绝进入 DP 模式。

```diff
 &dp {
     status = "okay";
-    svid = <0xff01>;
 };
```

**判断方法**：拔掉 Type-C 线然后插上，同时抓 dmesg：
```
# 正常（无 svid 属性）：
tcpm: PD message, 0: 0x1ad1 (SVID=ff01)
tcpm: Alt mode entered, SVID=ff01

# 异常（有 svid 属性）：
tcpm: PD message, 0: 0x1ad1 (SVID=ff01)
tcpm: SVID mismatch, ignore
```

### 2. test-power 干扰 PD 协商

`test-power` 是 Rockchip 的虚拟电源设备，用于 uboot 充电显示。这个设备在 kernel 中会注册一个 `power_supply` 设备，并尝试介入 VBUS 管理。当 Type-C 控制器也管理 VBUS 时，两者冲突。

```diff
-test-power {
-    status = "okay";
-};
+test-power {
+    status = "disabled";
+};
```

**根本原因**：`test-power` 会通过 power_supply 框架的 `set_online` 改变 VBUS 状态，Type-C 控制器以为 VBUS 已经被别的驱动控制了，跳过 PD 协商。

## 验证清单

```bash
# 1. Type-C Alt Mode 状态
watch -n 1 cat /sys/class/typec/port0/current_mode
# 插线后应显示 "dp"

# 2. DP 控制器
cat /sys/kernel/debug/dri/0/status | grep -i dp

# 3. PD 协商日志
dmesg | grep -E "tcpm|altmode|SVID"

# 4. EDID（显示器识别）
cat /sys/kernel/debug/dri/0/DP-1/edid | hexdump -C

# 5. USB 速度（确认 3.0 模式）
cat /sys/bus/usb/devices/1-1/speed  # 应为 5000

# 6. 热插拔压力测试
for i in {1..50}; do
    echo "Plug cycle $i"
    sleep 3
    # 检查 current_mode
    cat /sys/class/typec/port0/current_mode
done
```

## 典型失败模式

| 症状 | 最可能原因 | 第一检查点 |
|------|-----------|-----------|
| 插线无任何反应 | SVID 冲突 / test-power 干扰 | dmesg 搜 tcpm |
| 显示器闪一下即断 | PD 协商超时 | 抓 PD 日志看 hard reset |
| 只有 1080p 没有 4K | lane 数不够 / link rate 低 | cat DP-1/link_status |
| USB 只能 2.0 速度 | USB3 信号完整性差 | cat /sys/bus/usb/devices/1-1/speed |
| 热插拔几次后失效 | TCPM 状态机卡住 | cat typec/port0/port_type |

## Commit 索引

- `e7206cc8af6` — 删除 DP node 中 svid 属性 → Alt Mode 协商正常
- `fbf3d56fe48` — test-power status disabled → 消除 PD 协商干扰
