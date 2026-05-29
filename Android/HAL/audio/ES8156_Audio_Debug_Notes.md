# ES8156 音频调试笔记 (RK3576 + Android 14)

## 概述

ES8156 是 Everest Semiconductor 出品的音频 codec，通过 I2C 控制和 I2S 音频总线与 RK3576 连接。

---

## 一、驱动移植

### 1.1 Codec 驱动 (`df14b63c208`)

```c
&i2c3 {
    status = "okay";
    
    es8156: es8156@10 {
        compatible = "everest,es8156";
        reg = <0x10>;
        #sound-dai-cells = <0>;
    };
};
```

### 1.2 DTS 配置

Machine driver 层连接 codec 到 RK3576 I2S 控制器：

```c
sound {
    compatible = "simple-audio-card";
    simple-audio-card,name = "RK3576 ES8156";
    simple-audio-card,format = "i2s";
    simple-audio-card,mclk-fs = <256>;
    
    simple-audio-card,cpu {
        sound-dai = <&i2s8_8ch>;
    };
    
    simple-audio-card,codec {
        sound-dai = <&es8156>;
    };
};
```

---

## 二、TinyALSA HAL (`8f3f04ef8d2`)

### 2.1 实现

由于 audio HAL 基于 TinyALSA（而非 ALSA 框架），需配置 `audio_policy_configuration.xml`：

```xml
<audioPolicyConfiguration>
    <globalConfiguration speaker_drc_enabled="false"/>
    <modules>
        <module name="primary" halVersion="2.0">
            <attachedDevices>
                <item>Speaker</item>
            </attachedDevices>
            <devicePorts>
                <devicePort tagName="Speaker" type="AUDIO_DEVICE_OUT_SPEAKER" role="source">
                    <profile name="" format="AUDIO_FORMAT_PCM_16_BIT"
                             samplingRates="48000"
                             channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
                </devicePort>
            </devicePorts>
        </module>
    </modules>
</audioPolicyConfiguration>
```

### 2.2 CRYsound

CRY APK 集成了语音播报功能，音频路径经过 PDM 麦克风采集和 codec 播放：

```
PDM Mic → RK3576 PDM 控制器 → audio HAL → ES8156 Codec → Speaker
```

调试目录：
```makefile
# 创建 vendor debug 目录
mkdir /data/vendor/crysound
mkdir /data/vendor/debug
```

SELinux 权限：
```
/data/vendor/debug(/.*)?  u:object_r:vendor_debug_file:s0
/data/vendor/crysound(/.*)? u:object_r:crysound_file:s0
```

---

## 三、调试命令

```bash
# 查看音频设备
cat /proc/asound/cards

# 播放测试音频
tinyplay /vendor/etc/test.wav -D 0 -d 0

# 录音测试
tinymix -D 0 set "Main MIC" 1
tinycap /data/test.wav -D 0 -d 0 -r 48000 -b 16 -c 2

# 查看 mixer 控制
tinymix -D 0

# 检查 I2S 信号（调试用）
# 用示波器测量 I2S_SCLK、I2S_LRCK、I2S_SDOUT 引脚
```

---

## 四、经验总结

1. **I2S 时钟**：ES8156 需要精确的 MCLK（256× sample rate），RK3576 I2S 控制器需正确配置分频
2. **TinyALSA**：与标准 ALSA 不同，TinyALSA 使用 `audio_policy_configuration.xml` 定义路由而非 UCM
3. **PDM 麦克风**：`647c82bb22a` 添加了 PDM 麦克风驱动，注意 PDM clk 频率和 filtering 配置
4. **SELinux 权限**：CRY APK 访问 `/data/vendor/debug` 需要 `vendor_debug_file` 权限，`e9c1f38d20a` 修复了 dlopen 权限问题
5. **ALSA 与 TinyALSA 共存**：确认 `mediaprofiles.xml` 中 audio 配置指向正确的 HAL
