Demo App里面，选择/指定不同的屏幕（DRM框架篇）


**
### 零、 不同图形栈下的行为

#### 1️⃣ DRM/KMS 直写（无 X11 / Wayland）

如果你用：

- DRM dumb buffer
    
- GBM + DRM
    
- kmstest / modetest
    

那你是 **明确指定 connector id 的**，例如：

drmModeGetConnector(fd, connector_id);

这种方式是app完全可以直接id选屏的。

---

#### 2️⃣ X11 环境

在 X11 下：

用 `xrandr`：

xrandr --output HDMI-1 --auto  
xrandr --output DP-1 --auto

应用层不能直接指定 panel id，  
而是 X server 决定，应用只渲染到 root window。

---

#### 3️⃣ Wayland

Wayland 里：

- App 不能直接选物理显示
    
- compositor 决定输出分配
    

比如 Weston 可通过配置文件指定输出。

## 

---

📝 RK3576 DRM 应用开发：显示 ID 绑定技术指南

### 一、 核心概念：DRM 资源标识符 (ID)

在 Linux DRM (Direct Rendering Manager) 框架中，每一个显示组件都被抽象为一个对象，并分配一个唯一的 Object ID。
```
运行以下命令确认：
modetest
```
![](Pasted%20image%2020260305141001.png)

1. Connector (连接器)：物理接口的抽象（如 HDMI-A-1, DSI-1）。
- ID 含义：系统内核为该接口分配的“身份证号”。
- 唯一性：在特定固件版本下，ID 通常是固定的（例如 HDMI=185，DSI=201）。

2. CRTC (显示控制器)：VOP 的硬件通道。负责将内存中的像素数据扫描并输出。

3. Encoder (编码器)：负责将 CRTC 的数字信号转换为 Connector 协议要求的信号。

---

### 二、 为什么 Demo 程序需要“硬编码” ID？

在 main.cpp 中看到 const uint32_t hdmi_conn_id = 185;，这是因为嵌入式应用通常追求启动速度和确定性。

- 选屏逻辑：由于 RK3576 支持多路输出（HDMI、MIPI、DP、E-Ink 等），App 必须明确告知内核：“我要把数据发往哪一个物理插座”。
    
- Demo 局限：开发者为了代码简洁，省去了“自动扫描并匹配可用屏幕”的逻辑，直接通过 ID 指定了目标。
    



---

### 三、 技术实操：如何确认与修改 ID

#### 1. 确认当前系统 ID 状态

在板卡终端执行以下命令，识别当前连接的显示设备 ID：


modetest | grep connect  
  

输出示例分析：

- 第一列数字（如 185）即为 Connector ID。
    
- 状态为 connected 表示该物理接口已接屏且被驱动识别。
    
- 名字（如 HDMI-A-1 或 DSI-1）决定了你的物理链路。
    

#### 2. App 层的“选屏”修改

如果你需要从 HDMI 切换到 MIPI 显示，必须进行以下同步修改：

- 步骤 A：在源码中搜索 185，并将其替换为 modetest 查到的 MIPI ID（如 201）。
    
- 步骤 B：重新编译工程，生成新的二进制文件。
    
- 原理：修改 ID 后，App 调用 drmModeGetConnector 时会指向 MIPI 接口，从而完成显示路由的切换。
    



---

### 四、 进阶：从“手动改代码”到“动态选屏”

为了使技术指导更具扩展性，建议将硬编码 ID 改为动态获取或配置化。

#### 方案对比表

|   |   |   |   |
|---|---|---|---|
|方案|实现方式|优点|缺点|
|硬编码 (Demo现状)|id = 185|逻辑最简单，启动最快|更换屏幕必须改代码重新编译|
|命令行传参|./app --id 201|灵活，无需重新编译|每次启动需要手动输入或写脚本|
|自动枚举|调用 drmModeGetResources|智能化，自动点亮已连接屏幕|代码量增加，需处理多屏冲突逻辑|


---

### 五、 常见避坑指南

1. ID 漂移：虽然同一版固件 ID 相对固定，但如果内核更新或修改了设备树（DTS）中的显示节点顺序，modetest 查到的 ID 可能会发生变化。
    
2. 分辨率匹配：手动指定 ID 后，App 还需要获取该 ID 支持的 Mode（分辨率）。如果 185 号（HDMI）是 1080P，而 201 号（MIPI）是 720P，代码中如果还强行设置 1080P，会导致显示异常。
    
3. 资源占用：如果系统已有其他程序（如 UI 桌面）占用了 185 号 ID，你的 Demo 可能会因为请求不到 DRM Master 权限而初始化失败。
    

---



