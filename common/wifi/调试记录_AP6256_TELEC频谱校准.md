# 高级工程师调试记录：AP6256 TELEC 频谱频率偏移校准

## 功能概述
AP6256 是 AMPAK 基于 Broadcom BCM43455 方案的 2.4G/5G WiFi+BT 模组，使用 37.4MHz 外部晶振。TELEC（日本无线电法）认证要求载波中心频率偏移不得超过 ±20 ppm。当晶振实际频率偏离标称值时，所有频段的载波频率会等比例偏移，导致认证失败。

## 问题现象

TELEC 频谱分析仪实测报告：

- **偏移方向**: 全频段一致向高频偏移
- **偏移量**: 实测中心频率偏高 **104 kHz ~ 112 kHz**
- **超标模式**:
  - n20: 偏移达 **20.27 ppm**（超出 TELEC ±20 ppm 限值）
  - ac20: 偏移达 **20.49 ~ 21.29 ppm**（超出 TELEC ±20 ppm 限值）
  - 11a/11n/ac 其他模式虽未超标，但同样存在 +18 ~ +20 ppm 偏移

## 根因分析

### 晶振频率偏差计算

```
实测载波偏移：+108 kHz（取 104~112 中值）
PPM 偏移：    ≈ +20.8 ppm（与实测报告的 20.27~21.29 一致）

晶振实际频率 = 37.4 MHz × (1 + 20.8 × 10⁻⁶)
              = 37.400778 MHz
              = 37400.778 kHz
```

### 机制说明

Broadcom WiFi 芯片通过 `xtalfreq` NVRAM 参数获取参考时钟频率，PLL 据此计算倍频系数来合成载波频率：

```
实际载波 / 预期载波 = 实际晶振频率 / xtalfreq 设定值
```

当 `xtalfreq=37400` 但晶振实际为 37400.778 kHz 时，载波会被等比例放大 +20.8 ppm。

## 解决方案

### 修改 NVRAM 频率校准参数

修改 `nvram_ap6256.txt` 中的 `xtalfreq` 参数，使其反映晶振实际频率：

```diff
-#XTAL 37.4MHz
-xtalfreq=37400
+#XTAL 37.4MHz (trimmed +1kHz to compensate +20.8ppm crystal offset for TELEC compliance)
+xtalfreq=37401
```

### 修正原理

| 参数 | 修正前 | 修正后 |
|------|--------|--------|
| xtalfreq 设定值 | 37400 kHz | 37401 kHz |
| 晶振实际频率 | 37400.778 kHz | 37400.778 kHz |
| 载波偏差 | (37400.778/37400 - 1) × 10⁶ = **+20.8 ppm** | (37400.778/37401 - 1) × 10⁶ = **-5.9 ppm** |

每改变 xtalfreq 1 个单位（1 kHz），对载波频率产生约 **26.7 ppm** 的修正量（1/37.4 × 10⁶）。

修正后所有模式残留在 -6 ~ -7 ppm 内，符合 TELEC ±20 ppm 规范。

## 预期效果

| 模式 | 修正前偏移 | 修正后预估 | 状态 |
|------|-----------|-----------|------|
| 11a | ~+19 ppm | ~-6 ppm | 合规 |
| n20 | **+20.27 ppm** | ~-6.5 ppm | 合规 |
| ac20 | **+20.49 ~ 21.29 ppm** | ~-5.8 ~ -6.2 ppm | 合规 |
| n40 | ~+18 ppm | ~-6 ppm | 合规 |
| ac40 | ~+18 ppm | ~-6 ppm | 合规 |
| ac80 | ~+18 ppm | ~-6 ppm | 合规 |

## 验证方法

```bash
# 查看当前使用的 NVRAM 及 xtalfreq 值
adb shell "cat /vendor/etc/wifi/nvram_ap6256.txt | grep xtalfreq"

# 确认内核加载的 xtalfreq（dmesg）
adb shell dmesg | grep -i "xtal\|xtalfreq"
```

需重新送测 TELEC 频谱分析仪，确认所有模式中心频率偏移在 ±20 ppm 以内。

## 补充说明

### xtalfreq 整数的精度限制

`xtalfreq` 在 Broadcom NVRAM 格式中的单位为 kHz（整数），无法设定小数值（如 37400.778）。最小步进 1 kHz 对应载波修正 26.7 ppm。若后续实测发现残留偏移仍偏大（>±15 ppm），可考虑以下硬件层面优化：

1. **调整晶振负载电容（C₁/C₂）**: 减小负载电容会使晶振频率略微升高，反之降低。每改变 ~1pF 约影响 5-10 ppm
2. **更换高精度晶振**: 选用 ±10 ppm 以内的 TCXO 替代普通晶振

### 相关文件

- NVRAM 配置: `vendor/rockchip/common/wifi/firmware/nvram_ap6256.txt`
- 同系列其他模组参考: `nvram_ap6255.txt`, `nvram_ap6356.txt` 等

### 适用范围

本方法适用于所有使用 `xtalfreq` 参数的 Broadcom 方案 WiFi 模组（AP6255/AP6256/AP6356/AP6275 等），只需将晶振实际频率换算为对应的 `xtalfreq` 值即可。
