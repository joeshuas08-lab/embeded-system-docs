# ICM42607 陀螺仪调试笔记 (RK3576 + Android 14)

## 概述

ICM42607 是 TDK InvenSense 出品的 6 轴 IMU（加速度计 + 陀螺仪），通过 SPI 接口与 RK3576 连接。

---

## 一、驱动移植 (`479f831571b`)

### 1.1 DTS 配置

```c
&spi2 {
    status = "okay";
    
    icm42607: icm42607@0 {
        compatible = "invensense,icm42607";
        reg = <0>;
        spi-max-frequency = <10000000>;
        interrupt-parent = <&gpio3>;
        interrupts = <RK_PC1 IRQ_TYPE_LEVEL_HIGH>;
    };
};
```

### 1.2 驱动结构

使用 InvenSense 通用驱动框架（icm4260x 系列），ICM42607 是 ICM4260x 家族的变体：

```c
// include/linux/icm4260x.h
#define ICM42607_DEVICE_ID  0x47  // WHO_AM_I 寄存器值
```

---

## 二、陀螺仪方向修正 (`cb5bded2424`, `9b75f73c2af`)

### 2.1 问题

IMU 安装方向与系统坐标系不匹配，导致上报的角速度/加速度方向错误。典型表现：
- Android Sensor HAL 读取的 orientation 数据异常
- 屏幕旋转方向与实际物理旋转方向不一致

### 2.2 修复方法

在驱动中修改 axis remap，调整各轴的方向和映射：

```c
// 修复后配置（调整 axis map）
static const struct icm4260x_axis_map icm42607_axis_map = {
    .axis_x = { .sign = 1,  .reg = 0 },  // X 轴正向
    .axis_y = { .sign = -1, .reg = 1 },  // Y 轴反向
    .axis_z = { .sign = 1,  .reg = 2 },  // Z 轴正向
};
```

### 2.3 验证方法

```bash
# 查看原始传感器数据
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_z_raw

# 验证方向：将设备平放
# X/Y 轴加速度应接近 0，Z 轴应接近 +1g
```

---

## 三、调试命令

```bash
# 查看 IIO 设备列表
ls /sys/bus/iio/devices/

# 查看传感器数据精度和范围
cat /sys/bus/iio/devices/iio:device0/in_accel_scale
cat /sys/bus/iio/devices/iio:device0/in_anglvel_scale

# SPI 通信验证（需要 SPI 调试工具）
spidev_test -D /dev/spidev0.0 -p "\x75\x00\x00\x00"  # 读取 WHO_AM_I 寄存器

# Android Sensor HAL 层验证
dumpsys sensorservice
```

---

## 四、经验总结

1. **Axis Remap**：IMU 方向校正常见问题，需要在驱动层面做 axis remap
2. **SPI 速率**：ICM42607 支持最高 10MHz SPI，但长走线可能需要降低速率
3. **中断触发**：使用 LEVEL 中断比 EDGE 更可靠，避免数据丢失
4. **校准**：Sensor HAL 层还需要做温漂校准和零偏校准，驱动层面只做基本的轴映射
5. **坐标系**：Android 要求 sensorevent 遵循 ANDROID_SENSOR_COORDINATE_TRANSFORMATION 规范
