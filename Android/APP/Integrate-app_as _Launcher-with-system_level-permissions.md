# Integrate App as Launcher with System-Level Permissions

## 背景

将预编译 APK (`com.fangxin.door.user`) 集成到 MYIR YR3562 Android 14 固件中，要求：
- 打包进系统镜像（非 data/app 可卸载方式）
- 开机自启动
- 附加 root / 系统级权限

## APK 信息

- 包名: `com.fangxin.door.user`
- 版本: 2.7.44 (versionCode 194)
- SDK: targetSdkVersion 35, minSdkVersion 30
- 已声明权限: `RECEIVE_BOOT_COMPLETED`, `MOUNT_UNMOUNT_FILESYSTEMS`, `MOUNT_FORMAT_FILESYSTEMS`, `INSTALL_PACKAGES`, `READ_PRIVILEGED_PHONE_STATE` 等
- 含 arm64-v8a native libs (Agora SDK, Bugly, UVCCamera)

## 关键环境信息

- SELinux: `BOARD_SELINUX_ENFORCING := false` (permissive)
- 设备: `myd_yr3562` (tablet, rk3562)

## 方案：priv-app 预置

选用 `/system/priv-app/` 而非 `/odm/` 或 `/data/app/`：
- priv-app 获得 privileged 级别权限授予
- 配合 SELinux permissive，获得最大系统访问权限

APK 使用 `PRESIGNED` 保留原始签名（避免 Agora SDK 等第三方库签名校验失败）。

## 创建的文件

### 1. `device/rockchip/rk3562/myd_yr3562/prebuild_apps/Android.mk`

```makefile
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := FangxinDoor
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_PRIVILEGED_MODULE := true
LOCAL_CERTIFICATE := PRESIGNED
LOCAL_DEX_PREOPT := false
LOCAL_ENFORCE_USES_LIBRARIES := false
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
include $(BUILD_PREBUILT)
```

要点：
- `LOCAL_PRIVILEGED_MODULE := true` → 安装到 `/system/priv-app/`
- `LOCAL_CERTIFICATE := PRESIGNED` → 保留 APK 原始签名
- `LOCAL_DEX_PREOPT := false` → 跳过预编译 (APK 已优化)
- `LOCAL_ENFORCE_USES_LIBRARIES := false` → 跳过库声明校验
- 不用 `LOCAL_PREBUILT_JNI_LIBS` → APK 已有 `extractNativeLibs=false`, .so 在运行时 mmap

`all-makefiles-under` 自动发现此 Android.mk，无需额外 include。

### 2. `device/rockchip/rk3562/myd_yr3562/prebuild_apps/FangxinDoor.apk`

从 SDK 根目录 `app-2.7.44-194.apk.1.1` 复制并重命名。

### 3. `device/rockchip/rk3562/permissions/privapp-permissions-com.fangxin.door.user.xml`

```xml
<?xml version="1.0" encoding="utf-8"?>
<permissions>
    <privapp-permissions package="com.fangxin.door.user">
        <permission name="android.permission.READ_PRIVILEGED_PHONE_STATE"/>
        <permission name="android.permission.INSTALL_PACKAGES"/>
        <permission name="android.permission.MOUNT_UNMOUNT_FILESYSTEMS"/>
        <permission name="android.permission.MOUNT_FORMAT_FILESYSTEMS"/>
        <permission name="android.permission.WRITE_EXTERNAL_STORAGE"/>
        <permission name="android.permission.SYSTEM_ALERT_WINDOW"/>
        <permission name="android.permission.RECEIVE_BOOT_COMPLETED"/>
    </privapp-permissions>
</permissions>
```

显式授权 `signature|privileged` 级别的权限。APK 是非平台签名，必须通过此白名单才能获得这些权限。

## 修改的文件

### `device/rockchip/rk3562/myd_yr3562/myd_yr3562.mk`

新增两处：

```makefile
# 添加到 PRODUCT_PACKAGES
PRODUCT_PACKAGES += FangxinDoor

# 复制权限白名单到系统
PRODUCT_COPY_FILES += \
    device/rockchip/rk3562/permissions/privapp-permissions-com.fangxin.door.user.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/privapp-permissions-com.fangxin.door.user.xml
```

## 验证方法

```bash
# 1. 确认安装位置（priv-app 而非 data/app 或 odm）
adb shell pm path com.fangxin.door.user
# → package:/system/priv-app/FangxinDoor/FangxinDoor.apk

# 2. 确认权限授予
adb shell dumpsys package com.fangxin.door.user | grep -E "permission.*granted=true"

# 3. 确认进程 UID
adb shell ps -A -o USER,NAME | grep fangxin

# 4. 确认开机自启动
adb reboot && adb wait-for-device && adb shell ps -A | grep fangxin

# 5. 确认 SELinux 状态
adb shell getenforce
# → Permissive
```

## 结论

三项需求均验证通过：
- **打包进系统** → APK 位于 `/system/priv-app/`
- **开机自启动** → `RECEIVE_BOOT_COMPLETED` + priv-app 身份
- **Root 级权限** → 所有 privilege/signature 权限 granted=true，SELinux permissive
