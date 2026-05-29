# USB MTP 调试笔记 (RK3576 Android 14)

## 问题

RK3576 Android 14 设备连接到电脑后无法被识别为 MTP 文件传输设备，电脑不显示设备存储。

## 排查过程

### 1. 检查 USB Gadget 初始化

文件: `device/rockchip/common/rootdir/init.rk30board.usb.rc`

USB ConfigFS 目录树已完整创建，MTP/ADB/PTP 等 function 均已定义：
- `/config/usb_gadget/g1/functions/mtp.gs0`    -- MTP 传统 function
- `/config/usb_gadget/g1/functions/ffs.mtp`     -- MTP FunctionFS
- `/config/usb_gadget/g1/functions/ffs.adb`     -- ADB FunctionFS
- `/config/usb_gadget/g1/functions/ptp.gs1`     -- PTP function

vid: 0x2207 (Rockchip)
sys.usb.configfs: 2 (GKI 模式)
sys.usb.mtp.device_type: 3
sys.usb.mtp.batchcancel: true

### 2. 检查 USB 功能切换

文件: `system/core/rootdir/init.usb.configfs.rc`

支持 mtp / mtp,adb / ptp / rndis 等组合，切换逻辑正确。

### 3. 检查 USB 控制器

文件: `device/rockchip/rk3576/init.rk3576.rc`

```
sys.usb.controller = "23000000.usb"   -- rk3576 USB DRD 控制器
```

文件: `kernel-6.1/arch/arm64/boot/dts/rockchip/rk3576-evb.dtsi`

```dts
&usb_drd0_dwc3 { status = "okay"; };   // dr_mode = "otg"
```

### 4. 检查默认 USB 模式

文件: `device/rockchip/common/BoardConfig.mk:418`

```makefile
BOARD_USB_ALLOW_DEFAULT_MTP ?= false   # <-- 默认为 false
```

文件: `device/rockchip/common/device.mk:729-731`

```makefile
ifeq ($(BOARD_USB_ALLOW_DEFAULT_MTP), true)
    ro.usb.default_mtp=true
endif
```

rk3576 BoardConfig 未覆盖此值，所以 `ro.usb.default_mtp` 未设置。

### 5. 关键发现: `ro.usb.default_mtp` 是死代码

在 AOSP `frameworks/base/` 全量搜索 `ro.usb.default_mtp` / `default_mtp` / `defaultMtp`，
结果: **零引用**。整个 Android 14 framework Java 代码中没有读取这个属性的地方。

`BOARD_USB_ALLOW_DEFAULT_MTP` 和 `ro.usb.default_mtp` 只出现在 `device/rockchip/common/device.mk`
编译链中，设置了一个 framework 不读取的系统属性。这是 Rockchip 从旧版本遗留的配置，
在 Android 14 上已经失效。

### 6. 检查 USB Gadget HAL

文件: `hardware/rockchip/usb/gadget/UsbGadget.cpp`

- `setCurrentUsbFunctions()` 实现正确
- MTP 模式: vid/pid = 2207:0007
- MTP+ADB 模式: vid/pid = 2207:0017

### 7. 检查 Framework USB 默认逻辑

文件: `frameworks/base/services/usb/java/com/android/server/usb/UsbDeviceManager.java`

- `UsbHandlerLegacy` 构造函数读取 `persist.sys.usb.config` 作为默认 USB 功能
- 非正常启动时读取 `persist.sys.usb.<bootmode>.func`
- 正常启动时读取 `sys.usb.config`
- 没有读取 `ro.usb.default_mtp` 的代码

## 根因

Android USB 默认模式是"仅充电"。`ro.usb.default_mtp` 属性虽然在 Rockchip `device.mk` 中
可以配置，但在 Android 14 framework 中没有对应的读取代码，所以 `BOARD_USB_ALLOW_DEFAULT_MTP := true`
实际上不能改变默认 USB 模式。这是 Rockchip 遗留的无效配置。

## 解决方法

### 方法一: 通知栏切换（临时，每次都要操作）

1. USB 连接电脑
2. 下拉通知栏 → 点 "正在通过 USB 为此设备充电"
3. 选择 **"文件传输 (MTP)"**

### 方法二: 开发者选项设置（永久）

1. 设置 → 关于平板电脑 → 连点"版本号" 7 次，开启开发者模式
2. 设置 → 系统 → 开发者选项 → **默认 USB 配置**
3. 选择 **"文件传输 (MTP)"**

这个设置会写入 `Settings.Global`，由 `UsbDeviceManager` 在 USB 插入时读取。

### 方法三: 运行时强制切换（需要 root/adb root）

```sh
# 需要 root 权限，因为 sys.usb.config 受 SELinux 保护
adb root
adb shell 'echo mtp,adb > /config/usb_gadget/g1/configs/b.1/strings/0x409/configuration'
adb shell setprop sys.usb.config mtp,adb
```

## 附: 运行时修复失败原因

```sh
adb shell setprop sys.usb.config mtp,adb
  -> Failed to set property 'sys.usb.config'
```

`sys.usb.config` 不是普通系统属性，由 USB Gadget HAL 通过 ConfigFS 控制，
只有 USB 服务进程有权写入。adb shell 上下文无 SELinux 权限直接设置。

## 关键文件索引

| 文件 | 作用 |
|------|------|
| `device/rockchip/common/rootdir/init.rk30board.usb.rc` | USB ConfigFS 初始化 |
| `system/core/rootdir/init.usb.configfs.rc` | USB 功能切换（sys.usb.config 触发） |
| `device/rockchip/rk3576/init.rk3576.rc` | sys.usb.controller 定义 |
| `device/rockchip/common/BoardConfig.mk:418` | BOARD_USB_ALLOW_DEFAULT_MTP 默认值（无效） |
| `device/rockchip/common/device.mk:729-731` | ro.usb.default_mtp 设置（无效） |
| `device/rockchip/rk3576/BoardConfig.mk` | rk3576 平台 BoardConfig |
| `hardware/rockchip/usb/gadget/UsbGadget.cpp` | USB Gadget HAL 实现 |
| `frameworks/base/services/usb/java/com/android/server/usb/UsbDeviceManager.java` | Framework USB 管理（读 persist.sys.usb.config） |
| `kernel-6.1/arch/arm64/boot/dts/rockchip/rk3576-evb.dtsi` | USB 控制器 DTS 启用 |
