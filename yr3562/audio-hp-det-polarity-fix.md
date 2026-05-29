# 音频耳机检测极性修复

## 问题现象

耳机插入/拔出时，音频输出通道切换行为与预期相反：
- **插入耳机** → 声音从喇叭输出，耳机无声
- **拔出耳机** → 声音从耳机输出，喇叭无声

通过两次 amixer -c 0 contents 详细打印，放入对比工具vscode对比，确认耳机插拔检测状态值与实际路由状态相反。
###没插耳机
numid=37,iface=MIXER,name='Playback Path'
	  ; type=ENUMERATED,access=rw------,values=1,items=11
  ; Item #0 'OFF'
  ; Item #1 'RCV'
  ; Item #2 'SPK'
  ; Item #3 'HP'
  ; Item #4 'HP_NO_MIC'
  ; Item #5 'BT'
  ; Item #6 'SPK_HP'
  ; Item #7 'RING_SPK'
  ; Item #8 'RING_HP'
  ; Item #9 'RING_HP_NO_MIC'
  ; Item #10 'RING_SPK_HP'
  : values=3
###插入耳机
numid=37,iface=MIXER,name='Playback Path'
  ; type=ENUMERATED,access=rw------,values=1,items=11
  ; Item #0 'OFF'
  ; Item #1 'RCV'
  ; Item #2 'SPK'
  ; Item #3 'HP'
  ; Item #4 'HP_NO_MIC'
  ; Item #5 'BT'
  ; Item #6 'SPK_HP'
  ; Item #7 'RING_SPK'
  ; Item #8 'RING_HP'
  ; Item #9 'RING_HP_NO_MIC'
  ; Item #10 'RING_SPK_HP'
  : values=2

## 根因分析

### 硬件

耳机座检测引脚连接到 `GPIO3_PB2`，通过该引脚电平高低判断耳机是否插入。

关键：本项目硬件在**耳机插入时引脚为高电平**。

### 驱动逻辑

`sound/soc/rockchip/rockchip_multicodecs.c`:

```c
// adc_jack_handler() - L229
if (!gpiod_get_value(mc_data->hp_det_gpio)) {
    snd_soc_jack_report(jack_headset, 0, SND_JACK_HEADSET);  // 耳机拔出
    ...
    return;
}
// 耳机插入，上报 jack 事件
```

驱动通过 `gpiod_get_value()` 返回值判断：
- 返回 **1**（active）→ 耳机插入 → 切耳机、关喇叭
- 返回 **0**（inactive）→ 耳机拔出 → 切喇叭、关耳机

### 原设备树配置（错误）

```dts
hp-det-gpio = <&gpio3 RK_PB2 GPIO_ACTIVE_LOW>;
```

`GPIO_ACTIVE_LOW` 告诉 gpiolib 对物理电平取反：

| 物理状态 | 引脚电平 | gpiod_get_value() | 驱动判定 | 实际行为 |
|----------|----------|-------------------|----------|----------|
| 耳机插入 | 高       | 0 (取反)          | 拔出     | 喇叭响   |
| 耳机拔出 | 低       | 1 (取反)          | 插入     | 耳机响   |

这就是"耳机和喇叭互相反"的根本原因。

### 中断触发

驱动注册的是双边沿触发中断：

```c
// L511-514
IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT
```

插入和拔出都有边沿产生，中断本身没问题，问题在中断处理函数中读取的电平值与实际状态不一致。

## 修复

```diff
- hp-det-gpio = <&gpio3 RK_PB2 GPIO_ACTIVE_LOW>;
+ hp-det-gpio = <&gpio3 RK_PB2 GPIO_ACTIVE_HIGH>;
```

改为 `GPIO_ACTIVE_HIGH` 后，gpiolib 不再取反：

| 物理状态 | 引脚电平 | gpiod_get_value() | 驱动判定 | 实际行为 |
|----------|----------|-------------------|----------|----------|
| 耳机插入 | 高       | 1                 | 插入     | 耳机响   |
| 耳机拔出 | 低       | 0                 | 拔出     | 喇叭响   |

## 验证方法

```bash
# 1. amixer 查看 DAPM 通路状态
amixer -c 0 contents | grep -A 18 "Playback Path"

# 2. 插入耳机 → Path = HP/耳机有声音输出
gst-play-1.0 /usr/share/myir/Music/myir_audio.mp3
#    拔出耳机 → Path = SPK/喇叭有声音输出
gst-play-1.0 /usr/share/myir/Music/myir_audio.mp3

``


























`

## 受影响的文件

| 文件 | 说明 |
|------|------|
| `kernel-6.1/arch/arm64/boot/dts/rockchip/myd-yr3562.dtsi:78` | 修改 hp-det-gpio 极性 |
| `kernel-6.1/sound/soc/rockchip/rockchip_multicodecs.c` | 机器驱动，jack 检测逻辑 |
| `kernel-6.1/sound/soc/codecs/rk817_codec.c` | RK809 codec 驱动（兼容 rk817） |

## 相关配置点

此板未使用以下可选 GPIO（均由 RK809 内部寄存器控制）：

```dts
// 未配置，DAPM 事件不需要外部 GPIO 控制
// spk-con = <&gpioX RK_PXX GPIO_ACTIVE_HIGH>;
// hp-con  = <&gpioX RK_PXX GPIO_ACTIVE_HIGH>;
```

## 调试技巧

如需确认 GPIO 物理电平（绕开 gpiod 取反）：

```bash
# 通过 sysfs 直接读引脚物理电平（需要 debugfs 挂载）
mount -t debugfs none /sys/kernel/debug
cat /sys/kernel/debug/gpio | grep "PB2"

# 或用寄存器读取工具确认引脚输入电平
io -4 0x<GPIO3_BASE>  # RK3562 GPIO3 数据寄存器
```

---

**修复日期**: 2026-04-29
**平台**: Rockchip RK3562 (MYD-YR3562)
**内核版本**: Linux 6.1
