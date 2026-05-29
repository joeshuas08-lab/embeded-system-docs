# 系统杂项调试笔记 (RK3576 + Android 14)

## 汇总项目中不易归类的系统级调试

---

## 一、系统级配置精简

### 1.1 Kernel 精简 (`b9571b00c09`)

移除不必要的 kernel 配置模块，减少内核体积和启动时间：

```makefile
# rockchip_defconfig
# 移除未使用的驱动
# CONFIG_WLAN is not set
# CONFIG_GSENSOR_xxx is not set
```

### 1.2 Uboot 精简 (`0b1fc4312db`)

```makefile
# rk3576_defconfig
# 移除不需要的 Uboot 模块
# CONFIG_USB_STORAGE is not set
```

### 1.3 Uboot 存储设备检测去除 (`a8a2fc6e5fe`)

```c
// 跳过无用的存储设备检测，加快启动速度
// board_init 中去掉不必要的设备扫描
```

### 1.4 PCIe 打印去除 (`e12e44085c9`)

```c
// 减少 PCIe 初始化时的调试打印，加快内核启动速度
// 将 dev_info 改为 dev_dbg 或直接移除
```

### 1.5 Disabled VP2 (`a2fa7c1c2de`)

RK3576 的 Video Processor 2（VP2）在不需要双屏异显时关闭，节省内存带宽：

```c
&vop {
    status = "okay";
    // VP2 不使能
    vp2: vp2 {
        status = "disabled";
    };
};
```

---

## 二、Power 按键行为 (`205b4d5b142`)

### 2.1 按键配置

修改 Android 电源键响应行为：

```diff
# Generic.kl
- key 116   POWER             WAKE
+ key 116   POWER             WAKE_DROPPED
```

`WAKE_DROPPED` 在唤醒系统后丢弃该按键事件，避免同时触发其他动作。

### 2.2 12s 强制关机 (`1a6e28fe6d5`)

```diff
# frameworks/base/services/core/java/com/android/server/policy/PhoneWindowManager.java
- SHORT_PRESS_TIMEOUT = 500
+ SHORT_PRESS_TIMEOUT = 12000  // 12秒长按关机
```

---

## 三、PDM 麦克风 (`647c82bb22a`)

数字麦克风通过 PDM 接口连接，在 Android 中注册为音频输入设备：

```c
&pdm {
    status = "okay";
    pinctrl-0 = <&pdm_clk &pdm_clk1 &pdm_sdi0 &pdm_sdi1>;
};
```

使用 PDM 而非 I2S 麦克风的原因：
- 更高的抗干扰能力（数字传输）
- 更简单的 layout 布线（单线数据）

---

## 四、User Key (`ab0de5b32a3`)

添加自定义用户按键：

```c
&gpio_keys {
    status = "okay";
    
    user-key {
        label = "User Key";
        gpios = <&gpio0 RK_PA6 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_RESERVED>;
        wakeup-source;
    };
};
```

在 Generic.kl 中映射成特定功能：

```
key 246   USER_BUTTON      WAKE
```

---

## 五、Uboot/EFUSE/Baudrate (`bda597d917e`, `ffbd53ff509`)

### 5.1 Uboot Type-C GPIO 修复

`ffbd53ff509` 修复了 Uboot 阶段的 Type-C 检测 GPIO 配置错误，确保 Uboot 能正确识别 USB 插入。

### 5.2 串口波特率

`bda597d917e`：调试串口波特率从 1.5Mbps 修改为 115200：

```diff
- CONFIG_BAUDRATE=1500000
+ CONFIG_BAUDRATE=115200
```

降低波特率以提高通信稳定性，便于产线调试。

---

## 六、调试经验

1. **内核精简**：每次裁剪需确认依赖，避免 A/B 分区等隐形依赖导致启动异常
2. **Power Key**：`WAKE_DROPPED` 常见于行业设备（非手机产品），防止误触发
3. **VP2 禁用的前提**：确认系统只有一个显示输出需求，否则禁用 VP2 会导致第二个 display 无法工作
4. **PDM 布局**：PDM 时钟信号容易辐射干扰，layout 时需注意屏蔽
