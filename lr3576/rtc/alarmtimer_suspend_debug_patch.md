# alarmtimer suspend 调试补丁（定位"提前唤醒"来源）

**状态**：已从内核源码撤除，本文保留补丁备查。
**撤除原因**：属定位用的临时打印，不应进交付固件（客户 dmesg 会被刷屏）。
**原文位置**：`kernel-6.1/kernel/time/alarmtimer.c` → `alarmtimer_suspend()`

---

## 补丁内容

在 `alarmtimer_suspend()` 里加三处打印，用于定位 **suspend 时被编程的那个 alarm 是谁的**。

### 1) 函数局部变量

```c
	ktime_t min, now, expires;
	int i, ret, type;
	int min_from_freezer;                    /* + ADD */
	struct rtc_device *rtc;
	unsigned long flags;
	struct rtc_time tm;
```

### 2) 记录来源（freezer vs 正常队列）

```c
	min = freezer_delta;
	expires = freezer_expires;
	type = freezer_alarmtype;
	min_from_freezer = (min != 0);           /* + ADD */
	freezer_delta = 0;
```

在挑选最早 alarm 的循环里，命中正常队列时清掉标记：

```c
			expires = next->expires;
			min = delta;
			type = i;
			min_from_freezer = 0;        /* + ADD */
		}
```

### 3) 打印最早 alarm（含回调符号名）

`if (min == 0) return 0;` 之后插入：

```c
	/* debug: identify the soonest alarm that drives the wakeup */
	pr_info("alarmtimer: suspend soonest min=%lldms type=%d src=%s\n",
		ktime_to_ms(min), type, min_from_freezer ? "freezer" : "queue");
	if (!min_from_freezer) {
		struct alarm_base *base = &alarm_bases[type];
		struct timerqueue_node *node;
		struct alarm *a = NULL;

		spin_lock_irqsave(&base->lock, flags);
		node = timerqueue_getnext(&base->timerqueue);
		spin_unlock_irqrestore(&base->lock, flags);
		if (node) {
			/* struct alarm embeds node as first member */
			a = container_of(node, struct alarm, node);
			pr_info("alarmtimer:   queue head fn=%pS\n", a->function);
		}
	}
```

### 4) 打印 EBUSY 拦截

```c
	if (ktime_to_ns(min) < 2 * NSEC_PER_SEC) {
		/* debug: identify the alarm blocking suspend */
		pr_info("alarmtimer: suspend EBUSY min=%lldns type=%d expires=%lld\n",
			ktime_to_ns(min), type, ktime_to_ns(expires));
		pm_wakeup_event(dev, 2 * MSEC_PER_SEC);
		return -EBUSY;
	}
```

---

## 输出解读

```
alarmtimer: suspend soonest min=54000ms type=1 src=queue
alarmtimer:   queue head fn=alarm_timer_arm   ← 回调符号名，直接指向设置者
```

| 字段 | 含义 |
|------|------|
| `min` | 距离唤醒的毫秒数 —— 与 app 设定的时长对比 |
| `type` | 0=REALTIME 1=BOOTTIME 2=REALTIME_WAKEUP 3=BOOTTIME_WAKEUP |
| `src` | `freezer` = 来自 freezer 路径；`queue` = 正常 alarm 队列 |
| `fn` | **最有用** —— 回调函数符号名，指向设置 alarm 的组件 |

`fn` 是定位的关键：`alarm_timer_arm` 之类内核内部回调 vs 其他驱动的回调，一眼能区分。

---

## 复现步骤

```bash
# 1. 应用补丁后编译（必须走官方流程，否则缺 ramdisk 会 panic）
./build.sh -K

# 2. 刷机后设置 alarm 并休眠
adb shell am broadcast -n com.myir.alarmtest/.AlarmSetReceiver \
  -a com.myir.alarmtest.SET --es delay 300
adb shell "echo mem > /sys/power/state"

# 3. 抓打印
adb shell "dmesg | grep -E 'alarmtimer: (suspend|  queue)'"
```

---

## 历史用途

- 用于定位"设 120s alarm 但 10-60s 就被唤醒"的问题
- 确认 `src=queue` + 具体 `fn` → 指向非 AlarmManager 的隐藏 alarm
- 最终定位到 **health HAL 的 60s 周期 chore**（`CLOCK_BOOTTIME_ALARM` 的 timerfd）
  修复见 commit `6ea92292fb3`（改用 `CLOCK_BOOTTIME`）

---

## ⚠️ 相关提醒

**客户手册依赖这个打印！（已处理：2026-09）**

`客户测试手册_极简版.md` 和 `客户验收测试手册_定时休眠唤醒.md` 的排查章节
原本写着 `dmesg | grep "alarmtimer: suspend soonest"`。补丁撤除后该手段失效，
两份手册**已同步更新**为让客户抓 `su 0 dmesg | grep "Resume caused"` +
`fired.txt`（判断 alarm 是否真触发）。

本补丁保留备查：将来若再出现"提前唤醒"类问题，应用本补丁 + 重编内核，
可让 suspend 时被编程的 alarm 直接打出符号名（`fn=%pS`）——这是最快的定位手段。
