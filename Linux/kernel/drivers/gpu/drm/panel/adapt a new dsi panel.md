——这是一份以高级 BSP/驱动工程师视角编写的 RK356X MIPI DSI 屏幕 Bring-up 与 Debugging 指南。去除了基础的科普，直击底层架构、信号完整性（SI）和 DRM 驱动框架的核心逻辑。


---

# RK356x Platform MIPI-DSI Panel Bring-up & Debugging Guide

**Document Level:** Advanced / Senior BSP Engineer

**Target Subsystem:** Linux 6.1(DRM/KMS), Rockchip VOP2, Synopsis DWC MIPI DSI

**Hardware Context:** Custom FPC adapter, impedance uncontrolled (High SI Risk)

---

## 1. Hardware & Signal Integrity (SI) Audit

在 MIPI DSI (D-PHY) 这种动辄 Gbps 级别的高速差分总线上，硬件的物理层容错率极低。当前非标转接板方案必须进行严格的风险管控。

### 1.1 关键硬件指标确认

- **差分阻抗 (Differential Impedance):** D-PHY 严格要求 **100Ω ±10%** 的差分阻抗匹配。无包地、不等长的走线会导致严重的阻抗突变，进而引发信号反射、串扰，最终表现为眼图（Eye Diagram）闭合，接收端 IC 无法解串。
    
- **时序偏斜 (Intra-pair / Inter-pair Skew):** 差分对内等长误差需控制在 **< 5mil**，对间（Data to Clock）控制在 **< 10mil**。手工飞线或劣质转接板极易打破此平衡。
    
- **电源完整性 (PI) 与时序:** * 屏端驱动 IC (如 ILI9881, JD9365) 对上电时序（Power-on Sequence）极其敏感。必须用示波器抓取 `VCI (3.3V)` -> `IOVCC (1.8V)` -> `Reset` 的时序跳变，确保严格符合 Datasheet 规范，否则 IC 将锁死在硬件复位状态。
    
    - **背光 (VLED):** 确认底板 Boost 拓扑输出能力。$800\times1280$ 屏幕通常需要串并联多颗 LED，确认所需驱动电压 ( **5V~25V**)，电流 **~80mA**。需测量带载下的实际输出到背板的电压。
        

---

## 2. DRM Bridge Topology & DTS Configuration

Rockchip 平台的 DRM 拓扑为：`VOP2 (CRTC) -> DSI Host (Encoder) -> Panel (Connector)`。配置必须确保整条 Pipeline 的 **Format** 和 **Timing** 严密对齐。

### 2.1 像素时钟与带宽计算 (Pixel Clock & Bandwidth)

不要盲目照搬旧屏参数。必须根据 $800\times1280$ Datasheet 重算 `clock-frequency` ($PCLK$)：

`PCLK = (Hactive + HBP + HFP + HSYNC) * (Vactive + VBP + VFP + VSYNC) * FPS`

_典型值：若要求 60fps，$PCLK$ 通常落在 **65MHz - 75MHz** 区间。_

### 2.2 Panel 节点配置标准模板

在 `myd-yr3562-mipi101c.dtsi` 中，重构 `panel@0` 节点：

DTS
详见./attachment/myd-yr3562-mipi101c.dtsi![](myd-yr3562-mipi101c.dtsi)


```
&dsi {
    status = "okay";
    /* 针对劣质线材的降级策略：手动压低 PHY 速率以换取 SI 余量 */
    rockchip,lane-rate = <400>; 

    dsi_panel: panel@0 {
        status = "okay";
        compatible = "simple-panel-dsi";
        reg = <0>;
        backlight = <&backlight>;
        
        /* 硬件引脚映射 (必填项) */
        reset-gpios = <&gpioX RK_PX GPIO_ACTIVE_LOW>;
        // power-supply = <&vcc_lcd_regulator>; 

        /* DSI 协议层参数 */
        dsi,flags = <(MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
                      MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET)>;
        dsi,format = <MIPI_DSI_FMT_RGB888>;
        dsi,lanes = <4>;
        bpc = <8>;
        bus-format = <0x1017>; /* MEDIA_BUS_FMT_RGB888_1X24 */

        /* 物理尺寸 (影响 DRM 侧的 DPI 计算) */
        width-mm = <135>;  /* 查阅新屏 Datasheet 替换 */
        height-mm = <216>; /* 查阅新屏 Datasheet 替换 */

        /* 屏幕原厂提供的 DCS 唤醒与初始化序列 (关键黑盒) */
        panel-init-sequence = [
            /* 此处必须替换为 MX-A101FM40 专属代码，例如：*/
            /* 39 00 04 B9 83 10 2E (Page Bank Set) */
            /* 15 00 02 11 00       (Sleep Out) */
            /* 15 00 02 29 00       (Display On) */
        ];

        disp_timings0: display-timings {
            native-mode = <&dsi_timing0>;
            dsi_timing0: timing0 {
                clock-frequency = <71000000>; /* 根据 PCLK 公式计算 */
                hactive = <800>;
                vactive = <1280>;
                /* 以下 Porch 参数需严格查阅 Datasheet 的 Timing 表 */
                hfront-porch = <...>;
                hsync-len = <...>;
                hback-porch = <...>;
                vfront-porch = <...>;
                vsync-len = <...>;
                vback-porch = <...>;
                hsync-active = <0>;
                vsync-active = <0>;
                de-active = <0>;
                pixelclk-active = <0>;
            };
        };
    };
};
```

