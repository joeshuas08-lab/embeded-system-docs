# 高级工程师调试记录：PDM 麦克风与电源管理杂项调试

## 功能概述
PDM（Pulse Density Modulation）麦克风驱动调试涉及音频输入路径的适配和内核驱动优化，BCM 稳压器调试涉及电源管理 IC 的配置修正。在 RK3576 Android 14 平台中，PDM 麦克风作为数字音频输入设备，需要通过 PDM 接口驱动与 SoC 通信；BCM 稳压器则为系统提供稳定的电压输出。本记录总结这些较小但关键的调试经验。

## 调试方法

### 1. PDM 麦克风调试
```bash
# 查看 PCM/PDM 设备
arecord -l

# 录制测试音频
arecord -D hw:0,0 -f S16_LE -r 48000 -c 2 -d 5 test.wav

# 查看 PDM 驱动日志
dmesg | grep -E "(pdm|rockchip.*pdm)"

# 查看声卡路由
tinymix
```

### 2. 稳压器调试
```bash
# 查看系统中所有 regulator
cat /sys/kernel/debug/regulator/regulator_summary

# 查看特定 regulator（BCM）
cat /sys/kernel/debug/regulator/regulator_summary | grep -i bcm

# 查看 regulator 电压
cat /sys/kernel/debug/regulator/vcc_bcm/voltage
```

### 3. dvrs_hw 设备权限调试
```bash
# 查看设备节点权限
ls -l /dev/dvrs_hw

# 查看 SELinux 上下文
ls -Z /dev/dvrs_hw

# 查看应用权限
adb shell dumpsys package <package_name> | grep -i permission
```

## 常见问题及解决方案

### 1. PDM 麦克风驱动适配（commit `647c82bb22ae`）
**问题：** PDM 麦克风在 Android 系统中无法录音或录音异常。

**驱动修改：**
```c
// kernel-6.1/sound/soc/rockchip/rockchip_pdm_v2.c
// 为 Android 适配 PDM 接口驱动，主要修改：

// 1. 简化驱动代码（删除 334 行冗余代码）
// 2. 修正 DAI 链路配置
// 3. 优化 DMA 缓冲区管理

static const struct snd_soc_dai_ops rockchip_pdm_dai_ops = {
    .startup = rockchip_pdm_startup,
    .shutdown = rockchip_pdm_shutdown,
    .hw_params = rockchip_pdm_hw_params,
    .prepare = rockchip_pdm_prepare,
    .trigger = rockchip_pdm_trigger,
};
```

**设备树配置：**
```dts
// PDM 麦克风节点
&pdm {
    status = "okay";
    rockchip,path-map = <0 1 2 3>;  // 通道映射
    rockchip,sample-rate = <48000>;  // 采样率
    #sound-dai-cells = <0>;
};
```

**验证方法：**
```bash
# 使用 tinyalsa 录制测试
tinycap /data/test.wav -D 0 -d 0 -r 48000 -b 16 -c 2 -n 5

# 播放录制文件确认录音质量
tinyplay /data/test.wav

# 查看 PDM 驱动工作状态
cat /proc/asound/card0/pcm0c/sub0/status
```

### 2. BCM 稳压器配置修正（commit `f38e577a21d4`）
**问题：** BCM 稳压器输出电压不匹配，导致相关外设工作异常。

**解决方案：**
在设备树中修正 BCM 稳压器的电压配置：

```dts
// myd-lr3576.dtsi
&bcm_regulator {
    // 修正 regulator 电压配置
    regulator-name = "vcc_bcm";
    regulator-min-microvolt = <3300000>;  // 3.3V
    regulator-max-microvolt = <3300000>;
    regulator-always-on;
    regulator-boot-on;
    
    // 修正供电引脚配置
    gpio = <&gpio3 RK_PC0 GPIO_ACTIVE_HIGH>;
    enable-active-high;
};
```

**验证方法：**
```bash
# 确认 regulator 输出正常
cat /sys/kernel/debug/regulator/vcc_bcm/voltage
# 应为 3300000 uV (3.3V)

# 检查相关外设工作状态
dmesg | grep -i bcm
```

