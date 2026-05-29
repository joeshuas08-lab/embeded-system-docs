# Touch 调试笔记：ILI211x / GT1x (RK3576 + Android 14)

## 概述

项目涉及两种触控芯片：
1. **ILI211x** (ILITEK) - 主力触控方案，I2C 接口
2. **GT1x** (Goodix) - 备选方案，最早适配使用

---

## 一、GT1x 触控（初始方案）

### 1.1 移植 (`40a261b65c6`)

Goodix GT1x 是最早适配的触控方案，I2C 连接在 I2C2 bus：

```c
&i2c2 {
    status = "okay";
    
    gt1x: gt1x@14 {
        compatible = "goodix,gt1x";
        reg = <0x14>;
        irq-gpios = <&gpio0 RK_PB5 IRQ_TYPE_EDGE_RISING>;
        reset-gpios = <&gpio1 RK_PA2 GPIO_ACTIVE_LOW>;
    };
};
```

### 1.2 90° 旋转 (`b358c6d6c1c`)

触控方向与屏幕方向不匹配，通过修改上报坐标映射：

```c
// touch driver spin 90°
// 修改 input_report_abs 的坐标转换
input_report_abs(ts->input_dev, ABS_X, ts->screen_height - y);
input_report_abs(ts->input_dev, ABS_Y, x);
```

### 1.3 响应灵敏度 (`d3ada63db22`)

触摸响应延迟的问题修复，调整采样率参数和 IRQ 触发模式：

```c
// 修复前
irq-gpios = <&gpio0 RK_PB5 IRQ_TYPE_EDGE_RISING>;

// 修复后
irq-gpios = <&gpio0 RK_PB5 IRQ_TYPE_LEVEL_LOW>;
```

---

## 二、ILI211x 触控（主力方案）

### 2.1 驱动移植 (`97f3461dbc4`, `b6b50bfea84`)

ILITEK ILI211x 驱动目录：

```
kernel-6.1/drivers/input/touchscreen/
    ├── ili211x.c                # ILI211x 主驱动入口
    ├── ilitek_drv_main.c        # ILITEK 驱动主逻辑 (8902 lines)
    ├── ilitek_drv_common.h      # 驱动通用头文件
    ├── ilitek_drv_update.c      # 固件升级逻辑 (2406 lines)
    ├── new_mp_test/
    │   ├── mp_main.c            # 量产测试主流程
    │   ├── mp_test.c            # 测试项
    │   ├── mp_parse.c           # 测试配置解析
    │   ├── mp_result.c          # 测试结果处理
    │   └── mp_misc.c            # 测试工具函数
    └── Kconfig / Makefile
```

驱动总代码量约 16000 行，包含三重功能：
1. **基础触控**：坐标上报、手势识别
2. **固件升级**：支持 I2C 升级触控固件
3. **量产测试**：整机生产阶段的触控测试

### 2.2 省电模式 (`32750b3ff06`)

TP 切换到 powersave 模式，禁止空闲时自动升级：

```c
// 在 suspend 时停止升级检查
static int ili211x_suspend(struct device *dev)
{
    cancel_delayed_work(&ili211x->fw_update_work);
    // ... 进入低功耗模式
}
```

### 2.3 DTS 配置

```c
&i2c2 {
    status = "okay";
    
    ilitek@41 {
        compatible = "ilitek,ili211x";
        reg = <0x41>;
        interrupt-parent = <&gpio0>;
        interrupts = <RK_PB5 IRQ_TYPE_EDGE_FALLING>;
        irq-gpios = <&gpio0 RK_PB5 GPIO_ACTIVE_LOW>;
        reset-gpios = <&gpio1 RK_PA2 GPIO_ACTIVE_LOW>;
        
        status = "okay";
    };
};
```

---

## 三、调试方法

### 3.1 触控坐标测试

```bash
# 使用 getevent 查看原始触控事件
getevent -lt

# 示例输出
# /dev/input/event3: EV_ABS       ABS_MT_POSITION_X    00000258
# /dev/input/event3: EV_ABS       ABS_MT_POSITION_Y    00000190
# /dev/input/event3: EV_SYN       SYN_REPORT           00000000

# 使用 evtest 交互测试
evtest /dev/input/event3
```

### 3.2 ILI211x 固件升级

```bash
# 查看当前固件版本
cat /proc/ilitek/fw_ver

# 触发固件升级（将固件放入指定路径）
cp firmware.hex /lib/firmware/ilitek/
echo 1 > /proc/ilitek/update
```

### 3.3 量产测试

```bash
# 进入测试模式
echo 1 > /proc/ilitek/mp_test

# 查看测试结果
cat /proc/ilitek/mp_result
```

---

## 四、经验总结

1. **IRQ 选择**：EDGE 触发漏报率高，遇到响应延迟时优先考虑 LEVEL 模式
2. **固件升级**：ILITEK 升级过程不能断电，建议在升级前检查电池电量
3. **旋转适配**：触摸坐标旋转后需同时确认手势区域映射（如边缘手势、导航手势）
4. **量产测试**：ILITEK 内建 MP (Mass Production) 测试模式可测试开路/短路，对产线良率控制很重要
5. **功耗管理**：触控 I2C 在 system suspend 期间需正确进入休眠，否则会阻止系统进入深度睡眠
