# 高级工程师调试记录：PCIe 复位与电平适配

## 功能概述
PCIe 接口的复位信号和 IO 电平适配是保证外设正常工作的关键。在 RK3576 Android 14 平台中，PCIe 外设（如 SampleCard、WiFi 模块）需要正确的复位时序和电平匹配。调试涉及 PCIe 控制器驱动中的 GPIO 导出、设备树中的电平配置和硬件版本适配。

## 调试方法

### 1. PCIe 复位 GPIO 检查
```bash
# 查看 PCIe 复位 GPIO 状态
cat /sys/class/gpio/gpio*/direction
cat /sys/class/gpio/gpio*/value

# 查看 GPIO 控制器状态
cat /sys/kernel/debug/gpio | grep -i pcie
```

### 2. PCIe 链路状态检查
```bash
# 查看 PCIe 链路状态
lspci
lspci -vvv

# 查看链路速度和宽度
cat /sys/class/pci_bus/0000:*/device/link_speed
cat /sys/class/pci_bus/0000:*/device/link_width
```

### 3. 设备树验证
```bash
# 查看 PCIe 设备树节点
cat /sys/firmware/devicetree/base/pcie*/status
cat /sys/firmware/devicetree/base/pcie*/reset-gpios
```

## 常见问题及解决方案

### 1. PCIe 外设无法识别
**可能原因：**
- 复位 GPIO 未正确导出或控制
- 外设的电平不匹配（1.8V vs 3.3V）
- 复位时序不满足外设要求

### 2. PCIe 复位 GPIO 导出（commit `0248a4e6e4d`）
**问题：** PCIe 复位引脚未作为 GPIO 导出，导致用户空间无法控制复位。

**解决方案：**
在 `pcie-dw-rockchip.c` 驱动中添加 GPIO 导出：
```c
// 在 PCIe 控制器初始化后导出复位 GPIO
static int rockchip_pcie_gpio_export(struct device *dev)
{
    struct gpio_desc *reset_gpio;

    reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(reset_gpio))
        return PTR_ERR(reset_gpio);

    // 导出为 GPIO 以便用户空间访问
    gpiod_export(reset_gpio, false);
    gpiod_export_link(dev, "pcie-reset", reset_gpio);

    return 0;
}
```

**调试方法：**
```bash
# 确认复位 GPIO 已导出
ls -l /sys/class/gpio/ | grep pcie

# 手动控制复位
echo 0 > /sys/class/gpio/gpio<N>/value  # 复位
sleep 1
echo 1 > /sys/class/gpio/gpio<N>/value  # 释放复位
```

### 3. SampleCard PERST 电平从 1.8V 改为 3.3V（commit `abe8d6fa4db`）
**问题：** PCIe SampleCard 的 PERST（复位）引脚使用 1.8V 电平导致某些外设无法正确复位，需要改为 3.3V 电平。

**关键修改（`myd-lr3576.dtsi`, `myz-lr3576-pcie.dtsi`）：**
```dts
&pcie2x1l2 {
    // PERST 引脚电平从 1.8V 改为 3.3V
    pinctrl-0 = <&pcie20x1_perstn_h>;

    // 确认复位 GPIO 对应的电压域
    reset-gpios = <&gpio3 RK_PB5 GPIO_ACTIVE_LOW>;
};
```

**设备树 pinctrl 配置：**
```dts
&pinctrl {
    pcie {
        pcie20x1_perstn_h: pcie20x1-perstn-h {
            rockchip,pins =
                <3 RK_PB5 RK_FUNC_GPIO &pcfg_pull_none_3v3>;  // 3.3V
        };
    };
};
```

**验证方法：**
```bash
# 使用示波器测量 PERST 引脚电平
# 应在 3.3V（高电平）和 0V（低电平）之间切换
```

### 4. PCIe SampleCard 回退到 12 月版本（commit `521af8db134`）
**背景：** 某些 PCIe 外设在更新配置后工作异常，回退到 12 月 24 日的已知稳定版本。

**调试步骤：**
1. **对比变更**：检查设备树中 PCIe 相关配置的差异
2. **确认回归**：使用 git bisect 定位引入问题的 commit
3. **回退策略**：在 `myd-lr3576.dts` 和 `myz-lr3576-pcie.dtsi` 中删除或调整新增配置

```bash
# 查看回退涉及的变更
git diff 521af8db134^..521af8db134
```

## 调试案例

### 案例一：PCIe SampleCard 适配（多 commit 协作）
**背景：** 适配第三方 PCIe SampleCard，解决识别和稳定性的问题。

**迭代历程：**
1. **电平修正**（commit `abe8d6fa4db`）：PERST 信号从 1.8V 改为 3.3V，满足外设电平要求
2. **版本回退**（commit `521af8db134`）：回退到 12/24 稳定版本，排除近期变更的干扰
3. **复位导出**（commit `0248a4e6e4d`）：导出复位 GPIO，便于调试和电源管理

**经验总结：**
- PCIe 外设适配时，优先确认电平匹配（1.8V vs 3.3V）
- 复位时序必须满足外设 datasheet 要求
- 保持设备树变更的可追溯性，便于快速回退

## 高级调试工具

### 1. 示波器/逻辑分析仪
- **测量 PERST 时序**：确认复位脉冲宽度
- **测量 REFCLK**：确认 100MHz 参考时钟质量
- **测量电平**：确认各引脚电平符合外设规格

### 2. PCIe 分析工具
```bash
# lspci 详细输出
lspci -vvv -s 01:00.0

# PCIe 链路统计
cat /sys/kernel/debug/pci/*/link
```

### 3. GPIO 调试
```bash
# 监控 GPIO 状态变化
cat /sys/kernel/debug/gpio
watch -n 1 'cat /sys/kernel/debug/gpio | grep pcie'
```

## 注意事项
1. **电平匹配**：PCIe 外设的 IO 电平必须与主控匹配，错误的电平可能导致外设损坏。
2. **复位时序**：不同外设对复位脉冲宽度有不同的要求，需查阅 datasheet 确认。
3. **热插拔限制**：PCIe 外设通常不支持热插拔，操作前需先断电。
4. **信号完整性**：高速信号对 layout 敏感，调试异常时需考虑信号质量问题。

---
*编写：高级工程师*
*最后更新：2026-04-24*
*基于 commit：0248a4e6e4d, abe8d6fa4db, 521af8db134*
