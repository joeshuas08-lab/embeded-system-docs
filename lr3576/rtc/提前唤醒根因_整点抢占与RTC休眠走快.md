# 提前唤醒根因分析：整点抢占 + RTC 休眠走快

**日期**：2026-09（V6 板）
**现象**：
- 未充电 + 5min alarm → 几十秒就醒（问题 1）
- 充电 + 1h alarm → 几十秒就醒；1-15min 正常；提前量随设置时刻离整点的距离变化（问题 2）

**结论**：两个完全独立的根因，叠加造成上述现象。

---

## 根因 1：RTC 芯片休眠期间走快 ~1.9%（问题 2 主因）

### 测量（三个独立测试，速率一致）

| 测试 | 休眠时长（boot 时钟） | RTC 走的时间 | 速率 |
|------|---------------------|-------------|------|
| +120s wakealarm | 70.9s | 72s | +1.6%（短窗口，含 ±1s 取整噪声）|
| 自然测试（32min）| 1841.2s | 1876s | **+1.89%**（提前 35s 触发）|
| +1800s wakealarm（43min）| 2515.4s | 2564s | **+1.93%** |
| 清醒态 12min 采样（5 点）| — | 偏移恒定 | **0.000%** |

**修正**：是"休眠期间走快"，清醒态不漂。1h alarm → 提前 ~68s 触发 = 客人"设 1 小时几十秒就醒"的直接来源。

### 硬件线索（供硬件组排查）

- 只在 mem 休眠态漂移 → 疑 VDD 偏移或 32k 振荡器负载变化（RTC 的 CLKOUT 在 DTS 有 `clock-output-names` 但本板无消费者）
- 晶体负载电容 / 供电在休眠态需实测

### 软件修复（已刷机验证）

`kernel-6.1/drivers/rtc/rtc-hym8563.c` → `hym8563_rtc_set_alarm()`：
把请求间隔 **×1.02 + 向上取整到整分** 再写寄存器 —— 芯片走快 → 往后多设 2% → 触发点落回正确时刻。

关键点（改代码时必须保持）：
- 补偿必须在 `tm_sec = 0` 截断**之前**做（芯片只比较 MIN+HOUR，截断最多吃掉 59s，先截断再补会重新提早）
- `+ (alarm-now)/50 + 59` 再截断 = `ceil((alarm + 2%) 到整分)`
- 效果：**绝不早响，最多晚 ~1 分钟**

commit `737a8c44168` rtc: hym8563 pad alarm for fast suspend clock [V6 board]

### 验证（修复后）

| 测试 | 结果 |
|------|------|
| 55min 真实路径（Android alarm → RTC）| 请求 55m23s，实际晚 ~45s 醒（修复前推算会提前 ~64s）|
| 未充电 5min × 4 轮 | 睡眠 292-323s，全部由 RTC alarm 唤醒，+13~+53s 迟到 |

---

## 根因 2：整点抢占（问题 2 副因）

### 机理

每小时 :00 必然有系统 RTC_WAKEUP 精确闹钟排在队首：

1. **Settings 电池统计 job**（`PeriodicJobManager`，AOSP 14 默认行为）
   - `setExactAndAllowWhileIdle(RTC_WAKEUP)` 排到下一个整点，触发后自动重排下一整点 → 每小时必醒
2. **DND 默认日程规则"睡眠"**（`default_zen_mode_config.xml` 预置 22:00-07:00）
   - `ZenModeConditions` 对每条 automatic rule 都建立订阅（**即使 enabled=FALSE**，ZenModeConditions.java:164）→ `ScheduleConditionProvider.EVALUATE` 每天 22:00/07:00 排精确闹钟

### 证据

- **边界扫描 10/10 吻合**：alarm 触发点跨整点即被抢，否则不被抢
  （30min @23:26 → 正常；35min 触发点跨 00:00 → Next wakeup 变 00:00:00.001）
- **自然状态内核实拍**：1h alarm（触发 00:27）设后休眠 → 内核编程的 min=32min（正是 00:00 整点对），RTC 在 00:00:00 唤醒
- **判据**：`fired.txt` 无新增 = 只是亮屏，用户 alarm 未触发（还挂着）
- **提前量公式**：提前量 = 设置时刻离下一个整点的距离（:59:30 设 1h → 30 秒后就被唤醒）

### 修复（本地已完成，待推送验证）

| 项 | 改动 | 效果 |
|----|------|------|
| A. Settings 每小时 job | `PeriodicJobManager.refreshJob()`：`MYIR_ENABLE_PERIODIC_JOB=false` 时取消已挂 alarm 并 return | 每小时 :00 唤醒消失 |
| B. DND 默认规则 | `default_zen_mode_config.xml` 删除两条 `<automatic>` 规则 | 22:00/07:00 EVALUATE 消失（DND 功能本身保留，用户可自建规则）|

**注意**：B 对已有 userdata 无效（规则存在 `/data/system/notification_policy.xml`），
出厂固件（userdata 已清）生效；存量设备需删该文件一次。

---

## 问题 1：未充电 + 5min 提前唤醒（已修复验证）

**根因**：health HAL 60s 周期轮询用 `CLOCK_BOOTTIME_ALARM` 的 timerfd，每次从休眠中叫醒设备。
**修复**：commit `6ea92292fb3` 改用 `CLOCK_BOOTTIME`（不再叫醒）。

### 验证（未充电，4 轮）

| 轮次 | 睡眠时长 | 唤醒源 | 中途早醒 |
|------|---------|--------|---------|
| 1 | 323s | IRQ 80 hym8563 | 无 |
| 2 | 295s | IRQ 80 hym8563 | 无 |
| 3 | 323s | IRQ 80 hym8563 | 无 |
| 4 | 292s | IRQ 80 hym8563 | 无 |

每轮 dmesg 只有一对 suspend/resume，`suspend_stats/success` 递增。

---

## 验证方法论（复现时用）

1. **休眠时长**：设备 `date` 在 suspend 前后各打一次（boot 时钟，不受 RTC 漂移影响）
2. **唤醒源**：`dmesg | grep "Resume caused"` —— 应为 `IRQ 80, hym8563`
3. **真休眠**：`cat /sys/power/suspend_stats/success` 递增
4. **中途早醒**：整轮 dmesg 只应有一对 `PM: suspend entry` / `Resume caused`
5. **alarm 是否触发**：`/data/data/com.myir.alarmtest/fired.txt`（需修复版 APK，见 alarmtest_client/README）
6. **内核编程值**：`dmesg | grep hym8563_rtc_set_alarm` —— `set alarm` 应为 `expired` +2% 向上取整

## 已知噪声（非 bug）

- **时钟跳变**：设备 RTC 时间落后真实时间多日，手动 `date` 拨动或 NTP 同步时会跳变；
  RTC_WAKEUP alarm 按 epoch 存储，时间回拨会让它们全部延后。测试期间避免拨时钟。
- **wakealarm sysfs 调试接口**：`echo +N > /sys/class/rtc/rtc0/wakealarm` 时而未进队列
  （疑似与之前一次 rollover 交互），Android 真实路径（alarmtimer）不受影响，排查时优先用 app 路径。

## 相关 commit

| commit | 内容 |
|--------|------|
| `737a8c44168` | RTC 补偿（本文件根因 1）|
| `6ea92292fb3` | health HAL 停止周期唤醒（问题 1）|
| `1a8fba71a21` | hym8563 过期 alarm 滚动到下一分钟（更早）|
| `550dd5a045c` | 关 AOD / DeviceIdle motion（更早）|
| `ff2ba3135a9` | 撤 alarmtimer 调试打印 |
