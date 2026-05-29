# Camera3 HAL / RKISP 调试笔记 (RK3576 + Android 14)

## 概述

RK3576 Camera HAL 基于 Rockchip 私有的 Camera3 HAL 实现，对接 RKISP（Rockchip Image Signal Processor）。调试涉及 HAL profile 配置、ISP 参数调优、AF 配合等。

---

## 一、HAL 架构

### 1.1 层次

```
Camera Application (Camera2 API)
    |
Android Camera Framework (camera3 HAL interface)
    |
Rockchip Camera HAL (libcamera3_rkisp.so)
    |
    ├── RKISP Driver (v4l2)
    │       └── GC05A2 Sensor
    │
    └── 3A Library (RkAiq)
            └── IQ File (gc05a2_KYT-11210-V2_default.json)
```

### 1.2 Camera3 Profile

`camera3_profiles_rk3576.xml` 是 Camera HAL 的核心配置文件，定义 sensor 能力、支持的分辨率、帧率、AF 等。

```xml
<Profiles>
    <Camera id="0">
        <Sensor>
            <SensorName>gc05a2</SensorName>
            <V4l2Device>/dev/video0</V4l2Device>
        </Sensor>
        <SupportAutoFocus>true</SupportAutoFocus>
        <StreamConfigurations>
            <width>2592</width> <height>1944</height> <framerate>25</framerate>
            <width>1280</width> <height>720</height>  <framerate>25</framerate>
        </StreamConfigurations>
    </Camera>
</Profiles>
```

---

## 二、Key 调试点

### 2.1 AF 配置

```xml
<VCM>
    <I2cBus>4</I2cBus>
    <I2cAddr>0x18</I2cAddr>
    <VcmType>dw9714</VcmType>
</VCM>
```

### 2.2 ISP 参数 `fee33ebc2b3`

ISP 3A 参数锁定时修改两个文件：
1. `camera3_profiles_rk3576.xml` - profile 层
2. `gc05a2_KYT-11210-V2_default.json` - IQ 算法参数

### 2.3 AF 防抖优化 `663119ff507`

```xml
<!-- camera3_profiles.xml 中 AF 相关 -->
<AFSupport>
    <mode>CAF</mode>
    <AFType>HW</AFType>
</AFSupport>
```

---

## 三、调试命令

```bash
# Camera HAL 日志
logcat -s CameraService camerahal:V

# 查看 camera HAL 版本
dumpsys media.camera

# 查看 ISP 状态
cat /sys/kernel/debug/rkisp/isp0/stats

# 测试 preview
gst-launch-1.0 rkcamsrc device=/dev/video0 io-mode=4 ! videoconvert ! waylandsink

# 测试拍照
gst-launch-1.0 rkcamsrc device=/dev/video0 num-buffers=1 ! jpegenc ! filesink location=/tmp/test.jpg
```

---

## 四、经验总结

1. **Profile 匹配**：camera3_profiles.xml 中 sensor 名称必须与驱动中注册的名称完全一致
2. **IQ 文件路径**：通过 `camera_etc.mk` 动态拷贝，确认路径映射关系和符号链接
3. **AF 生效条件**：HAL 层需要 `SupportAutoFocus=true` + VCM 配置 + BoardConfig `CAMERA_SUPPORT_AUTOFOCUS=true`
4. **ISP 资源**：RK3576 ISP 支持多路流但 buffer 有限，高分辨率预览 + 拍照可能遇到 buffer 不足
