# 高级工程师调试记录：MIPI 面板 ESD 自恢复调试

## 功能概述
在 RK3576 Android 14 平台中，MIPI DSI 接口的显示屏可能因静电放电（ESD）或其他干扰导致显示异常（花屏、黑屏、闪烁）。ESD 自恢复机制用于检测显示链路异常并自动恢复，无需用户手动重启设备。该功能涉及 panel 驱动、DSI 控制器和 U-Boot 视频驱动的协同配合。

## 调试方法

### 1. ESD 检测日志查看
```bash
# 查看 ESD 恢复相关日志
dmesg | grep -E "(esd|panel|dsi|recover)"

# 查看 DRM 事件
dmesg | grep -i drm
```

### 2. 手动模拟 ESD 测试
```bash
# 触发 panel 异常（谨慎使用）
echo 1 > /sys/kernel/debug/dri/0/trigger_esd

# 查看是否触发恢复流程
dmesg | tail -20
```

### 3. panel 状态监控
```bash
# 查看 panel 状态
cat /sys/class/drm/card0-HDMI-A-1/status
cat /sys/class/drm/card0-DSI-1/status

# 查看 connector 状态
cat /sys/kernel/debug/dri/0/connectors
```

## ESD 恢复实现原理

ESD 自恢复机制通过以下流程实现（commit `8540ee685fe`）：

```
Panel 异常 (如花屏)
    ↓
DSI 控制器检测到错误 (video mode 异常)
    ↓
触发 DRM 面板事件 (panel event)
    ↓
执行恢复流程：
    1. 关闭 DSI 控制器
    2. 复位 panel (GPIO toggle)
    3. 重新初始化 panel (发送初始化序列)
    4. 重新启动 DSI 控制器
    5. 恢复显示内容
```

### 内核实现关键点

**panel-simple.c**（commit `8540ee685fe`）：
```c
// ESD 恢复功能添加
static int panel_simple_esd_recover(struct drm_panel *panel)
{
    struct panel_simple *p = to_panel_simple(panel);

    dev_info(panel->dev, "ESD recovery started\n");

    // 1. 关闭 panel
    panel_simple_disable(panel);

    // 2. 硬件复位
    gpiod_set_value_cansleep(p->reset_gpio, 1);
    msleep(100);
    gpiod_set_value_cansleep(p->reset_gpio, 0);
    msleep(100);

    // 3. 重新上电和初始化
    panel_simple_prepare(panel);

    // 4. 使能 panel
    panel_simple_enable(panel);

    dev_info(panel->dev, "ESD recovery completed\n");
    return 0;
}
```

**dw-mipi-dsi2-rockchip.c**：
```c
// DSI 控制器端配合恢复
static void dw_mipi_dsi2_esd_recover(struct dw_mipi_dsi2 *dsi)
{
    // 复位 DSI 控制器内部状态
    dsi_write(dsi, DSI_RSTN, 0);
    usleep_range(1000, 2000);
    dsi_write(dsi, DSI_RSTN, 1);

    // 重新配置视频模式参数
    dw_mipi_dsi2_video_mode_config(dsi);
    dw_mipi_dsi2_video_mode_enable(dsi);
}
```

## 常见问题及解决方案

### 1. ESD 恢复后显示仍异常
**可能原因：**
- Panel 初始化序列不完整
- DSI 控制器恢复后参数未正确重配
- 恢复时序未满足 panel 规格要求

**调试步骤：**
1. **检查恢复流程日志**：
   ```bash
   dmesg | grep -A 20 "ESD recovery started"
   ```

2. **确认时序参数**：对比 normal 初始化与恢复后的初始化序列是否一致

3. **调整恢复时序**：
   ```c
   // 延长复位和等待时间
   gpiod_set_value_cansleep(p->reset_gpio, 1);
   msleep(200);  // 从 100ms 延长到 200ms
   ```

