# 唤醒策略补丁（两级）

对应内核/框架仓库的两个 commit，可用 `git am` 直接打进对应仓库：

- `0001-alarm-only-allowlisted-packages-may-arm-wakeup-alarm.patch`
  （第一级，默认交付）：AlarmManagerService 白名单，只有 com.cry.acoustic_pocket
  的唤醒闹钟能唤醒设备，其余降级为非唤醒型。
- `0002-rtc-forbid-RTC-IRQ-from-resuming-the-SoC-fallback-V6.patch`
  （最后一级，兜底）：DTS + hym8563 驱动，把 RTC 中断移出唤醒路径，完全禁止
  RTC 定时唤醒。

顺序：先 0001 后 0002。方案说明见 ../唤醒策略说明_两级方案.md。
