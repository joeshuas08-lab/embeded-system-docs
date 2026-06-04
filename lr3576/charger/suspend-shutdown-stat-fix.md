# BQ25890 STAT LED 休眠关机行为修复

**日期:** 2026-06-04
**依赖:** `bq25890_stat_disable_no_vbus.md`

## 问题

STAT_DIS 引入后，休眠和关机状态下插拔充电器不会亮灯。

## 根因

STAT_DIS=1（LED 禁用）设置为常规无电状态。休眠/关机时驱动不运行，若 STAT_DIS=1 则 BQ25890 硬件检测到 VBUS 也无法驱动 STAT 引脚。

## 修复

`suspend` 和 `shutdown` 进入前强制写 `STAT_DIS=0`，让硬件自行响应 VBUS 插拔：

```c
// bq25890_suspend()
bq25890_field_write(bq, F_STAT_DIS, 0);  // enable STAT before suspend
return bq25890_field_write(bq, F_CONV_RATE, 0);

// bq25890_shutdown()
regmap_field_write(bq->rmap_fields[F_STAT_DIS], 0);  // enable STAT before shutdown
```

## 适用场景

- 休眠时插拔充电器
- 关机时插拔充电器（VSYS 未掉电，因电池在线）
