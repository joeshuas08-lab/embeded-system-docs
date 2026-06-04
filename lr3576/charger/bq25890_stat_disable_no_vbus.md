# BQ25890 STAT 充电指示灯未插充电器时闪烁问题

**日期:** 2026-06-04
**平台:** RK3576 (LR3576 底板)
**芯片:** BQ25890 / SC89890H
**提交:** `charger: bq25890: suppress STAT LED blink when charger absent`

## 问题

未插充电器时充电指示灯异常闪烁。预期未插线时 LED 熄灭，实际持续闪烁。

## 复现

1. 板子电池供电，不插 USB 充电线
2. 观察充电指示灯（暖白色 LED，接 BQ25890 STAT 引脚）
3. LED 持续闪烁（~1Hz）

## 根因

1. NTC 开路使 TSPCT=0x7F(-21°C)，触发 NTC_FAULT=COLD，STAT 引脚输出 1Hz 闪烁
2. 驱动每次 `get_chip_state()` 读取 REG0C 会清除故障锁存位，故障立即重新触发，形成连续闪烁循环
3. 卸载驱动后闪烁停止，证实是驱动活动导致
4. VBUS 判断应用 VBUS_STAT 而非 PG_STAT，后者在 NTC 故障时会误报

## 修复

`bq25890_get_chip_state()` 中先读 VBUS_STAT + PG_STAT，两者均为 0 时：
1. 写 STAT_DIS=1 + CONV_RATE=0
2. 不读 REG0C（避免触发故障清除-重触发循环）
3. 直接返回

有 VBUS 时正常读全部状态寄存器。

suspend/shutdown 进入前写 STAT_DIS=0，让 BQ25890 硬件自行处理插拔。详见 [suspend-shutdown-stat-fix.md](suspend-shutdown-stat-fix.md)

## 相关配置

- CONFIG_CHARGER_BQ25890=y，rk3576.config 中不应禁用
- REG07 加入 regmap volatile 范围
- 休眠/关机细节见子文档
