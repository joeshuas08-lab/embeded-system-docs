# 内核崩溃日志分析指南

## 日志结构

Kernel panic 输出按以下顺序排列，**1-3 是关键，4-5 几乎不看**：

```
1. 内核正常日志末尾     ← 崩溃前最后一条正常消息
2. 崩溃描述行           ← 错误类型 + 地址
3. Call trace 调用栈    ← 最重要，从下往上读
4. 寄存器 dump          ← 很少需要
5. 汇编 Code dump       ← 几乎不用看
```

## 三步分析法

### 第一步：看崩溃点和上下文

```bash
# 搜索崩溃标记
grep "Unable to handle\|BUG:\|Oops:\|Kernel panic\|WARNING:" boot.log
```

找到错误类型：

| 错误信息 | 含义 |
|----------|------|
| `Unable to handle kernel NULL pointer dereference at 0xNNNN` | NULL 指针解引用，偏移量 NNNN |
| `Unable to handle kernel paging request at 0xNNNN` | 访问了非法地址（use-after-free/野指针） |
| `BUG: soft lockup` | 调度器卡死，某 CPU 长时间未调度 |
| `BUG: unable to handle kernel paging request` | 缺页异常 |
| `WARNING: at ...` | 断言失败，内核还能继续跑 |

**记住**：地址 `0xNNNN` 如果很小（< 0x1000），是 **NULL 指针 + 结构体成员偏移**。如果很大且看起来像随机值，是 **野指针或 use-after-free**。

然后看**崩溃前最后几条正常日志**：

```bash
grep -B5 "Unable to handle" boot.log
```

这些日志告诉你崩溃前内核在做什么操作。比如：

```
[    2.396174] ttyS3 at MMIO 0xff690000   ← 正在初始化串口
[    2.397668] ttyS9 at MMIO 0xff6f0000   ← 串口探测的最后一条
[    2.397795] Unable to handle kernel NULL pointer  ← 紧接着崩溃
```

→ 串口探测完成后出的事，和串口驱动有关。

### 第二步：看调用栈（最关键）

```
Call trace:
  __cmpxchg_case_acq_32+0x20/0x38       ← 实际崩溃的汇编函数，忽略
  _raw_spin_lock_irqsave+0x28/0x38      ← 拿锁
  tty_port_tty_get+0x20/0x54            ← tty 层，port 是 NULL
  serial8250_update_uartclk+0x44/0x164  ← 8250 串口时钟更新
  dw8250_clk_work_cb+0x30/0x3c          ← ★ 这里！DW8250 时钟回调
  process_one_work+0x16c/0x208          ← 工作队列框架
  worker_thread+0x198/0x284
```

**从下往上读**：`dw8250_clk_work_cb` 是 work 回调 → 调 8250 更新时钟 → 调 tty 层获取 port → 拿锁时因为 port 是 NULL 崩溃。

`+0xNN/0xNN` 格式：`函数偏移/函数总大小`，不用深究，看函数名即可。

### 第三步：从寄存器确认（可选）

```
x0 : 00000000000000a8    ← 实际访问的地址
x19: 0000000000000000    ← 0 = 空指针，就是它传进去的
```

`x0 + x19 = 0x0` 说明某个指针参数是 NULL。结合代码逻辑：`tty_port_tty_get(struct tty_port *port)` 的 port = NULL。

## 实战流程总结

```
$ grep "Unable to handle\|Call trace\|Workqueue\|Comm:" boot.log

1. 看 Unable to handle → 确定崩溃类型
2. 看崩溃前 -B5 行正常日志 → 知道当时在做什么
3. 看 Call trace 最下面 → 找到触发源（通常是驱动回调/work/中断）
4. 看 Comm: / Workqueue: → 确定是哪个进程/队列的上下文
```

## 常见崩溃模式速查

| 模式 | 日志特征 | 典型原因 |
|------|---------|---------|
| NULL 指针 | `NULL pointer dereference at 0xNN` + NN < 0x1000 | probe 顺序问题，结构体未初始化 |
| use-after-free | `paging request at 0xNN` + NN 是随机大值 | 释放后还在用，常见于热插拔 |
| 死锁 | `soft lockup` + 长时间无日志 | 中断里拿锁、spinlock 死循环 |
| 内存耗尽 | `Out of memory` + `allocation failed` | OOM killer 之前的内存分配失败 |
| 驱动 probe 失败 | `deferred probe pending` + 无 panic | 驱动依赖的资源还没 ready |

## 调试技巧

```bash
# 1. 只看关键信息
dmesg | grep -E "Oops|panic|BUG|Call trace|Unable|Workqueue|Comm:"

# 2. 找崩溃前最后几行正常日志
dmesg | grep -B10 "Unable to handle"

# 3. 查看某个驱动相关的所有日志
dmesg | grep -i "8250\|serial\|uart\|tty"

# 4. 如果在启动阶段崩溃，用 earlyprintk
# cmdline 加: earlycon console=ttyFIQ0,115200 loglevel=7 ignore_loglevel
```
