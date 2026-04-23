# Embedded Linux 图形栈与 Flutter / Qt 支持技术指导（增强版）

---

## 1. 背景说明

在嵌入式 Linux 平台（如 Rockchip RK 系列、NXP i.MX 系列）中，UI 框架（Flutter / Qt）虽属于应用层，但**强依赖底层图形栈（GPU/DRM/Wayland/EGL）**。

常见误区：
- ❌ 只要 `apt install` 即可运行
- ❌ UI 框架与硬件无关

实际：
- ✔ API 与硬件解耦
- ❗运行能力由“图形栈是否打通”决定

---

## 2. 图形系统分层（工程视角）

```
Application (Flutter / Qt)
        ↓
Engine (Skia / Qt SceneGraph)
        ↓
EGL / OpenGL ES / Vulkan
        ↓
Wayland / DRM(KMS) / FB
        ↓
Kernel DRM/KMS + VOP/LCDC
        ↓
GPU Driver (Mali / Vivante)
        ↓
Hardware (SoC + Display)
```

---

## 3. 关键结论

### 3.1 安装成功 ≠ 可运行

`apt install` 仅代表依赖满足，不代表：
- GPU 可用
- EGL 初始化成功
- UI 可显示

---

### 3.2 Flutter vs Qt（能力对比）

| 项目 | Qt | Flutter |
|------|----|--------|
| 渲染后端 | OpenGL / 软件渲染 | Skia + GPU |
| GPU 依赖 | 可选 | 强依赖 |
| 无 GPU 支持 | ✔（linuxfb） | ❌ |
| 显示后端 | eglfs / wayland / x11 / fb | wayland / drm |
| 嵌入式成熟度 | ⭐⭐⭐⭐⭐ | ⭐⭐ |

---

### 3.3 Flutter 运行硬性条件

必须全部满足：
- OpenGL ES 2.0+
- EGL 正常
- DRM/KMS 可用
- Wayland（推荐）

---

## 4. 平台能力评估维度（FAE标准）

评估一个 SoC 是否支持 Flutter / Qt：

### 4.1 CPU 架构
- ARMv7 / ARMv8
- 是否支持 NEON

### 4.2 GPU 能力
- 是否存在 GPU（Mali / Vivante）
- OpenGL ES 版本（2.0 / 3.0）

### 4.3 显示子系统
- DRM/KMS 是否支持
- VOP / LCDC 能力

### 4.4 显示接口
- RGB
- LVDS（单/双通道）
- MIPI-DSI
- HDMI / eDP

### 4.5 软件栈支持
- Wayland（weston）
- X11
- Framebuffer

---

## 5. 平台特性详细对比

### 5.1 RK3506（低成本控制类）

**定位**：MCU替代 / 工控

**CPU**：Cortex-A7

**GPU**：
- ❌ 无完整3D GPU
- ❌ 无标准 OpenGL ES

**显示子系统**：
- ✔ 基本 LCD 控制器
- ❌ DRM/KMS 能力弱

**显示接口**：
- RGB
- SPI

**图形栈支持**：
- ✔ Framebuffer
- ❌ Wayland（基本不可用）
- ❌ OpenGL

**框架支持结论**：
- Qt：✔（linuxfb）
- Flutter：❌
- LVGL：✔（强烈推荐）

---

### 5.2 RK3562（中端 Linux SoC）

**定位**：工业 HMI / 网关 / 多媒体

**CPU**：Cortex-A53

**GPU**：
- Mali GPU
- ✔ OpenGL ES 2.0/3.0

**显示子系统**：
- ✔ DRM/KMS
- ✔ VOP

**显示接口**：
- MIPI-DSI
- LVDS（部分型号）
- HDMI

**图形栈支持**：
- ✔ Wayland（weston）
- ✔ DRM
- ✔ OpenGL ES

**框架支持结论**：
- Qt：✔（eglfs / wayland）
- Flutter：✔（推荐）

---

### 5.3 i.MX93（低功耗AI平台）

**定位**：AI + 低功耗工业

**CPU**：Cortex-A55

**GPU**：
- Vivante（部分SKU弱化）
- ⚠ OpenGL ES 支持需确认

**显示子系统**：
- ✔ LCDIF
- ✔ DRM（有限）

**显示接口**：
- MIPI-DSI
- RGB

**图形栈支持**：
- ✔ Wayland（支持但性能有限）
- ⚠ GPU能力偏弱

**框架支持结论**：
- Qt：✔
- Flutter：⚠（需验证）

---

## 6. 推荐软件架构

### 6.1 RK3562 / 高端方案

- Kernel: DRM/KMS
- GPU: Mali Driver
- Display: Wayland (weston)
- UI: Flutter / Qt

---

### 6.2 RK3506 / 低端方案

- Kernel: FB
- UI: Qt (linuxfb) / LVGL

---

## 7. 开发方式建议

不推荐：
- 直接 apt 安装

推荐：
- BSP SDK
- Yocto
- 交叉编译

---

## 8. FAE 问诊三板斧

1. UI 是否需要动画/高帧率？
2. 分辨率？（720p / 1080p / 更高）
3. 是否必须 Flutter？

---

# 附录 A：案例分析（RK3506 vs RK3562 / i.MX93）

## A.1 项目目标

- 低成本版本：RK3506
- 高性能版本：RK3562 / i.MX93
- 计划量产：100 台
- 自研载板
- Linux + Flutter UI

---

## A.2 技术可行性

### RK3506

- 无 GPU
- 无 OpenGL
- 无 Wayland

👉 结论：
- ❌ Flutter 不可行
- ✔ Qt / LVGL

---

### RK3562

- Mali GPU
- OpenGL ES
- Wayland

👉 结论：
- ✔ Flutter 推荐平台

---

### i.MX93

👉 风险点：
- GPU能力不稳定
- Flutter需验证

---

## A.3 推荐产品策略

| 版本 | SoC | UI方案 |
|------|-----|--------|
| 低成本 | RK3506 | Qt / LVGL |
| 高端 | RK3562 | Flutter |

---

## A.4 客户资料需求拆解

### 硬件
- SOM Pinout
- 原理图
- 电源设计

### 软件
- BSP
- GPU Driver
- Wayland

### UI
- Demo
- 性能测试（glmark2）

---

## A.5 关键风险

- RK3506 + Flutter = 不成立
- i.MX93 Flutter = 不确定

---

## A.6 最终结论

建议采用分平台 UI 架构：

- 低端：轻量 UI（Qt/LVGL）
- 高端：Flutter

避免统一 UI 技术路线。

---

