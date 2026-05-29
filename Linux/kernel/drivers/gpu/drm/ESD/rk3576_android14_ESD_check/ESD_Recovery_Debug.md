# MIPI DSI 面板 ESD 自动恢复调试笔记

## 概述

在 RK3576 + Android 14 平台上，MIPI DSI 面板在 ESD 干扰下可能进入异常状态（画面冻结、花屏）。通过修改 `panel-simple.c` 驱动，实现了硬件 ESD 监测和自动恢复机制。

提交：`8540ee685fe` (add : esd mipi panel auto-recovery)

---

## 一、ESD 检测机制

### 1.1 硬件接口

面板提供 **test GPIO**（DTS 中 `test-gpios` 属性），用于触发和检测：

```c
// DTS 配置
test-gpios = <&gpio1 RK_PB0 GPIO_ACTIVE_HIGH>;
```

### 1.2 检测流程

```c
static u8 esd_check_value_read(struct panel_simple *panel)
{
    // 1. 切换 test GPIO 电平，触发面板回传状态
    gpiod_direction_output(panel->test_gpio, 0);
    mdelay(1);
    gpiod_direction_output(panel->test_gpio, 1);
    
    // 2. 通过 DSI 读取电源模式寄存器 0x0A
    dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
    err = mipi_dsi_dcs_get_power_mode(dsi, &value);
    dsi->mode_flags |= MIPI_DSI_MODE_LPM;
}
```

### 1.3 判断逻辑

```
正常状态：寄存器 0x0A 返回 0x9C
异常状态：多次读取返回非 0x9C
```

| 读取结果 | 含义 |
|---------|------|
| 0x9C | 面板正常 |
| 其他值 | 面板异常，进入恢复计数 |

### 1.4 重试计数

```c
static void esd_check_handler(struct work_struct *work)
{
    // 正常：检查间隔 1000ms
    // ESD 触发：检查间隔缩短到 300ms
    // 连续 3 次异常 → 触发 reset_work
}
```

---

## 二、恢复机制

### 2.1 Reset Workqueue

```c
static void esd_mipi_reset(struct work_struct *work)
{
    // 重置前取消 ESD check
    cancel_delayed_work_sync(&p->esd_check_dw);
    
    // DPMS OFF
    conn->force = DRM_FORCE_OFF;
    conn->funcs->fill_modes(conn, ...);
    
    msleep(1000);  // 等待面板掉电
    
    // DPMS ON
    conn->force = DRM_FORCE_ON;
    conn->funcs->fill_modes(conn, ...);
}
```

使用 DRM connector 的 `force` 属性实现重置，相当于软件级掉电重启：

```
Force OFF → 等待 1s → Force ON → 面板初始化序列重新执行
```

### 2.2 生命周期管理

在 panel 生命周期事件中管理 ESD check：

```c
panel_simple_enable()
    → schedule_delayed_work(esd_check_dw, 5000ms)  // 开启后启动
    
panel_simple_unprepare()
    → cancel_delayed_work_sync(esd_check_dw)       // 关闭前取消
```

---

## 三、Uboot/Kernel 配合

UBoot 端的 dw_mipi_dsi2 驱动也需要对应修改：

```
u-boot/drivers/video/drm/dw_mipi_dsi2.c
    + 2 lines (同步调整)
kernel-6.1/drivers/gpu/drm/rockchip/dw-mipi-dsi2-rockchip.c
    + 2 lines (同步调整)
```

---

## 四、调试方法

```bash
# 查看 ESD 检查日志
echo 'file panel-simple.c +p' > /sys/kernel/debug/dynamic_debug/control

# 触发 ESD（模拟）
# 拉低面板供电或注入噪声

# 查看恢复过程 dmesg 日志
dmesg | grep -i "esd\|panel\|mipi"
```

---

## 五、经验总结

1. **GPIO 时序**：test GPIO 的 pulse timing 必须匹配面板 datasheet 要求
2. **恢复可靠性**：连续 3 次检测才触发恢复，避免误触发
3. **DPMS 方式**：使用 DRM force 而不是硬件 reset，确保 DRM 状态一致
4. **并发安全**：reset_work 和 esd_check_dw 之间通过 cancel/work 机制互斥
5. **测试覆盖**：ESD 测试需覆盖整机工作状态（播放视频、预览摄像头等场景）
