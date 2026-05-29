# 高级工程师调试记录：开机动画与 BootVideo 定制

## 功能概述
开机动画是 Android 设备启动过程中的视觉呈现，直接影响用户的第一印象。在 RK3576 Android 14 平台中，开机动画的定制涉及 `bootanimation.zip` 资源管理、`bootanimation.ts`（boot video）格式转换、动画启动速度优化以及 SEAndroid 权限配置。调试需平衡视觉效果与启动时间。

## 调试方法

### 1. 开机动画文件检查
```bash
# 查看动画资源文件
ls -l device/rockchip/common/bootanimation.zip
ls -l device/rockchip/common/bootshutdown/bootanimation.zip

# 查看 bootvideo 文件
ls -l device/rockchip/common/bootvideo/bootanimation.ts
```

### 2. bootanimation.zip 格式检查
bootanimation.zip 是标准的 zip 压缩包，包含：
```
bootanimation.zip
├── desc.txt          # 动画描述文件
├── part0/            # 第一部分（通常为循环）
│   ├── 000.png
│   ├── 001.png
│   └── ...
└── part1/            # 第二部分（通常为收尾）
    ├── 000.png
    └── ...
```

`desc.txt` 格式示例：
```
800 1280 24   # 宽 高 帧率
p 1 0 part0   # 循环播放 part0（1次，0 表示循环）
p 0 0 part1   # 播放 part1（0次，显示最后一帧）
```

### 3. bootvideo.ts 调试
```bash
# 检查 bootvideo 文件大小
ls -lh device/rockchip/common/bootvideo/bootanimation.ts

# 查看 bootvideo 相关日志
dmesg | grep -i bootvideo
dmesg | grep -i bootanim

# 在 target 上查看实际使用的文件
ls -l /system/media/bootanimation.ts
ls -l /system/media/bootanimation.zip
```

### 4. SELinux 权限检查
```bash
# 查看 bootanim 的 SELinux 上下文
adb shell ls -Z /system/media/bootanimation.ts

# 查看 bootanim.te 策略（commit fc0dd04003e）
cat device/rockchip/common/sepolicy/vendor/bootanim.te
```

## 常见问题及解决方案

### 1. 开机动画不显示或黑屏
**可能原因：**
- 动画文件缺失或损坏
- 文件格式不正确（bootanimation.zip vs bootanimation.ts）
- SELinux 权限不足

**调试步骤：**
1. **确认文件存在**：
   ```bash
   adb shell ls -l /system/media/bootanimation*
   ```

2. **检查 SELinux 权限**（commit `fc0dd04003e`）：
   ```te
   # bootanim.te
   allow bootanim vendor_file:file read;
   allow bootanim vendor_file:dir search;
   ```

3. **确认 bootanim 服务运行正常**：
   ```bash
   adb shell ps -A | grep bootanim
   adb shell dumpsys graphics | grep bootanim
   ```

### 2. 开机动画卡顿或掉帧
**问题：** 动画播放不流畅，有明显卡顿。

**可能原因：**
- 动画帧率过高
- 图片分辨率过大
- 启动过程中 CPU 负载高

**解决方案：**
1. **优化 desc.txt**：降低帧率（如 24fps → 15fps）
2. **减小图片分辨率**：匹配屏幕实际分辨率
3. **压缩图片质量**：使用 PNG 量化工具减小文件体积

### 3. bootanimation.zip 文件过大（commit `13aa228b5b8`）
**问题：** bootanimation.zip 从 976KB 增长到 3.3MB，影响启动速度和存储空间。

**调试方法：**
```bash
# 查看 zip 内部文件
unzip -l device/rockchip/common/bootshutdown/bootanimation.zip

# 检查未使用的资源
# 移除不必要的帧或降低帧数
```

### 4. bootanimation.ts 与 bootanimation.zip 共存冲突（commit `fc0dd04003e`, `b837d199aca`）
**问题：** 同时存在 ts 和 zip 格式的动画文件，系统优先选择哪个？

