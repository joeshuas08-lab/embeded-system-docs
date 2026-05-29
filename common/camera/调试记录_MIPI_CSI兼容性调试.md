# MIPI-CSI 兼容性调整：v2 板卡适配

## 问题现象

- v2 板卡摄像头无法打开
- dmesg 报 gc05a2 probe 失败
- VBUS 电压为 0

## 根因分析

v2 板修改了 Type-C 和摄像头共用的 VBUS 供电方案。v1 板用 GPIO 控制 regulator-fixed 来使能 VBUS，v2 板的这个 GPIO 被 bq25890 充电器中断占用了。

**v1 方案**：
```dts
vbus5v0_typec: vbus5v0-typec {
    compatible = "regulator-fixed";
    enable-active-high;
    gpio = <&gpio3 RK_PB0 GPIO_ACTIVE_HIGH>;
};
```

**v2 方案**：硬件将 VBUS 改为常供电，不再需要 GPIO 控制。所以 dts 中注释掉 `enable-active-high` 和 `gpio` 属性。

## 本质理解

这个问题的本质是 **pinmux 功能复用冲突**。同一个 GPIO pad 在 v1 板上作为 VBUS 使能脚，在 v2 板上被充电器 IC 用作中断脚。如果两个驱动都 claim 这个 pad，第二个 probe 的驱动会失败。

**判断冲突的方法**：
```bash
# 查看 pinmux 状态
cat /sys/kernel/debug/pinctrl/pinctrl-handles | grep gpio3
dmesg | grep "pinctrl\|pin request"
```

## 适用场景（什么时候会遇到这类问题）

板卡迭代时供电方案变更会导致此类问题，不限于 MIPI-CSI：
- Type-C VBUS
- 摄像头 AVDD enable
- 触摸屏复位 GPIO
- LCD 电源使能

**复用 GPIO 的板级变更都需要检查**是否和其他设备的中断 / control GPIO 冲突。

## 验证清单

```bash
# 摄像头能 probe
dmesg | grep gc05a2

# VBUS 电压正常
cat /sys/class/regulator/*/name | grep vbus
cat /sys/class/regulator/*/microvolts

# 检查 pinmux 无冲突
cat /sys/kernel/debug/gpio | grep 3_PB0
```

## Commit 索引

- `75c599979dc` — 注释掉 GPIO enable + pinctrl，适配 v2 板