### 2. ESD 触发过于频繁（误检测）
**可能原因：**
- DSI 信号完整性差
- 电源噪声导致误检测
- 检测阈值过于灵敏

**解决方案：**
1. **硬件检查**：使用示波器测量 DSI 时钟和数据线信号质量
2. **调整检测阈值**：
   ```c
   // 增加检测去抖计数
   #define ESD_DEBOUNCE_COUNT 3  // 连续检测到 3 次才触发恢复
   ```
3. **增加恢复间隔限制**：
   ```c
   // 两次恢复之间至少间隔 30 秒
   #define ESD_RECOVER_INTERVAL_MS 30000
   ```

### 3. ESD 恢复过程中系统卡死
**可能原因：**
- DRM 锁未正确释放
- 恢复流程中调用了可能休眠的函数
- 面板电源序列不正确

**调试方法：**
```bash
# 查看恢复过程中的内核栈
echo w > /proc/sysrq-trigger
dmesg | tail -30

# 检查是否有死锁
echo l > /proc/sysrq-trigger
```

## 调试案例

### 案例：MIPI 面板 ESD 自动恢复实现（commit `8540ee685fe`）
**背景：** 量产测试中发现部分设备在静电测试后出现花屏，需要人工重启才能恢复，影响用户体验。需要实现自动检测和恢复机制。

**关键修改：**
1. **panel-simple.c**：添加 ESD 恢复回调函数
2. **dw-mipi-dsi2-rockchip.c**：添加 DSI 控制器复位和重配逻辑
3. **U-Boot dw_mipi_dsi2.c**：同步更新 U-Boot 阶段的恢复支持

**实现要点：**
1. ESD 检测基于 DSI 视频模式的状态监控
2. 恢复流程包括 panel 下电 → 硬件复位 → 重新上电 → 重新初始化
3. 恢复完成后需要刷新 framebuffer 内容

**验证方法：**
```bash
# 1. 通过 debug 节点触发 ESD 模拟
echo 1 > /sys/kernel/debug/dri/0/trigger_esd

# 2. 观察恢复日志
dmesg -w | grep -E "(esd|panel|dsi)"

# 3. 确认显示恢复正常
# （目测或通过拍照对比）
```

**经验总结：**
- 不同 panel 的初始化序列差异较大，恢复逻辑需要针对具体 panel 调试
- DSI 控制器在恢复后需完全重新配置 video mode 参数
- 恢复过程中需要处理 DRM 内部锁状态，避免死锁
- U-Boot 阶段的恢复逻辑可确保在系统启动过程中也能处理面板异常

## 高级调试工具

### 1. DSI 信号分析
- **示波器/逻辑分析仪**：测量 DSI clock/data lane 信号
- **眼图测试**：评估信号完整性

### 2. ESD 模拟测试
```bash
# 使用 ESD 模拟器进行接触放电测试
# 通常为 ±4kV ~ ±8kV 接触放电
# 测试点：金属边框、连接器外壳、按键
```

### 3. 内核 ESD 调试
```bash
# 监控 DRM 事件
echo 0x1ff > /sys/module/drm/parameters/debug

# 查看 panel 寄存器状态
cat /sys/kernel/debug/dri/0/panel-regs
```

## 注意事项
1. **ESD 测试安全**：ESD 测试时确保设备接地良好，避免损坏敏感元器件。
2. **恢复成功率**：ESD 恢复不是万能的，严重硬件损坏无法通过软件恢复。
3. **用户体验**：恢复过程中可能出现短暂黑屏，需确保恢复时间在可接受范围（< 2 秒）。
4. **兼容性**：不同 panel 型号需要单独验证 ESD 恢复流程。
5. **误恢复避免**：正常使用中不应误触发 ESD 恢复，需设置合理的检测阈值。

---
*编写：高级工程师*
*最后更新：2026-04-24*
*基于 commit：8540ee685fe*
