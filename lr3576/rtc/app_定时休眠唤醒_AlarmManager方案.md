# App 定时休眠唤醒方案（AlarmManager 标准路径）— LR3576/RK3576

日期：2026-09-09（2026-09-17 更新：整点唤醒消除、RTC 补偿、静止节流固化）
状态：已验证（49 轮 5min 循环、1h 跨整点乘 4、15min 乘 3、开机场景乘 2，全部通过）

2026-09-16 起唤醒精度语义修订：不早醒，最多晚 1 分钟左右（此前是"分钟截断可提前
≤59s"）。客户 app 无需改动，验收标准按新语义执行。

## 产品需求

1. 无操作一段时间 → system app 下发休眠（echo mem）
2. 休眠前 app 设 RTC alarm → 定时唤醒

## 方案结构

```
产品 app:
  ① 设定时唤醒: AlarmManager RTC_WAKEUP（替代 /sys/class/rtc/rtc0/wakealarm 直写）
  ② 下发休眠: echo mem > /sys/power/state
```

## ① 定时唤醒（Java API）

```java
AlarmManager am = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
long triggerAt = System.currentTimeMillis() + sleepDurationMs;
Intent intent = new Intent(context, WakeupReceiver.class);
PendingIntent pi = PendingIntent.getBroadcast(context, 0, intent,
        PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, triggerAt, pi);
```

```java
public class WakeupReceiver extends BroadcastReceiver {
    public void onReceive(Context c, Intent i) {
        // 唤醒后业务（必要时调 PowerManager.wakeUp 亮屏）
    }
}
```

## ② 下发休眠

```sh
echo mem > /sys/power/state
```

## 关键注意事项

| 注意点 | 说明 |
|--------|------|
| **唤醒精度：绝不早醒，最多晚 ~1 分钟** | 固件含 RTC 休眠走快补偿（+2% 向上取整到分钟）。1h alarm 实测 +10~+15s；5min 实测 +7~+53s。客户验收按"不早于设定时间"判定 |
| 休眠时长 ≥ 2 分钟 | HYM8563 分钟精度 + 补偿取整 |
| mem 可能失败重试 | 系统有 <2s wakeup alarm 时 EBUSY 拒——sleep 3s 重试 ≤3 次 |
| **不要用 wakealarm 直写** | 与 alarmtimer 竞争 → aie_timer 误清 + AIE 关 → 睡死（曾 19h+ 不醒）|
| **禁用 CalendarProvider2** | 每 ~60s 设 ELAPSED_WAKEUP alarm → 打断 mem 休眠。产品无日历需求：`pm disable-user --user 0 com.android.providers.calendar`（需工厂固化）|
| **app 内不要设短周期 wakeup alarm** | 任何几分钟一次的 RTC/ELAPSED wakeup alarm 都会让设备每隔几分钟醒一次 —— 若观察到"几分钟就醒"，先查自家 app 的 alarm（`dumpsys alarm` 按包名过滤）|
| 充满电时 charger 可能周期性中断 | 电量 100% 时充电管理会在充满/涓流间切换，每次切换可能产生一次瞬时唤醒（几秒级，非 app alarm）|
| acoustic_pocket 可运行 | CRY 运行中验证通过——无需 kill，但注意其自身 alarm 行为 |

## BSP 修复清单（已提交）

### 1. `9151df728da` — AOD 关闭
- `config_dozeAlwaysOnEnabled=false`（rk3576 overlay）
- 修复：息屏后 doze dream 保持屏幕 STATE_ON → suspend 失败亮屏

### 2. `f5d849d2f55` — 驱动顺延 + DeviceIdle motion
- **rtc-hym8563.c**：alarm 秒清零后若过期 → 顺延下一分钟
  - 修复：过期 alarm 立即触发 → aie_timer 误清 → AIE 关 → **睡死**；以及 suspend 1.25s 早醒
- **config_autoPowerModeUseMotionSensor=false**（rk3576 overlay）
  - 修复：无 SMD sensor → DeviceIdle 永判"非静止" → ~2min wakeup alarm 无限重设 → **打断 app mem 休眠**
- 配套：`settings put global location_enable_stationary_throttle 0`
  - StationaryThrottlingLocationProvider 注册 stationary listener（触发 motion 监控）
  - **产品固化需**: SettingsProvider overlay 默认 0 或开机设置

### 3. `47c8f8f3056` — health HAL 不打断 app 休眠 + 调试增强
- **HealthLoop.cpp**：periodic-chores timerfd 从 `CLOCK_BOOTTIME_ALARM` → `CLOCK_BOOTTIME`
  - 根因：充电时 health HAL 每 60s 重设 wakeup alarm（防过热检测）→ suspend 时被编程进 RTC → **每次 mem 后 ~60s 被它唤醒**
  - 修复后：醒着时 60s 检测照常；suspend 冻结不唤醒；resume 补发一次
