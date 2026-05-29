# HYM8563 RTC 休眠唤醒 + 定时关机 调试笔记

## 需求
设备进入 mem 休眠后，HYM8563 RTC 闹钟能唤醒系统，实现"休眠超时自动关机"功能。

## 硬件改动

### 飞线
HYM8563 Pin 3 (INT, 开漏低有效) → SoC GPIO0_C5
VDD_3V3 上拉板上已有，只需飞一根信号线。

### GPIO0_C5 选择原因
- GPIO0 bank 属于 PMU 常电域，mem sleep 时保持供电，可唤醒 SoC
- 板子 DTS 编译链中该引脚未被占用（PCIe 复位用 C7、USB OTG 用 C6）

## DTS 修改

文件：`kernel-6.1/arch/arm64/boot/dts/rockchip/myd-lr3576.dts`

### 1. pinctrl 新增
```dts
hym8563 {
    hym8563_int: hym8563-int {
        rockchip,pins = <0 RK_PC5 RK_FUNC_GPIO &pcfg_pull_up>;
    };
};
```

### 2. RTC 节点新增
```dts
rtc: hym8563@51 {
    ...
    pinctrl-names = "default";
    pinctrl-0 = <&hym8563_int>;
    interrupt-parent = <&gpio0>;
    interrupts = <RK_PC5 IRQ_TYPE_LEVEL_LOW>;
    wakeup-source;
};
```

## 软件链路

休眠超时关机（Android 层，已修改 PowerManagerService.java）:
  sleep → scheduleSleepShutdownAlarm()
    → AlarmManager.setExact(RTC_WAKEUP)
      → alarmtimer (kernel)
        → hym8563_rtc_set_alarm()        ← 写入硬件闹钟寄存器
        → hym8563_rtc_alarm_irq_enable() ← 使能 AIE 中断

闹钟触发唤醒:
  HYM8563 INT 拉低
    → GPIO0_C5 中断 → SoC 唤醒
      → hym8563_irq() ISR（清除 AF/TF 标志）
        → alarmtimer 投递闹钟事件
          → mSleepShutdownAlarmListener.onAlarm()
            → shutdownOrRebootInternal(HALT_MODE_SHUTDOWN)

定时器 vs 闹钟寄存器:
  - 超时 ≤ 255秒 → 硬件倒计时定时器 (TIE)
  - 超时 > 255秒  → 硬件闹钟寄存器 (AIE)
  - 驱动 hym8563_rtc_set_alarm() 自动判断

## 休眠超时时间配置

```sh
# 设置超时秒数（默认 300，即 5 分钟）
setprop persist.sys.sleep_shutdown_time 300

# 禁用功能
setprop persist.sys.sleep_shutdown_time 0
```

## 验证方法

```sh
# 1. 确认 RTC 中断已注册
cat /proc/interrupts | grep hym8563

# 2. 检查 wakeup 源
cat /sys/class/rtc/rtc0/device/power/wakeup

# 3. 手动闹钟唤醒测试（设 60 秒后闹钟，然后 suspend）
echo +60 > /sys/class/rtc/rtc0/wakealarm
echo mem > /sys/power/state
# 等待 60 秒，系统应自动唤醒

# 4. 休眠超时关机日志
logcat -s PowerManagerService | grep sleep_shutdown
```

## 关键文件

| 文件 | 说明 |
|------|------|
| `myd-lr3576.dts` | DTS 中断 & wakeup 配置（本次修改） |
| `rtc-hym8563.c` | RTC 驱动，suspend/resume wakeup 处理 |
| `PowerManagerService.java` | Android 层休眠超时关机逻辑（之前修改） |
| `rk3576-evb.dtsi:656-667,696-698` | Rockchip 参考设计（GPIO0_PA0，本次未采用） |
