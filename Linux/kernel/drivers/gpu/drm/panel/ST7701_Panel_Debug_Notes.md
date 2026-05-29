# ST7701 MIPI DSI 显示屏调试笔记 (RK3576 + Android 14)

## 概述

ST7701 是 Sitronix 出品的 MIPI DSI 显示驱动芯片，项目使用 800x480 分辨率面板，通过 DSI 接口与 RK3576 连接。调试涵盖：初始移植、功耗优化、噪声电流、ESD 保护。

---

## 一、驱动移植

### 1.1 DTS 配置 (`40a261b65c6`)

面板定义在 `myz-rk3576-mipi-800-480.dtsi`：

```c
&dsi0 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;
    
    dsi_panel: panel@0 {
        compatible = "simple-panel-dsi";
        reg = <0>;
        backlight = <&backlight>;
        power-supply = <&vcc3v3_lcd_n>;
        reset-gpios = <&gpio1 RK_PB2 GPIO_ACTIVE_LOW>;
        
        enable-sequence = <
            /* power on */
            /* reset 时序 */
        >;
        
        panel-init-sequence = [
            /* 初始化命令序列 */
        ];
        
        display-timings {
            native-mode = <&timing0>;
            timing0: timing0 {
                clock-frequency = <33000000>;
                hactive = <800>;
                vactive = <480>;
                hback-porch = <46>;
                hfront-porch = <46>;
                hsync-len = <20>;
                vback-porch = <23>;
                vfront-porch = <22>;
                vsync-len = <10>;
            };
        };
    };
};
```

### 1.2 初始化序列

ST7701 使用厂商自定义命令 `FFh` 作为 Page Select，切换寄存器页：

```
Page 0: 基础设置 (Gamma, Power, Display)
Page 1: 高级设置 (Timing, Driver)
Page 2: Security
Page 3: Test
```

初始化序列示例：
```
29 00 06 FF 77 01 00 00 10  // 选择 Page 1
29 00 03 C0 63 00            // 设置 Panel 控制
29 00 02 CC 18               // 偏置电流控制
...
29 00 06 FF 77 01 00 00 00  // 返回 Page 0
```

---

## 二、面板初始化优化 (`6f196f0b36e`)

### 2.1 功耗优化

提交 `6f196f0b36e` (update display panel init code & cut down power consumption)：

**修改内容：**
1. **PWM 频率调整**：2000 → 10000 (500kHz → 100kHz)，降低背光 PWM 开关损耗
2. **Gamma 校正优化**：B0/B1 寄存器值调整，优化显示效果同时降低功耗
3. **电源管理优化**：调整 VGH/VGL 电荷泵频率和驱动能力

```diff
- pwms = <&pwm1_6ch_0 0 2000 0>;    // 500kHz
+ pwms = <&pwm1_6ch_0 0 10000 0>;   // 100kHz

- 29 00 02 CC 10
+ 29 00 02 CC 18     // 降低偏置电流
```

### 2.2 VCOM 调整

通过 DTS init sequence 设置 VCOM 电压以优化闪烁和功耗平衡：

```
29 00 02 B0 7D      // VCOM 设置（VcomH）
29 00 02 B1 39      // VCOM 设置（VcomL）
```

VCOM 电压直接影响液晶翻转速度和功耗，需在 display flicker 和功耗之间找平衡。

---

## 三、显示噪声电流修复 (`12bb4528582`)

### 3.1 问题描述

提交 `12bb4528582` (diaplsy noise current)：面板在某些画面下出现条纹噪声，伴随电源电流异常波动。

### 3.2 根因分析

噪声电流由面板源极驱动（Source Driver）的充电时序不匹配引起，特定画面 pattern 下数据线充放电电流过大。

### 3.3 修复方法

调整 DTS 中 Source Driver 相关设置：
```
29 00 02 B5 47      // Source 驱动能力调整
29 00 02 B7 8A      // 预充电时间调整
29 00 02 C1 09      // 减少交叉干扰
```

**排查方法：**
```bash
# 用电流钳测量面板供电回路 (vcc3v3_lcd_n)
# 观察不同画面下的电流波形
# 噪声突出时，用示波器看 Source Driver 输出
```

---

## 四、ESD 自动恢复

> 详见 [ESD 恢复笔记](../ESD/rk3576_android14_ESD_check/ESD_Recovery_Debug.md)

ESD 恢复机制通过在 `panel-simple.c` 中添加检测和恢复 workqueue 实现：

1. 周期性读取面板寄存器 `0x0A` (DCS_GET_POWER_MODE)
2. 正常值应为 `0x9C`
3. 连续 3 次异常触发 reset_work
4. reset_work 强制 DPMS OFF → wait → DPMS ON

---

## 五、Boot 动画与 Logo

### 5.1 自定义 Logo (`db7b5d541a6`)

```makefile
# BoardConfig.mk
BOOT_LOGO := myir_custom
```

Logo 图片位置：
```
vendor/rockchip/common/logo/
    └── myir_custom.bmp
```

### 5.2 Boot Video/Animation (`fc0dd04003e`)

从 bootanimation.zip 切换到 bootanimation.ts 格式：

```
device/rockchip/common/bootanimation.ts
```

Boot Video 使能：
```makefile
# BoardConfig.mk
PRODUCT_BOOT_ANIMATION := bootvideo
```

---

## 六、调试工具集

```bash
# 查看 panel 状态
cat /sys/kernel/debug/dri/0/summary

# 强制 DPMS off/on
echo off > /sys/class/drm/card0-<connector>/dpms
echo on > /sys/class/drm/card0-<connector>/dpms

# 调整背光
echo 100 > /sys/class/backlight/backlight/brightness

# 抓取 framebuffer
cat /dev/fb0 > /tmp/fb.raw
```

---

## 七、经验总结

1. **PWM 频率选择**：越高 flicker 越少但开关损耗越大，100kHz 在品质和功耗间平衡较好
2. **初始化序列**：ST7701 的 Page 切换机制容易出错，每个命令前需要确认当前 Page
3. **噪声电流**：通常在特定灰阶或颜色 pattern 下暴露，测试需覆盖全灰阶
4. **功耗调优**：面板功耗主要来自 Source Driver 充电、VCOM 驱动和背光，逐项优化
