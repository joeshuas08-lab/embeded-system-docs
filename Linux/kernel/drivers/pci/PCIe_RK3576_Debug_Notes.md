# PCIe 调试笔记 (RK3576 + Sample Card)

## 概述

RK3576 平台的 PCIe 调试包含 Root Complex (RC) 和 Endpoint (EP) 两种模式，涉及 Sample Card 适配、电源管理、热插拔等调试。

---

## 一、PCIe 基础移植

### 1.1 硬件拓扑

```
RK3576 PCIe2x1l0 (RC)
    │
    ├── PCIe Sample Card (默认)
    │       └── PERST → GPIO4_PB2
    │
    ├── PCIe Wi-Fi (由 c3381af8893 disabled)
    │
    └── 4G 模块 (由 f76636a5582 disabled, 后续删除无用 wifi_driver)
```

### 1.2 DTS 配置

`myz-lr3576-pcie.dtsi`：

```c
&pcie2x1l0 {
    status = "okay";
    reset-gpios = <&gpio4 RK_PB2 GPIO_ACTIVE_HIGH>;
    vpcie3v3-supply = <&vcc3v3_pcie>;
    vpcie1v8-supply = <&vcc1v8_pcie>;
};
```

### 1.3 EP 模式 (`9ad1093eafb`, `41c577424ec`)

RK3576 PCIe 可配置为 EP 模式：

```c
// DTS 配置 EP 模式
&pcie2x1l0 {
    rockchip,pcie-ep-mode;
};
```

EP 模式用于设备被外部 host 枚举的场景（如基带处理器通过 PCIe 连接）。本项目后续主要使用 RC 模式连接 Sample Card。

---

## 二、Sample Card 调试

### 2.1 版本回溯 (`521af8db134`)

Sample Card 从新版本回退到 2024/12 版本：

```diff
- 移除针对新版本 Sample Card 的特殊 DTS 配置
- 恢复 12/24 版本的 PCIe 时序参数
```

### 2.2 PERST IO 电压修复 (`abe8d6fa4db`)

**问题**：Sample Card 的 PERST# 引脚电压从 1.8V 异常跳变到 3.3V，导致 PCIe link 不稳定。

**根因**：RK3576 GPIO bank 的 IO 电压由对应的 VCCIO 供电决定，PERST 使用的 GPIO4 在某个配置下映射到 3.3V domain。

**修复**：DTS 中明确指定 PERST GPIO 的 pinctrl 配置：

```c
pinctrl-0 = <&pcie_perst>;
pinctrl-names = "default";

&pinctrl {
    pcie {
        pcie_perst: pcie-perst {
            rockchip,pins = <4 RK_PB2 RK_FUNC_GPIO &pcfg_pull_none>;
        };
    };
};
```

---

## 三、电源管理调试

### 3.1 电源域控制 (`966f688b359`)

**需求**：系统 suspend 时关闭 PCIe 电源，resume 时重新初始化。

**实现方法**：移除 regulator 的 `regulator-always-on` 属性，改为通过 GPIO 控制电源开关：

```diff
- regulator-always-on;
+ enable-active-high;
+ gpio = <&gpio4 RK_PB2 GPIO_ACTIVE_HIGH>;
```

改动的 regulator：
1. `vcc_1v8_s0` - 1.8V PCIe 参考电源
2. `vcc1v8_pcie` - 1.8V PCIe IO 电源
3. `vcc3v3_pcie` - 3.3V PCIe 主电源（新增 GPIO 控制）
4. `vcc_pcie_slot_5v` - 5V PCIe 插槽电源（暂时注释）

### 3.2 回退 (`b01808052a0`)

`b01808052a0` (Revert "force pcie power cut off when sys-suspend")：

强制断电导致某些 Sample Card 在 resume 后无法正确重新初始化。回退原则：保留部分电源，仅切除 3.3V 主电源。

### 3.3 省电模式

**L1.1 → L1.2 回退** (`c52bef74972`)

PCIe ASPM L1.2 省电级别更高但兼容性较差，部分设备进入 L1.2 后无法唤醒：

```
L1.0 → L1.1 → L1.2 (省电递增，兼容性递减)
```

回退到 L1.1 以兼容更多 Sample Card。

相关 defconfig 配置：
```makefile
# rockchip_linux_defconfig
CONFIG_PCIEASPM=y
# CONFIG_PCIEASPM_DEBUG is not set
```

---

## 四、PCIe 调试命令

```bash
# 查看 PCIe 设备
lspci
lspci -vvv                     # 详细信息

# 查看链路状态
cat /sys/kernel/debug/pci/*/link   # link 速率和宽度

# ASPM 状态
cat /sys/module/pcie_aspm/parameters/policy

# 强制 PCIe 复位（通过 GPIO）
echo 0 > /sys/class/gpio/gpio23/value   # PERST 拉低
sleep 1
echo 1 > /sys/class/gpio/gpio23/value   # PERST 释放

# 查看 PCIe 电源状态
cat /sys/kernel/debug/regulator/regulator_summary | grep pcie

# PCIe 吞吐测试
dd if=/dev/zero of=/dev/null bs=1M count=1000  # 通过 PCIe SSD 测试
```

---

## 五、经验总结

1. **PERST 时序**：PCIe 规范要求 PERST# 释放后至少等待 100ms 才能开始 link training
2. **IO 电压匹配**：GPIO 电压 domain 必须与设备要求匹配，否则 link training 阶段会失败
3. **ASPM 兼容性**：L1.2 虽然省电但兼容性差，不建议在产品中默认开启
4. **热插拔**：RK3576 PCIe 控制器支持 hotplug，但需要 BIOS/ATF 配置 NXP Reset 控制器
5. **调试入口**：使用 GPIO 控制 PCIe 电源对调试非常有用，建议保留 sysfs 控制接口
