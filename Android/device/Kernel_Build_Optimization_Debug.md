# Kernel/Uboot 编译优化与配置裁剪调试笔记

## 概述

RK3576 项目中进行了多轮编译优化和配置裁剪，目的：减少内核体积、加快编译速度、降低启动时间、去除无用功能。

---

## 一、SDK 版本升级 (`95d5955cb4e`)

```bash
# update sdk to rkr7
```

将 SDK 从初始版本升级到 Rockchip RK Release 7 (rkr7)，涉及：
- kernel 基线更新
- ATF/OPTEE 版本同步
- hardware/rockchain 相关库更新
- 设备树 binding 同步

---

## 二、内核编译优化

### 2.1 全 CPU 线程编译 (`c5930ba18cd`)

```diff
# build.sh
- make -j$(nproc)
+ make -j$(($(nproc) * 2))
```

使用 CPU 逻辑线程数的 2 倍进行并行编译，充分利用超线程能力。在 RK3576 开发机上显著减少编译时间。

### 2.2 Kernel 配置裁剪 (`b9571b00c09`, `844135f009d`)

**b9571b00c09** (Trim unnecessary configurations in kernel)：

```makefile
# 裁剪的主要模块
# 移除未使用的文件系统和驱动
# CONFIG_EXT4_FS is needed (保留必要项)
# CONFIG_NFS_FS=n
# CONFIG_CIFS=n

# 移除不必要的 GPU 配置
# CONFIG_MALI_DEBUG=n

# 移除调试配置
# CONFIG_DEBUG_FS can be removed in production
```

**844135f009d** (Optimize kernel configuration)：

进一步裁剪：
```makefile
# 移除未使用的 WiFi 协议栈
# CONFIG_CFG80211 is set (if needed)
# CONFIG_MAC80211 is set

# 移除 USB gadget 的冗余配置
# CONFIG_USB_G_ANDROID=n

# 移除网络文件系统
# CONFIG_SMB_SERVER=n
# CONFIG_CIFS=n
```

### 2.3 PCIe 打印去除 (`e12e44085c9`)

PCIe 驱动初始化时大量 dev_info 打印会拖慢启动速度：

```c
// 将 dev_info 改为 dev_dbg 或移除
// drivers/pci/controller/dwc/pcie-dw-rockchip.c
- dev_info(dev, "PCIe link up, speed gen%d\n", speed);
+ dev_dbg(dev, "PCIe link up, speed gen%d\n", speed);
```

减少约 30+ 行内核 log 输出。

### 2.4 Uboot 裁剪 (`0b1fc4312db`)

```makefile
# Trim unnecessary configurations in U-Boot
# 移除不必要的 Uboot 命令
# CONFIG_CMD_USB=n (如果不用 Uboot USB)
# CONFIG_CMD_NET=n
# CONFIG_CMD_PCI=n
# CONFIG_BOOTDELAY=0 (缩短启动等待)
```

### 2.5 Uboot 存储检测去除 (`a8a2fc6e5fe`)

```c
// 移除 Uboot 阶段的存储设备自动检测
// 加快 Uboot 启动约 500ms ~ 1s
// board_init 中去掉不必要的设备扫描循环
```

---

## 三、功能禁用

### 3.1 4G 模块 (`f76636a5582`)

```makefile
# BoardConfig.mk
BOARD_SUPPORT_4G := false

# manifest.xml 中排除 4G 相关组件
# rk3576_u.mk 中不包含 4G RIL 和 HAL
```

修改涉及 3 个文件：
- `BoardConfig.mk` - 禁用 4G 编译标志
- `manifest.xml` - 从 manifest 中移除 RIL 和 modem 相关组件
- `rk3576_u.mk` - 不包含 4G 相关的包

```makefile
# rk3576_u.mk 排除项示例
PRODUCT_PACKAGES += \
    -RILService \
    -ModemService \
```

### 3.2 以太网和 PCIe WiFi (`c3381af8893`)

```makefile
# kernel config
# CONFIG_PCIE_WIFI is not set
# CONFIG_ETHERNET is not set
# CONFIG_STMMAC_ETH is not set
```

因为 PCIe 接口被 Sample Card 占用，所以禁用 PCIe WiFi。以太网在 RK3576 上默认不使能（板级设计无以太网 PHY）。

### 3.3 WiFi 驱动清理 (`bd619c84b66`, `b5add6656b5`)

```bash
# 删除未使用的 wifi_driver 外部模块
rm -rf external/wifi_driver
```

该目录中的驱动代码在项目中未使用且无法编译，直接删除减少维护负担。

---

## 四、LCD Density (`6ddb850fd3c`)

```diff
# rk3576_u.mk
- PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=180
+ PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=213
```

在 800x480 分辨率下：
- density=180：UI 元素偏小，触摸操作困难
- density=213：UI 元素更大，更适合触控操作

---

## 五、调试 RC 清理 (`91942a3aa9f`)

删除 `init.myir_debug.rc`，移除调试阶段的 init 配置：

```diff
- init.myir_debug.rc        # 删除调试 rc 文件
- import init.myir_debug.rc # 从 init.rk30board.rc 移除引用
```

```makefile
# BoardConfigVendor.mk
- MYIR_DEBUG := true
```

---

## 六、经验总结

1. **裁剪原则**：每次裁剪必须确认依赖关系，建议分步提交便于回退
2. **编译优化**：`-j$(nproc)*2` 在 SSD + 大内存机器上效果显著，HDD 上反而不利
3. **4G 禁用**：如果硬件上没有 4G 模块，manifest.xml 的组件排除可以减少约 200ms 的 system_server 启动时间
4. **LCD Density**：调大 density 会增加 GPU 渲染压力但提升触控体验，需要平衡
5. **调试 RC**：产品发布前务必移除调试阶段创建的 init.rc 文件和权限
