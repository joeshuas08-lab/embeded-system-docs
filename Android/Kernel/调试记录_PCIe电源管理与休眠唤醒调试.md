# 高级工程师调试记录：PCIe 电源管理与休眠唤醒调试

## 功能概述
PCIe 外设在系统休眠（suspend）和唤醒（resume）过程中的电源管理是嵌入式系统调试的难点。在 RK3576 Android 14 平台中，PCIe 接口连接 SampleCard 或其他外设时，需要在系统休眠时切断 PCIe 电源以降低功耗，并在唤醒时重新初始化 PCIe 驱动以恢复外设功能。调试涉及设备树配置、电源域控制和驱动初始化时序。

## 调试方法

### 1. PCIe 电源状态检查
```bash
# 查看 PCIe 控制器电源状态
cat /sys/kernel/debug/pm_genpd/pm_genpd_summary | grep -i pcie

# 查看 PCIe 设备电源状态
cat /sys/bus/pci/devices/0000:*/power/control
cat /sys/bus/pci/devices/0000:*/power/runtime_status

# 查看 PCIe 链路状态
lspci -vvv
```

### 2. 休眠唤醒日志分析
```bash
# 查看休眠唤醒过程中的 PCIe 日志
dmesg | grep -E "(suspend|resume|pcie|PCIe)"

# 查看系统挂起状态
cat /sys/power/state

# 手动触发休眠测试
echo mem > /sys/power/state
```

### 3. 电源域控制验证
```bash
# 查看 PCIe 电源域
cat /sys/kernel/debug/pm_genpd/pm_genpd_summary | grep pcie

# 查看 GPIO 供电状态
cat /sys/kernel/debug/gpio | grep -i pcie
```

## 常见问题及解决方案

### 1. 休眠后 PCIe 外设无法唤醒（commit `a2f74260ec3f`, `966f688b359a`）
**问题：** 系统从休眠唤醒后，PCIe 外设（如 SampleCard）无法正常工作，链路未恢复。

**原因分析：**
- PCIe 控制器在休眠时未完全断电，导致唤醒后状态不一致
- 外设电源未在休眠时切断，但控制器已丢失上下文
- 驱动初始化顺序问题

**解决方案：**
在设备树中配置休眠时强制切断 PCIe 电源：

```dts
// myz-lr3576-pcie.dtsi
&pcie2x1l2 {
    // 使能 PCIe 电源域控制
    rockchip,power-cut-off = <1>;
    
    // 在系统休眠时关闭 PCIe 电源
    pinctrl-0 = <&pcie20x1_pins>;
    pinctrl-1 = <&pcie20x1_pins_sleep>;
    pinctrl-names = "default", "sleep";
    
    // 休眠时关闭供电
    vpcie3v3-supply = <&vcc3v3_pcie>;
    vpcie1v8-supply = <&vcc1v8_pcie>;
};
```

**关键实现：**
```c
// 通过 pinctrl 的 sleep 状态在 suspend 时切斷电源
static int rockchip_pcie_suspend(struct device *dev)
{
    // 保存 PCIe 控制器状态
    // 切断 PCIe 电源域
    // 关闭外设供电
    pinctrl_pm_select_sleep_state(dev);
    return 0;
}

static int rockchip_pcie_resume(struct device *dev)
{
    // 恢复 PCIe 电源域
    // 重新初始化控制器
    // 重新枚举 PCIe 总线
    pinctrl_pm_select_default_state(dev);
    return rockchip_pcie_init(dev);
}
```

**调试要点：**
```bash
# 休眠前记录 PCIe 状态
lspci -vvv > /data/pcie_before.txt

# 触发休眠
echo mem > /sys/power/state

# 唤醒后检查恢复情况
lspci -vvv > /data/pcie_after.txt
diff /data/pcie_before.txt /data/pcie_after.txt
```

### 2. PCIe 电源切断后系统卡死
**问题：** 休眠时切断 PCIe 电源导致系统无法完全进入休眠或唤醒时死机。

**迭代历程（commit `a2f74260ec3f` → `b01808052a01` → `966f688b359a`）：**
1. **首次实现**（`a2f74260ec3f`）：在 `myz-lr3576-pcie.dtsi` 中配置电源切断
2. **回退**（`b01808052a01`）：因部分外设兼容性问题 revert
3. **重新实现**（`966f688b359a`）：修复已知问题后重新应用

**排查方法：**
```bash
# 检查休眠唤醒过程中的错误
dmesg | grep -E "(fail|error|abort)" -i

# 检查电源域状态转换
cat /sys/kernel/debug/pm_genpd/pm_genpd_summary

# 检查 PCIe 控制器寄存器状态
# 通过 debugfs 查看
cat /sys/kernel/debug/pci/*/registers
```

## 调试案例

### 案例：PCIe 休眠唤醒电源管理（多 commit 迭代）
**背景：** SampleCard 在系统休眠唤醒后无法正常工作，需要重新上电和初始化。

**调试过程：**
1. **问题发现**：休眠唤醒后 `lspci` 看不到设备或链路异常
2. **根因分析**：PCIe 控制器在休眠时未完全断电，导致唤醒后状态混乱
3. **首次修复**（`a2f74260ec3f`）：配置设备树使能电源切断
4. **回退**（`b01808052a01`）：发现部分外设兼容性问题
5. **重新实现**（`966f688b359a`）：优化电源控制时序，重新使能

**经验总结：**
- PCIe 休眠唤醒需要综合考虑控制器、外设和电源域的协同
- 不同外设对电源切断的容忍度不同，需要逐个验证
- 电源域的控制时序至关重要，过早或过晚都会导致问题
- 建议通过 pinctrl sleep 状态管理电源切换

## 高级调试工具

### 1. 电源域调试
```bash
# 查看电源域状态
cat /sys/kernel/debug/pm_genpd/pm_genpd_summary

# 查看电源域依赖关系
cat /sys/kernel/debug/pm_genpd/pm_genpd_dependencies
```

### 2. PCIe 链路调试
```bash
# 查看 PCIe 链路能力
lspci -vvv -s 01:00.0

# 查看 PCIe 链路状态寄存器
setpci -s 01:00.0 0x60.l
```

### 3. 休眠唤醒跟踪
```bash
# ftrace 跟踪休眠唤醒流程
echo 1 > /sys/kernel/debug/tracing/events/power/suspend_resume/enable
cat /sys/kernel/debug/tracing/trace_pipe
```

### 4. GPIO 状态监控
```bash
# 监控 PCIe 相关 GPIO 电平变化
watch -n 1 'cat /sys/kernel/debug/gpio | grep pcie'
```

## 注意事项
1. **电源时序**：PCIe 电源切断和恢复的时序必须严格遵循外设 datasheet 要求。
2. **兼容性验证**：每次修改 PCIe 电源管理策略后需测试所有 PCIe 外设。
3. **唤醒延迟**：PCIe 重新初始化可能增加唤醒时间，需在功耗和性能间平衡。
4. **备份状态**：休眠前需保存 PCIe 控制器关键寄存器状态，用于唤醒后恢复。
5. **外设热插拔**：PCIe 外设不支持热插拔，休眠前应确保设备已正常关闭。

---
*编写：高级工程师*
*最后更新：2026-04-24*
*基于 commit：a2f74260ec3f, b01808052a01, 966f688b359a*
