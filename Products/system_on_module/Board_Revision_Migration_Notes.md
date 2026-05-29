# 硬件版本迁移调试笔记 (LR3576: v1 → v2 → v3)

## 概述

LR3576 核心板经历了 v1/v2/v3 三个硬件版本迭代，每次硬件改版都伴随相应的 DTS 调整。

---

## 一、版本演化

| 版本 | 主要变更 | 对应 DTS 提交 |
|------|---------|-------------|
| v1 | 初始设计 | SDK rkr7 基线 |
| v2 | CSI 引脚变更、Type-C 调整 | `30c0854b666`, `2168acc2423` |
| v3 | 新增器件供电、GPIO 重构 | `3a60ff49763` |

---

## 二、v1 → v2：CSI 引脚迁移

### 2.1 背景

硬件团队重新布局摄像头接口，MIPI CSI 相关 GPIO 从 GPIO0 domain 移到 GPIO3 domain。

### 2.2 变更内容

**提交 `30c0854b666`** (port MIPI CSI2 to v2 board)：

```diff
  vcc_mipipwr: vcc-mipipwr-regulator {
-     gpio = <&gpio0 RK_PD3 GPIO_ACTIVE_HIGH>;
+     gpio = <&gpio3 RK_PC5 GPIO_ACTIVE_HIGH>;
  };
```

同时新增了 rkvpss 和 rkisp_vir0_sditf 节点的使能。

**提交 `2168acc2423`** (MIPI-CSI Compatible with v2 board)：

Type-C DTS 中调整了 USB 角色切换逻辑。

### 2.3 CSI 修复 (`8b01eecc34c`)

硬件版本迁移后的一次 DTS 清理提交：

```diff
# 主要变更
1. data-lanes: <1 2> → <1 2 3 4>  # 从 2-lane 改为 4-lane
2. 移除 power-domains = <&power RK3576_PD_VI>  # 电源域修正
3. 移除重复的 reset-gpios/avdd-supply 属性  # 去重
4. 移除 rkvpss 节点使能  # 移除未使用的 ISP 后端
5. pinctrl 整理  # 删除重复的 pinmux 定义
```

关键修复：CSI 数据通道从 2-lane 扩展为 4-lane。实际硬件走线使用 4 对差分对，之前只配置 2-lane 导致带宽不足。

---

## 三、v2 → v3：新增器件供电

### 3.1 背景

v3 板在硬件上增加了陀螺仪和 WiFi/BT 模块的独立供电控制，改用 GPIO 控制电源开关。

### 3.2 变更内容  

**提交 `3a60ff49763`** (fit v3 board)：

```diff
+ fake_pwr_gyro: fake-pwr-gyro {
+     compatible = "regulator-fixed";
+     regulator-name = "fake_pwr_gyro";
+     gpio = <&gpio4 RK_PC1 GPIO_ACTIVE_HIGH>;
+ };
+ 
+ fake_pwr_wifibt: fake-pwr-wifibt {
+     compatible = "regulator-fixed";
+     regulator-name = "fake_pwr_wifibt";
+     gpio = <&gpio4 RK_PC3 GPIO_ACTIVE_HIGH>;
+ };
```

**fake_pwr_** 命名含义：这些 regulator 不直接给器件供电（器件已有常开电源），而是作为器件复位/使能信号，遵循 regulator 框架的延迟和时序控制。

### 3.3 音频 GPIO 重构

```diff
  es8156_sound: es8156-sound {
      spk-con-gpio = <&gpio4 RK_PA4 GPIO_ACTIVE_LOW>;
+     pinctrl-names = "default";
+     pinctrl-0 = <&spk_ctl_gpio>;
  };
+
+ audio {
+     spk_ctl_gpio: spk-ctl-gpio {
+         rockchip,pins = <4 RK_PA4 RK_FUNC_GPIO &pcfg_pull_up>;
+     };
+ };
```

增加了 audio pinctrl 节点，将喇叭控制引脚纳入 pinmux 管理，避免其他驱动误配置该引脚。

### 3.4 PCIe GPIO 重构

```diff
+ boost {
+     vcc_5v0_dev_en: vcc-5v0-dev-en {
+         rockchip,pins = <4 RK_PB2 RK_FUNC_GPIO &pcfg_pull_up>;
+     };
+ };
```

PCIe 相关的 5V 使能引脚增加到 boost pinctrl 分组中。

---

## 四、BCM  regulator 修复 (`f38e577a21d`)

### 问题

v3 板上 WiFi/BT 模块（BCM 方案）不工作，供电 GPIO 未正确配置。

### 修复

```diff
  wireless_wlan: wireless-wlan {
-     // WIFI,poweren_gpio = <&gpio1 RK_PC6 GPIO_ACTIVE_HIGH>;
+     WIFI,poweren_gpio = <&gpio1 RK_PC6 GPIO_ACTIVE_HIGH>;
  };
```

之前被注释掉的 WiFi 使能引脚在 v3 板上需要重新使能，因为 v3 的供电方案发生了变化。

---

## 五、PCIe Reset GPIO 导出 (`0248a4e6e4d`)

调试需求：需要从 userspace 控制 PCIe PERST# 信号：

```c
// drivers/pci/controller/dwc/pcie-dw-rockchip.c
+ gpio_request_array(pcie->reset_gpios, 1);
+ gpio_export(pcie->reset_gpios[0].gpio, true);
```

通过 sysfs 操作：
```bash
# 查看 PCIe reset GPIO 编号
gpioinfo | grep pcie

# 控制 PERST#
echo 0 > /sys/class/gpio/gpio<N>/value  # assert PERST
sleep 1
echo 1 > /sys/class/gpio/gpio<N>/value  # de-assert PERST
```

---

## 六、调试经验

1. **硬件版本兼容**：DTS 应通过 `#include` 和条件编译处理多版本，减少分支管理成本
2. **GPIO 迁移**：硬件改版涉及 GPIO 重映射时，务必同时更新 pinctrl 节点
3. **fake_pwr 模式**：用 regulator 框架管理 GPIO 的使能/复位可以利用其延迟控制，但命名要加注释避免困惑
4. **CSI data-lanes**：lane 数配置错误会导致 mipi 信号无法 lock，用示波器测量前确认 DTS 配置