### 2.3 driver
mipi和lvds都是用标准的simple驱动(注意Linux Mainline和Vendor BSP有不同)，
然后 不同产商特性（不同 MIPI IC）的寄存器初始化指令 ，drm框架的 在设备树加、fb框架要去simple驱动加（包括uboot的）；

| **特性 / 框架**            | **Legacy FB (Framebuffer)**                          | **Vendor DRM (如 Rockchip BSP)**                | **Mainline DRM (Linux 社区标准)**      |
| ---------------------- | ---------------------------------------------------- | ---------------------------------------------- | ---------------------------------- |
| **LVDS 适配**            | 在已有驱动内（详见LVDS笔记）添加硬编码或简单参数 DTS                       | `panel-simple` (DTS 配 Timing)                  | `panel-simple` (DTS 配 Timing)      |
| **MIPI 适配**            | \（同lvds）                                             | `panel-simple-dsi` (**DTS 填 init-sequence**)   | **专属 `.c` 驱动** (调用 `mipi_dsi` API) |
| **U-Boot 适配**(分阶段亮屏显示) | 修改 `display` 驱动源码（详见lvds笔记，只不过在uboot也都添加一段一样的寄存器初始化） | RK的u-boot可共用kernel解析能力 (解析 DTS 同步点亮)；全志的DRM未知； | 编写独立 Panel Driver 或简单 Timing       |
| **核心逻辑**               | 耦合度高，维护困难                                            | **配置化设计**，方便快速点屏                               | **解耦设计**，强调代码复用和规范                 |

#### 2.3.1关于 "Simple 驱动" 的范畴

- **LVDS：** 确实几乎都用 `panel-simple.c`。因为 LVDS 只是物理层转换，不涉及协议层的握手，只要有 PWM、GPIO 电平和 Timing 就能亮。
    
- **MIPI：** 虽然厂商叫它 `panel-simple-dsi`，但它本质上是一个 **“高度集成的通用驱动”**。它比 LVDS 的驱动多了一个核心功能：**DCS (Display Command Set) 解析引擎**。它通过读取 DTS 里的字节流，模拟了标准驱动里 `mipi_dsi_dcs_write` 的行为。
#### 2.3.2寄存器初始化位置的纠正

- **FB 框架：** 你说“要在 simple 驱动加”是对的，但更准确的说法是：**FB 框架缺乏统一的 Panel 管理层**。所以每个屏通常都是一个独立的 `.c` 驱动文件，或者是直接在 SoC 的 LCD 控制器驱动里硬编码初始化代码。
    
- **DRM 框架（BSP 版）：** 初始化指令确实加在 DTS 里，但原理是厂商在 `panel-simple-dsi.c` 里写了一个循环，去 `of_property_read_u8_array` 读取这些指令。

---

## 3. Backlight Subsystem & PWM Regulation (Software)

背光控制是显示适配中独立于 MIPI 链路的关键环节。在 RK356x 平台_+Linux6.x上，通常采用 `pwm-backlight` 驱动。

### 3.1 PWM 节点配置与频率对齐

- **频率匹配：** 不同的背光驱动芯片（如屏端的 Driver IC 或主板上的 Boost IC）对 PWM 输入频率有特定要求。通常建议设置为 **20kHz - 50kHz**。若频率过低（如 <1kHz），肉眼可能会察觉到频闪（Flicker）。
    
- **占空比极性：** 确认 `PWMS_PROP` 的极性。若发现系统亮度调至 255 反而全黑，说明需要切换 `PWM_POLARITY_INVERTED`。
    

DTS

```
&pwm3 {
    status = "okay";
    pinctrl-names = "active";
    pinctrl-0 = <&pwm3m0_pins>; /* 确保引脚复用不冲突 */
};

backlight: backlight {
    compatible = "pwm-backlight";
    /* 参数 2 表示周期，单位为纳秒 (ns)。40000ns = 25kHz */
    pwms = <&pwm3 0 40000 PWM_POLARITY_DEFAULT>; 
    
    /* 亮度分级曲线：建议使用对数曲线而非线性，以符合人眼感知 */
    brightness-levels = <0 10 25 50 100 150 200 255>;
    default-brightness-level = <6>; /* 对应 200 */
    
    /* 某些硬件需要使能引脚 */
    // enable-gpios = <&gpioX RK_PX GPIO_ACTIVE_HIGH>; 
};
```

### 3.2 常见问题排查 (Troubleshooting)（背光不稳模组严重亮灭闪屏）

