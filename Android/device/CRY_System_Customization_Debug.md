# CRY 系统定制调试笔记 (RK3576 + Android 14)

## 概述

CRY 品牌系统定制涉及 Android framework、HAL、SELinux、系统应用等多层面的修改，最终交付 H4.0S0.1.6 系统版本。

---

## 一、系统 UI 定制

### 1.1 SystemUI 修改

**隐藏状态栏、导航栏、摄像头绿点、音量弹窗** (`bdc93538cf8`, `043f7985b0a`)：

```java
// frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/
// 通过 overlay 或运行时移除

// 隐藏状态栏
mStatusBarWindowController.setForceHidden(true);

// 隐藏导航栏
mNavigationBarController.getView().setVisibility(View.GONE);

// 移除摄像头/麦克风隐私指示器
mPrivacyChip.setVisibility(View.GONE);
```

相关文件：
```
device/rockchip/common/overlay/CRY/
    frameworks/base/core/res/res/values/config.xml
```

### 1.2 锁屏移除 (`d91fa12b483`)

移除 Android 锁屏界面：

```xml
<!-- overlay/config.xml -->
<bool name="config_enableLockScreen">false</bool>
<!-- 或通过 SettingsProvider -->
<integer name="def_lockscreen_disabled">1</integer>
```

### 1.3 Launcher 替换 (`e6f6347df14`, `1371a4f7b7f`)

替换默认 Launcher 为 CRY 定制版本：

```makefile
PRODUCT_PACKAGES += \
    CRYLauncher \
    
# 移除原厂 Launcher
PRODUCT_PACKAGES += \
    -FallCakHome \
```

移除不需要的系统 wallpaper 和 app：
```makefile
PRODUCT_PACKAGES += \
    -WallpaperPicker \
```

### 1.4 输入法 (`2f6c8521e47`)

定制输入法预置：

```makefile
PRODUCT_PACKAGES += \
    CRYInputMethod \
    
# 允许未知来源安装
settings put secure install_non_market_apps 1
```

---

## 二、应用与权限管理

### 2.1 移除 Rockchip 预置应用 (`8e132557c71`)

```makefile
PRODUCT_PACKAGES += \
    -RkUpdateService \     # OTA 升级服务
    -RkExplorer \          # 文件管理器
    -RkMusic \
    -RkVideoPlayer \
```

### 2.2 CRY APK (`4c1403413d9`, `1fea77060b9`)

CRY 主应用程序安装在 `/data/vendor/`：

```makefile
# 安装路径
PRODUCT_COPY_FILES += \
    vendor/cry/apk/CRY.apk:$(TARGET_COPY_OUT_VENDOR)/app/CRY/CRY.apk
```

后续版本移到 `/data/vendor/~` 目录 (`ff33774a693`)：

```makefile
PRODUCT_COPY_FILES += \
    vendor/cry/apk/CRY.apk:$(TARGET_COPY_OUT_VENDOR)/~/CRY.apk
```

### 2.3 未知来源安装 (`02558277f73`)

```xml
<!-- overlay -->
<integer name="config_installNonMarketApps">1</integer>
```

### 2.4 CMD Service (`776cb2bfb1a`, `3c680d4991e`)

添加 CRY 命令服务，用于系统控制和调试：

```java
// CRY CMD Service
// 提供系统级命令接口
// 允许后台控制系统配置
```

---

## 三、SELinux 权限调试

### 3.1 /dev/dvrs_hw 权限 (`52b9b38430b`)

```te
# vendor_sepolicy
/dev/dvrs_hw u:object_r:dvrs_hw_device:s0

# CRY APK 权限
allow cry_app dvrs_hw_device:chr_file rw_file_perms;
```

### 3.2 dlopen 权限 (`e9c1f38d20a`)

CRY APK dlopen `/data/vendor/debug/` 下的 so 文件：

```te
# 允许 CRY APK dlopen vendor debug 目录
allow cry_app vendor_debug_file:file rx_file_perms;
allow cry_app vendor_debug_file:dir r_dir_perms;
```

### 3.3 init.rc 权限验证 (`d91fa12b483`)

```
# 修正 init.xx.rc 中的安全上下文件
chmod 0666 /sys/class/gpio/gpio23/value
chmod 0666 /sys/class/gpio/gpio22/value
```

SEAndroid 策略路径：
```
device/rockchip/rk3576/rk3576_u/sepolicy/
```

---

## 四、系统版本管理

### 4.1 版本号格式

```makefile
# buildinfo.sh
BUILD_DISPLAY_ID := CRY_H4.0S0.1.6
```

版本格式：`H{硬件版本}.{主版本}S{软件版本}.{次版本}.{补丁版本}`

| 提交 | 版本 |
|------|------|
| `c8e4311d174` | H1.0S0.1.0 |
| `370dd1f000d` | H1.0S0.1.1 |
| `d8861ef9044` | H4.0S0.1.3 |
| `e6eac9e4abd` | H4.0S0.1.6 |

### 4.2 NTP 服务器 (`c18c1fd095c`)

```xml
<!-- config.xml -->
<string name="config_ntpServer">ntp.aliyun.com</string>
```

---

## 五、Boot 动画

### 5.1 从 zip 到 ts (`fc0dd04003e`)

```makefile
# 使用 boot_video 替代 bootanimation.zip
PRODUCT_BOOT_ANIMATION := bootvideo
TARGET_BOOTANIMATION_NAME := 800x480
```

使用 `.ts` (MPEG-TS) 格式视频作为开机动画，比传统 zip 帧动画更流畅。

### 5.2 加速 (`86676cb4876`)

```makefile
# 调整 bootanimation 服务启动时机
on boot
    start bootanimation

# 提升动画帧率
export BOOTANIMATION_FPS 30
```

---

## 六、GPIO 与 Sysfs

```makefile
# init.rc
chmod 0666 /sys/class/gpio/gpio22/value
chmod 0666 /sys/class/gpio/gpio23/value
chmod 0666 /sys/class/gpio/gpio24/value
```

GPIO22/23/24 用于 CRY 系统控制和状态指示。

---

## 七、调试命令

```bash
# 查看 SystemUI 状态
dumpsys window policy

# 查看当前 Launcher
cmd shortcut get-default-launcher

# SELinux 权限调试
# 查看 SELinux 拒绝日志
logcat -b events | grep "avc: denied"
dmesg | grep avc

# 临时设置为 permissive 模式
setenforce 0
# 复现问题，收集日志
setenforce 1

# 生成 SELinux policy
# 从 denials 生成 .te 规则
audit2allow -i /proc/kmsg

# 查看系统版本
getprop ro.build.display.id

# GPIO 调试
cat /sys/class/gpio/gpio23/value
echo 1 > /sys/class/gpio/gpio23/value
```

---

## 八、经验总结

1. **Overlay 优先**：SystemUI 定制优先使用 overlay 机制，避免修改 framework 源码
2. **SELinux**：permissive 模式仅用于调试，发布时必须 enforcing + 完整策略
3. **APK 安装路径**：`/data/vendor/` 目录需在 init.rc 中 mkdir 并配置 SELinux context
4. **版本号**：兼顾开发和客户需要，版本号需包含硬件版本和软件版本信息
5. **Boot 动画**：MPEG-TS 格式比 zip 帧动画占用更少内存，适合低内存设备
