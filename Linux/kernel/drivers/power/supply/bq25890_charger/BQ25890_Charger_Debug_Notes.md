# BQ25890 充电调试笔记 (RK3576 + Android 14)

> 寄存器映射基础参考 [BQ25890_IINLIM_Driver_Notes.md](./BQ25890_IINLIM_Driver_Notes.md)

## 涉及提交

| 提交 | 功能 |
|------|------|
| `50b2c46f12c` | add charger bq25890 |
| `074c4c05862` | add gas gauge & add battery & amend power key |
| `c403e443988` | add poweroff quick charge |
| `bf482f44061` | fix : pwr-off quick charge |
| `1391d9491b1` | Reapply "add poweroff quick charge" |
| `626e8f33ef4` | [Fix]uboot bq25890 issue |
| `68d43560dee` | Fix : Limit the Current within 1 Ampere |
| `f99cef6bd32` | Fix : Limit the Current within 1 Ampere |
| `dbb38cfc9c8` | add : update for 3900mAh 3.85V battery |
| `57c53751430` | [FEAT]compatible with new bat |
| `2976f44b735` | [Fix]compatible with new bat |

---

## 一、充电配置

### 1.1 PD 充电协商限流

`bq25890_set_pd_param()` 中限制最大充电电流 1A：

```c
static void bq25890_set_pd_param(struct bq25890_device *bq, int vol, int cur)
{
    if (cur > 1000000)
        cur = 1000000;              // 强制限制 1A
    iilim = bq25890_find_idx(cur, TBL_IINLIM);
    bq25890_field_write(bq, F_IINLIM, iilim);
}
```

### 1.2 USB 类型检测限流

```c
case POWER_SUPPLY_USB_TYPE_DCP:
    input_current_limit = bq25890_find_idx(1000000, TBL_IINLIM);  // DCP: 1A
    break;
case POWER_SUPPLY_USB_TYPE_SDP:
default:
    input_current_limit = bq25890_find_idx(500000, TBL_IINLIM);   // SDP: 500mA
```

### 1.3 关机充电默认值

```c
static void bq25890_shutdown(struct i2c_client *client)
{
    regmap_field_write(bq->rmap_fields[F_IINLIM], 0x1F);  // 1650mA
    regmap_field_write(bq->rmap_fields[F_ICHG], 0x3F);    // 4032mA (=63×64mA)
}
```

**注意**：关机充电状态由 Uboot 控制，`626e8f33ef4` 修复了 Uboot 中 BQ25890 的初始化时序问题。

---

## 二、关机快充

### 2.1 实现原理

关机快充允许设备在关机状态下（系统未启动）通过 BQ25890 进行充电，充电状态由 Uboot 管理。

提交 `c403e443988` 添加，后被 `bd94c8735de` 临时回退，最终在 `1391d9491b1` (Reapply) 重新应用。

### 2.2 Uboot 配置

```c
// Uboot 驱动中使能充电
#define CONFIG_BQ25890_CHARGER
```

### 2.3 修复点 (`bf482f44061`)

问题：关机快充在某些条件下无法正确检测充电插入/拔出。

根因：GPIO 中断配置不正确，充电器插入时电平检测方向反了。

```c
// 修复前
gpio_direction_input(charger->chrg_gpio);

// 修复后
gpio_direction_input(charger->chrg_gpio);
gpio_set_debounce(charger->chrg_gpio, DEBOUNCE_TIME);
```

---

## 三、电池兼容性

### 3.1 新旧电池兼容 (`57c53751430`, `2976f44b735`)

| 电池版本 | 容量 | 电压 |
|---------|------|------|
| 旧版 | 3500mAh | 3.7V |
| 新版 | 3900mAh | 3.85V |

兼容策略：
1. DTS 中添加电池匹配表，通过电池 ID 引脚自动检测
2. gas gauge (BQ27541) 重新配置 chemistry profile
3. 充电电压从 4.2V 调整到 4.4V（适配 3.85V 电池）

### 3.2 3900mAh 电池参数

提交 `dbb38cfc9c8` 更新：
- 满充电压：4.4V
- 充电电流：1500mA (max)
- 截止电流：128mA
- 容量：3900mAh

```c
// BQ27541 gauge 配置
{
    .design_capacity = 3900,        // mAh
    .design_voltage = 3850,         // mV
    .termination_voltage = 4400,    // mV (4.4V)
}
```

### 3.3 BQ27541 移植 (`1fc80beab30`)

使用 BQ27541 作为 fuel gauge，I2C 地址 0x55：

```
bq27541: bq27541@55 {
    compatible = "ti,bq27541";
    reg = <0x55>;
};
```

---

## 四、限流问题排查

### 4.1 问题

`68d43560dee` / `f99cef6bd32`：设备充电电流超过 1A，导致电源适配器过载保护。

### 4.2 根因

PD 协商后充电电流未正确 clamp。BQ25890 支持最大 3.25A IINLIM，PD 协商到更高电流时直接使用协商值。

### 4.3 修复

在 PD 协商回调中强制限制最大电流：

```c
#define MAX_CHARGE_CURRENT_UA  1000000   // 1A

if (cur > MAX_CHARGE_CURRENT_UA)
    cur = MAX_CHARGE_CURRENT_UA;
```

---

## 五、调试命令

```bash
# 查看充电状态
cat /sys/class/power_supply/bq25890-charger/uevent

# 查看电池信息
cat /sys/class/power_supply/battery/uevent

# 设置输入电流限制
echo 500000 > /sys/class/power_supply/bq25890-charger/input_current_limit

# I2C 直接读取 BQ25890 寄存器
i2cget -f -y 6 0x6a 0x00   # REG00: IINLIM
i2cget -f -y 6 0x6a 0x04   # REG04: ICHG
i2cget -f -y 6 0x6a 0x06   # REG06: VREG

# I2C 读取 BQ27541 电量计
i2cget -f -y 6 0x55 0x00 w  # 当前电量百分比
i2cget -f -y 6 0x55 0x02 w  # 电池电压
i2cget -f -y 6 0x55 0x04 w  # 当前电流
i2cget -f -y 6 0x55 0x10 w  # 剩余容量
```

---

## 六、经验总结

1. **关机快充**：Uboot 驱动需适配 USB 插入/拔出检测，deglitch 时间设置合理避免误触发
2. **限流策略**：PD 协商值不能直接使用，需考虑硬件散热能力和电源适配器规格
3. **大容量电池**：3900mAh/3.85V 电池需要更新 BQ27541 的 chemistry ID，否则电量显示不准确
4. **VREG 设置**：3.85V 电池的满充电压为 4.4V，比常规 3.7V 电池的 4.2V 高，需修改充电电压配置