### 3. dvrs_hw 设备权限修复（commit `52b9b38430b7`）
**问题：** `/dev/dvrs_hw` 设备节点权限不足，应用层无法访问。

**解决方案：**
1. **文件上下文配置**（`device/rockchip/common/sepolicy/vendor/file.te`）：
   ```te
   # 添加 dvrs_hw 设备节点类型
   type dvrs_hw_device, dev_type;
   ```

2. **文件上下文映射**（`file_contexts`）：
   ```
   /dev/dvrs_hw u:object_r:dvrs_hw_device:s0
   ```

3. **初始化 RC 配置**（`init.connectivity.rc`）：
   ```rc
   # 设置设备节点权限
   chmod 0666 /dev/dvrs_hw
   chown system system /dev/dvrs_hw
   ```

4. **去除全局权限**：在 `system/sepolicy/public/app.te` 中移除不必要全局权限。

**验证方法：**
```bash
# 检查设备权限
ls -l /dev/dvrs_hw
# 应显示 crw-rw-rw- root system

# 验证应用可访问
adb shell su -c "echo test > /dev/dvrs_hw"
```

## 调试案例

### 案例一：PDM 麦克风 Android 适配（commit `647c82bb22ae`）
**背景：** PDM 麦克风驱动在 Android 系统中无法正常工作，需要从 Linux 驱动适配到 Android 音频框架。

**适配内容：**
1. 大幅重构 `rockchip_pdm_v2.c`（删除 334 行冗余代码，保留 53 行核心逻辑）
2. 更新 `rockchip_pdm_v2.h` 头文件
3. 配置正确的 DAI 链路和 DMA 参数

**验证：**
- 使用 tinycap/tinyplay 测试录音和播放
- 使用 tinymix 查看音频路由
- 测试不同采样率（16kHz/48kHz）下的录音质量

### 案例二：BCM 稳压器配置修正（commit `f38e577a21d4`）
**背景：** BCM 相关外设因供电电压不正确导致功能异常。

**修改：** 在 `myd-lr3576.dtsi` 中修正 BCM regulator 的电压配置为 3.3V。

### 案例三：dvrs_hw PCIe 设备权限（commit `52b9b38430b7`）
**背景：** CRY 应用需要访问 `/dev/dvrs_hw` 设备节点，但默认 SEAndroid 权限阻止访问。

**解决方案：** 添加 dvrs_hw 设备类型、文件上下文和 init RC 权限配置。

## 高级调试工具

### 1. 音频调试
```bash
# 通过 sysfs 查看 PDM 状态
cat /sys/devices/platform/fe*/*pdm*/registers

# 录制并分析音频频谱
tinycap /data/test.wav -D 0 -d 0 -r 48000 -b 16 -c 2 -n 5
# 将 test.wav 拉到 PC 用 Audacity 分析
```

### 2. Regulator 调试
```bash
# 监控 regulator 电压变化
watch -n 1 'cat /sys/kernel/debug/regulator/regulator_summary | head -30'
```

### 3. SEAndroid 权限调试
```bash
# 检查 SELinux 审计日志
adb shell dmesg | grep avc

# 使用 audit2allow 生成策略
adb shell dmesg | grep avc | audit2allow
```

## 注意事项
1. **PDM 时钟配置**：PDM 接口的时钟频率需与麦克风规格匹配（通常 1-4MHz）。
2. **Regulator 电压范围**：配置 regulator 电压时需确保在 IC 规格范围内。
3. **SEAndroid 权限最小化**：添加设备权限时应遵循最小权限原则，避免过度授权。
4. **音频延迟**：PDM 麦克风路径可能存在较大延迟，实时音频应用需注意。
5. **稳压器负载**：确认 BCM 稳压器的驱动能力满足外设需求。

---
*编写：高级工程师*
*最后更新：2026-04-24*
*基于 commit：647c82bb22ae, f38e577a21d4, 52b9b38430b7*