**解决思路：**
RK3576 平台支持两种格式：
- `bootanimation.zip`：传统格式，由 `bootanim` 服务解析
- `bootanimation.ts`：boot video 格式，在内核/早期阶段播放

**配置策略：**
```makefile
# device/rockchip/rk3576/device.mk
# 启用 bootvideo（优先使用 ts 格式）
PRODUCT_COPY_FILES += \
    device/rockchip/common/bootvideo/bootanimation.ts:system/media/bootanimation.ts

# BOARD_BOOTANIMATION 指向 zip 格式
BOARD_BOOTANIMATION := device/rockchip/common/bootshutdown/bootanimation.zip
```

### 5. 开机动画启动慢（commit `86676cb4876`）
**问题：** Android 系统启动后需要较长时间才开始播放动画。

**优化方法：**
1. **提前启动 bootanim 服务**：
   ```rc
   # init.rc 中提前启动
   on boot
       start bootanim
   ```

2. **减小文件体积**：减小 bootanimation.zip 大小以加快加载速度

3. **使用 bootanimation.ts**：在更早的阶段显示动画

## 调试案例

### 案例一：bootanimation.zip 大小优化（commit `13aa228b5b8`）
**背景：** bootanimation.zip 膨胀到 3.3MB，需要缩小到合理范围。

**优化措施：**
1. 减少动画帧数（删减冗余帧）
2. 压缩 PNG 图片（使用 pngquant 或 optipng）
3. 降低图片色深

**结果：** bootanimation.zip 从 3.3MB 减小到 976KB。

### 案例二：从 bootanimation.zip 切换为 bootanimation.ts（commit `fc0dd04003e`）
**背景：** 为获得更早的启动显示，将开机动画从传统的 bootanimation.zip 格式切换为 bootvideo ts 格式。

**修改内容：**
1. 用 `bootanimation.ts` 替换 `bootanimation.zip`
2. 更新 `device.mk` 中的文件拷贝配置
3. 添加 SELinux 权限规则
4. 更新 `BoardConfig.mk` 中的配置

**验证方法：**
```bash
# 确认系统启动过程中显示的动画是新的 ts 文件
# 观察启动阶段显示画面出现的时机（应该比之前更早）
```

### 案例三：bootanimation.ts 协作修改（commit `b837d199aca`）
**背景：** 客户（CRY）提供了自定义的 bootanimation.ts，需要集成到系统中。

**集成过程：**
1. 将客户提供的 `bootanimation.ts` 放入 `device/rockchip/common/`
2. 更新 `device.mk` 引用新文件
3. 验证启动动画正常显示

## 高级调试工具

### 1. 动画文件分析
```bash
# 检查 bootanimation.zip 内容
unzip -l bootanimation.zip

# 检查 desc.txt
unzip -p bootanimation.zip desc.txt

# 检查图片信息
identify bootanimation.zip/part0/*.png
```

### 2. 启动时间分析
```bash
# 查看 bootanim 启动时间戳
dmesg | grep -E "(bootanim|bootanimation)"

# 使用 bootchart 分析启动过程
adb shell 'echo 1 > /data/bootchart/enabled'
adb reboot
# 启动后收集 bootchart 数据
```

### 3. 动画渲染性能
```bash
# 查看 SurfaceFlinger 帧率
adb shell dumpsys SurfaceFlinger --latency
```

## 注意事项
1. **文件大小控制**：bootanimation.zip 建议控制在 2MB 以内，避免影响启动速度。
2. **分辨率匹配**：动画图片分辨率应与屏幕分辨率一致，避免缩放开销。
3. **格式选择**：bootanimation.ts 适合需要在 U-Boot 阶段显示的动画，bootanimation.zip 适合 Android 启动后的动画。
4. **SEAndroid 权限**：添加新文件或路径时需同步更新 SELinux 策略。
5. **冗余清理**：切换文件格式后及时清理旧的动画文件，避免占用存储空间。

---
*编写：高级工程师*
*最后更新：2026-04-24*
*基于 commit：13aa228b5b8, 86676cb4876, fc0dd04003e, b837d199aca*
