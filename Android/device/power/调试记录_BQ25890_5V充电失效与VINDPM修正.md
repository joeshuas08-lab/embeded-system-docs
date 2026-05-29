# 调试记录：BQ25890 5V 充电失效与 VINDPM 寄存器修正

## 问题现象

- **9V PD 适配器**：充电正常，`/sys/class/power_supply/bq25890-charger/status` 显示 "Charging"
- **5V 适配器（普通 USB-A 或 5V PD）**：不充电，status 显示 "Not charging"
- 设备：RK3576 + BQ25890（或兼容芯片 SY6970 / BQ25895）

## 排查路径

```
1. 确认 5V 适配器插入后 PD 协商是否成功
   → cat /sys/class/power_supply/usb/uevent 确认 PD 电压/电流已读到
2. 检查充电器驱动是否收到了 PD 通知
   → dmesg | grep "bq25890_set_pd_param" 查看 vol/cur 参数
3. 对比 5V 和 9V 时的寄存器写入值
   → 5V 时 VINDPM 计算值是否把输入电压阈值抬得过高
```

## 根因分析

### VINDPM 寄存器类型混淆

bq25890 系列有两类 VINDPM 配置方式：

| 芯片型号 | 寄存器 | 类型 | 行为 |
|---------|--------|------|------|
| BQ25892 | `F_VINDPM_OFS` (Reg0E) | 偏移量 | 值 = 从 VBUS 减去多少 mV 触发 VINDPM |
| BQ25890/95/96, SY6970 | `F_VINDPM` (Reg0D) | 绝对值 | 直接设定 VINDPM 阈值电压 (2.6V - 15.3V) |

**原始代码对所有芯片都写 `F_VINDPM_OFS` 偏移寄存器**：

```c
// 错误：在 BQ25890 上写偏移寄存器，该寄存器不存在或无预期行为
bq25890_field_write(bq, F_VINDPM_OFS, vindpm);
```

对于 BQ25890，`F_VINDPM_OFS` 寄存器不存在或映射到其他功能。VINDPM 阈值保持默认值（约 4.5V 左右），导致：

- **9V 输入**：VBUS = 9V > 默认 VINDPM 阈值 → VINDPM 不触发 → 充电正常
- **5V 输入**：VBUS = 5V ≈ 默认 VINDPM 阈值 → VINDPM 间歇触发或直接拉低输入 → **不充电**

这就是"9V 能充、5V 不能充"的根因。

### 修复方案

根据芯片型号分支处理：

```c
if (bq->chip_version == BQ25892) {
    // BQ25892: 使用偏移量寄存器
    bq25890_field_write(bq, F_VINDPM_OFS, vindpm);
} else {
    // BQ25890/95/96, SY6970: 使用绝对值寄存器
    int vindpm_volt = vol - 800000;  // 留 0.8V 裕量
    if (vindpm_volt < 2600000) vindpm_volt = 2600000;
    if (vindpm_volt > 15300000) vindpm_volt = 15300000;
    int vindpm_reg = (vindpm_volt - 2600000) / 100000;
    bq25890_field_write(bq, F_VINDPM, vindpm_reg);
}
```

**关键设计决策——0.8V 裕量**：
- 5V 输入时 VINDPM 设为 4.2V，低于 VBUS 5V，保证不会误触发
- 9V 输入时 VINDPM 设为 8.2V，同样留有裕量
- 裕量太小 → 线缆压降可能触发 VINDPM；裕量太大 → 失去输入欠压保护意义

### 附带修复：PD 属性读取失败的回退

原始代码中，如果 `POWER_SUPPLY_PROP_CURRENT_NOW` 或 `POWER_SUPPLY_PROP_VOLTAGE_NOW` 读取失败，整个 PD 通知处理直接 return，充电参数不被设置，IINLIM 保持默认 100mA。

修复后添加了 fallback 逻辑：

```c
// 电流读取失败 → 默认 1.0A
if (ret == 0 && prop.intval > 0)
    bq->pd_cur = prop.intval;
else
    bq->pd_cur = 1000000;  // 5V 非 PD 适配器的合理默认值

// 电压读取失败 → 默认 5V
if (ret == 0 && prop.intval > 0)
    bq->pd_vol = prop.intval;
else
    bq->pd_vol = 5000000;
```

### 附带修复：充电使能位显式置位

硬件初始化时显式写 `F_CHG_CFG = 1` 使能充电。部分批次芯片复位后该位默认为 0（禁止充电），不显式置位表现为插电不进充。

## PDO 广播调整

设备树中的 sink PDO（设备向适配器宣称自己能接受的电压/电流组合）调整为：

```dts
sink-pdos =
    <PDO_FIXED(5000, 1800, PDO_FIXED_USB_COMM)   // 5V/1.8A = 9W
     PDO_FIXED(9000, 1000, PDO_FIXED_USB_COMM)>;  // 9V/1.0A = 9W
```

两个档位均为 9W，热耗一致，适配器兼容性最优。

## 调试技巧

### 快速判定 VINDPM 是否误触发
```bash
# 读 Reg0D (VINDPM) 和 Reg0E (VINDPM_OFS)
i2cget -y 0 0x6a 0x0d
i2cget -y 0 0x6a 0x0e
# 比较 VBUS 实测电压 vs VINDPM 阈值
cat /sys/class/power_supply/usb/voltage_now
```

### 追踪 PD 协商到充电参数设置的完整链路
```bash
# 1. PD 协商结果
cat /sys/class/power_supply/usb/uevent
# 2. bq25890 收到的参数
dmesg | grep "bq25890_set_pd_param"
# 3. 确认 IINLIM 和 ICHG 已更新
cat /sys/class/power_supply/bq25890-charger/input_current_limit
cat /sys/class/power_supply/bq25890-charger/constant_charge_current
# 4. 充电状态
cat /sys/class/power_supply/bq25890-charger/status
```

## 设计原则总结

1. **寄存器兼容性**：同一驱动适配多颗芯片时，差异寄存器必须按 chip_version/device_id 分支，不能假设寄存器映射一致
2. **PD 协商的容错性**：USB PD 属性读取可能因时序问题失败，驱动必须设合理默认值，不能直接 return 让充电器停在 100mA
3. **充电使能位**：关键控制位不要依赖"上电默认值"，必须在 init 序列中显式置位
4. **PDO 设计**：多电压档位应保持功率一致，避免适配器选到低功率档位导致充电慢

---

*编写：高级工程师*
*更新：2026-05-20*
*基于 commit：66d69a67942, 68d43560dee, f99cef6bd32, bf482f44061*