- **现象：`dmesg` 提示 `backlight device not found`**
    
    - **排查：** 检查 `panel` 节点中是否正确引用了 `backlight = <&backlight>;`。若未引用，DRM 框架在点亮屏幕时不会触发背光使能。
        
- **现象：亮度调节失效或只有 0/1 两档**
    
    - **排查：** 检查 PWM 引脚复用（IOMUX）。确保该 GPIO 没有被其他功能（如 UART 或 SPI）占用。使用示波器测量 PWM 引脚，确认在调节 `/sys/class/backlight/` 时波形占空比是否有变化。
        
- **现象：背光有电但屏幕“全黑”**
    
    - **排查：** 区分“背光亮”与“显示黑”。在暗室观察屏幕边缘是否有漏光。如果有漏光但无图像，说明背光已工作，重点应回到 MIPI 初始化指令（Init Sequence）和时序（Timing）上。

---
## 4. Adjust to TP driver
### 4.1 add the TP IC driver
Get the driver rutine from the display manufacturer,
And then add into kernel;
### 4.2 update the firmware of the TP IC
To adjust the x-y accuracy, meanwhile don't do this at driver or app_structure(westom/qt) which will bring external load for the cpu.

## 5.Advanced Troubleshooting Protocol

### 5.1 Error: `-517` (EPROBE_DEFER)

- **Log:** `failed to find panel or bridge: -517`
    
- **Root Cause:** 驱动加载顺序引发的依赖缺失。DSI Host Probing 时，Panel driver（尚未就绪）或者 GPIO/Regulator（尚未分配）。
    
- **Action:** 仅需观察后续是否有 `bound ffb10000.dsi`。如果有，此 Log 为内核架构正常行为，无需干预。
	- 若无，排查 DTS 中的 `reset-gpios`（已判定为正常，因为分配的io序号对，而且默认凹字形为reset波形） 或 `power-supply`（已判断为正常，因为已验证不影响休眠唤醒） 节点是否写错导致 Panel probe fail。
-
    ![](Pasted%20image%2020260420144809.png)

### 5.2 Error: DSI Timeout / `-110`

- **Log:** `MIPI DSI transfer timeout` 或 `rockchip-mipi-dsi: fail to send command`
    
- **Root Cause:** 物理层握手失败。DSI Host 尝试从 LP (Low Power) 模式切换到 HS (High Speed) 模式发送数据时，屏端没有返回 ACK（如 BTA 机制超时）。
    
- **Action:** 1. 绝对的硬件问题或时序问题。首查 Reset 时序是否正确。
    
    2. 查验 FPC 线序是否接反。
    
    3. **Workaround:** 针对阻抗失控的转接板，通过在 DTS 限制 `rockchip,lane-rate` 强制降频（如降至 300~500Mbps），以此降低高频衰减，强行建立链路握手。
    

### 5.3 无报错，绑定成功，但依旧黑屏

- **Root Cause:** Pipeline 已通，但屏幕内部状态机异常。
    
- **Action:**
    
    1. **背光确认：** 使用手电筒高光侧照屏幕，检查是否隐约有画面（UI 或 Boot Logo）。若有，查 Backlight 驱动或电路。
        
    2. **DCS 指令校验：** DSI 初始化代码（`panel-init-sequence`）有误。IC 未能成功执行 `Sleep Out (0x11)` 和 `Display On (0x29)`，或者内部 Charge Pump 未被激活。**必须联系屏厂 FAE 索取对应的 Linux Init Payload。**
        

### 5.4 画面偏移、花屏、闪烁 (Tearing / Artifacts) (delay不当画面轻微波纹闪)

- **Root Cause:** VOP 输出的 Timing 与 Panel 内部 RAM 的刷新率（TE/VSYNC）未对齐，或者带宽溢出。
    
- **Action:**
    
    1. 校验 `hbp / hfp / vbp / vfp`。
        
    2. 调整 `dsi,flags`，如切换 `MIPI_DSI_MODE_VIDEO_BURST` 到 `MIPI_DSI_MODE_VIDEO_SYNC_PULSE` 模式进行对比测试。
        
    3. 若出现随机白噪点，通常为 MIPI 眼图恶化导致的误码（Bit Error），需重新设计 PCB 并做 100Ω 阻抗控制。
    
        

---

## 6. Next Step Actions (Priority List)

1. **获取固件级支持:** 向屏厂 FAE 索要 `MX-A101FM40` 搭配当前驱动 IC (如 JD9365 / ILI9881) 的 **DCS 寄存器初始化数组**。这是当前黑屏最致命的 Block point。
    
2. **硬件重构规划:** 若降频后仍无法建立通讯，证明当前手工板的插损/回损已击穿 D-PHY 协议底线。必须重新 Layout，打样包含差分阻抗控制的 FPC/PCB。
    
3. **参数对齐:** 根据 Datasheet 第 11 页重构 DTS 中的 Timing 节点。