- **alarmtimer.c**：suspend debug 打印标注 min-alarm 来源（freezer/queue）+ 队列头函数符号
- **rk3576.config**：CONFIG_PM_AUTOSLEEP（可选路径，与 mem 无关）

### 4. CalendarProvider2 禁用（运行时，需产品固化）
- 根因：calendar provider 每 ~60s 设精确 ELAPSED_WAKEUP alarm（同步检查）→ 打断 mem
- 处理：`pm disable-user --user 0 com.android.providers.calendar`（产品无日历需求）
- 固化方式：出厂配置禁用或 PRODUCT_PACKAGES 移除

### 5. `737a8c44168` — RTC 休眠走快补偿（2026-09）
- 根因：HYM8563 芯片在 mem 休眠期间时钟走快 ~1.9%（实测 1.6-1.93%，清醒态 0 漂移）→ 所有 suspend 编程的 alarm 按比例提前触发（1h ≈ 提前 70s）
- 修复：驱动把 alarm 间隔 ×1.02 + 向上取整到整分 → **绝不早醒，最多晚 ~1 分钟**
- 硬件线索：疑似休眠态振荡器负载/供电变化，硬件组待排查

### 6. `0a5bb71c733` — 整点系统唤醒消除（2026-09）
- 根因①：Settings 电池统计 job 每小时 :00 排 `setExactAndAllowWhileIdle(RTC_WAKEUP)` 且自续期 → 每小时必醒；1h 用户 alarm 必然跨整点被抢占（提前量 = 设置时刻离整点的距离）
- 根因②：DND 默认"睡眠"日程规则（22:00-07:00）即使未启用也会订阅条件 → 每天 22:00/07:00 各一次精确唤醒
- 修复：Settings PeriodicJobManager 禁用（`MYIR_ENABLE_PERIODIC_JOB=false`）+ default_zen_mode_config.xml 移除两条默认规则
- 注意：存量 userdata 需清一次 `/data/system/notification_policy.xml`（整包刷机会自动清）

## 提前唤醒根因总表（mem 后 <alarm 时间就醒）

| 根因 | 机制 | 修复 |
|------|------|------|
| DeviceIdle motion alarm | 无 SMD sensor 永判非静止 → ~2min wakeup 无限重设 | config_autoPowerModeUseMotionSensor=false + stationary_throttle=0（f5d849d2f55）|
| health HAL 充电检测 | CLOCK_BOOTTIME_ALARM 60s 周期 | 改 CLOCK_BOOTTIME（47c8f8f3056）|
| CalendarProvider2 | 每 ~60s ELAPSED_WAKEUP | 禁用（产品固化）|
| RTC 驱动秒清零 | 过期 alarm 立即触发（早醒 1s 级/睡死）| 顺延下一分钟（f5d849d2f55）|
- `rk3576.config` CONFIG_PM_AUTOSLEEP（已加，未提交——mem 路径不依赖，可选）

## 验证结果（2026-09-10 最终）

**AlarmManager RTC_WAKEUP + echo mem 路径**：全部通过（health + calendar 修复后）
- **5 轮 × alarm 120s**（acoustic_pocket 运行中，不 kill）：
  - 睡眠时长 70/110/91/91/131s，派发计数逐轮递增（0→1→2→3→4→5→…）✓
- **单轮 × alarm 300s（5 分钟）**：睡 293s 精确唤醒 ✓
  - 内核证据：`suspend soonest min=297645ms src=queue`（编程的即 app alarm，无抢占）
  - `Resume caused by IRQ 83, hym8563`（RTC alarm 唤醒）
- 无睡死、无 <60s 提前唤醒、派发全部正常
- 睡眠时长波动 = HYM8563 分钟精度截断（正常，误差 ≤59s）

**定位手段**（后续复现用）：
- `dmesg | grep alarmtimer` → `min=xxxms src=freezer/queue` 直接看出谁的 alarm 被编程
- 全系统 ALARM timerfd 扫描（找 dumpsys 之外的"隐形唤醒源"）：
  `for p in /proc/[0-9]*; do ... grep clockid ... 8|9 = ALARM 时钟 ...`

## 测试工具

`/tmp/alarmtest/AlarmTest.apk`（platform 签名）：
- MainActivity：启动设 alarm（`--es delay N` 秒）
- AlarmSetReceiver（广播 `com.myir.alarmtest.SET --es delay N`）：不重启 activity 设 alarm
- AlarmReceiver：验证派发（写 `/data/data/com.myir.alarmtest/fired.txt`）

## 遗留

- 产品固化项（已全部完成）：
  1. 日历 provider —— 已从构建移除（rk3576_u.mk filter-out CalendarProvider，不再需要 pm disable-user）
  2. `location_enable_stationary_throttle=0` —— 已固化（commit `272007d2033`，SettingsProvider 默认值加 DatabaseHelper 写入；开机约 10 分钟的 Doze 静止检测唤醒随之消除）
  3. health HAL 新二进制 —— 已在整包中
- CRY（acoustic_pocket）运行中验证通过，无需特殊处理
