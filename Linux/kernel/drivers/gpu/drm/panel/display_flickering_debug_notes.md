# 轻微闪屏问题调试记录

## 问题描述
**现象**：RK3576平台MIPI DSI显示屏在运行阶段出现轻微闪屏现象，表现为间歇性、不规则的亮度变化或像素不稳定。

**影响阶段**：主要出现在显示稳定运行阶段，而非启动或关闭瞬间。

## 调试过程

### 1. 问题定位
- **时间点**：2025-09-11
- **提交**：`cd8dba472ad136c65` ([Fix] resolve display flickering issue)
- **修改文件**：
  1. `kernel-6.1/arch/arm64/boot/dts/rockchip/myz-rk3576-mipi-800-480.dtsi`
  2. `kernel-6.1/drivers/gpu/drm/panel/panel-simple.c`

### 2. 修改内容分析

#### 2.1 设备树文件修改（关键运行阶段修复）

| 参数 | 修改前 | 修改后 | 作用阶段 | 影响范围 |
|------|--------|--------|----------|----------|
| `hsync-len` | `<2>` | `<8>` | **运行阶段** | 每帧刷新 |
| `rockchip,lane-rate` | 注释 | `<400>` | **运行阶段** | 数据传输 |
| `reset-delay-ms` | `120` | `150` | 启动阶段 | 复位时序 |
| `prepare-delay-ms` | `120` | `200` | 启动阶段 | 准备时序 |
| `enable-delay-ms` | `120` | `200` | 启动阶段 | 使能时序 |

#### 2.2 面板初始化序列调整
- **Sleep Out命令位置**：从序列末尾移至开头
- **寄存器值重调**：
  - `CC`: `18` → `10`（时钟/电源配置）
  - `B0/B1`: 完全重写伽马校正表
  - `B1`: `39` → `21`（电源偏置电压）
  - `B2`: `87` → `07`（驱动能力）
  - `B5`: `47` → `4E`（时序/电压）
  - 删除冗余命令（多个 `FF 77 01 00 00 13` 和 `E8` 命令）

#### 2.3 驱动调试信息添加
```c
// 在 panel-simple.c 中添加调试打印
pr_info("panel-simple: disable delay %d ms\n", p->desc->delay.disable);
pr_info("panel-simple: unprepare delay %d ms\n", p->desc->delay.unprepare);
pr_info("panel-simple: prepare delay for %d ms\n", p->desc->delay.prepare);
pr_info("panel-simple: reset low period for %d ms\n", p->desc->delay.reset);
pr_info("panel-simple: delay.init for %d ms\n", p->desc->delay.init);
pr_info("panel-simple: enable delay %d ms\n", p->desc->delay.enable);
```

## 关键发现（基于官方文档验证）

### 1. 延迟参数的作用范围（内核源码定义）
```c
/**
 * @delay: Structure containing various delay values for this panel.
 * @prepare: the time that it takes for the panel to become ready
 * @enable: the time that it takes for the panel to display the first valid frame
 * @disable: the time that it takes for the panel to turn the display off
 * @unprepare: the time that it takes for the panel to power itself down completely
 * @reset: the time that it takes for the panel to reset itself completely
 * @init: the time that it takes for the panel to send init command sequence
 */
```
**结论**：延迟参数仅作用于**状态转换阶段**（启动、关闭、复位），对运行阶段的持续闪烁影响有限。

### 2. 运行阶段有效参数
| 参数                   | 官方文档依据                                                                  | 解决的根本问题                 |
| -------------------- | ----------------------------------------------------------------------- | ----------------------- |
| `hsync-len`          | 显示时序规范                                                                  | 行同步稳定性（过窄的同步脉冲导致行间时序漂移） |
| `rockchip,lane-rate` | `Documentation/devicetree/bindings/display/rockchip/rockchip,rk618.txt` | 数据传输速率匹配（默认速率可能不匹配面板需求） |
| 初始化寄存器值              | 面板规格书                                                                   | 伽马校正、电源偏置、驱动能力等显示质量参数   |

### 3. 运行阶段闪屏的根因分析
1. **时序不稳定**：`hsync-len=2` 过窄，导致行同步信号不可靠
2. **数据速率不匹配**：未明确设置 DSI 速率，使用默认值可能造成时钟-数据不同步
3. **显示参数不当**：伽马校正、电源偏置等寄存器值未优化，导致像素响应不均匀

## 解决方案有效性评估

### 1. 有效解决方案（针对运行阶段）
| 修改                           | 有效性  | 原理               |
| ---------------------------- | ---- | ---------------- |
| `hsync-len = <8>`            | ✅ 高  | 增加水平同步脉冲宽度，稳定行同步 |
| `rockchip,lane-rate = <400>` | ✅ 中高 | 明确数据速率，确保传输质量    |
| 寄存器值调整（B0/B1等）               | ✅ 中  | 优化显示参数，改善像素响应    |

### 2. 辅助解决方案（针对启动阶段）
| 修改 | 有效性 | 原理 |
|------|--------|------|
| 延迟参数增加 | ⚠️ 低（对运行阶段） | 确保启动时电源和信号稳定 |
| Sleep Out命令调整 | ⚠️ 低 | 优化启动流程顺序 |

### 3. 调试/冗余修改
| 修改 | 建议 |
|------|------|
| `pr_info`调试打印 | ❌ 生产代码应移除 |
| 部分冗余命令删除 | ⚠️ 可保留，但影响较小 |

## 验证方法建议

### 1. 精简测试方案
```bash
# 测试1：还原延迟参数（验证对运行阶段影响）
sed -i 's/reset-delay-ms = <150>/reset-delay-ms = <120>/' myz-rk3576-mipi-800-480.dtsi
sed -i 's/prepare-delay-ms = <200>/prepare-delay-ms = <120>/' myz-rk3576-mipi-800-480.dtsi
sed -i 's/enable-delay-ms = <200>/enable-delay-ms = <120>/' myz-rk3576-mipi-800-480.dtsi

# 测试2：还原hsync-len（预期重现闪屏）
sed -i 's/hsync-len = <8>/hsync-len = <2>/' myz-rk3576-mipi-800-480.dtsi

# 测试3：注释lane-rate（预期可能出现闪烁）
sed -i 's/rockchip,lane-rate = <400>;/\/\/ rockchip,lane-rate = <400>;/' myz-rk3576-mipi-800-480.dtsi
```

### 2. 监控手段
```bash
# 查看内核日志中的调试信息
dmesg | grep "panel-simple"

# 监控显示相关错误
dmesg | grep -i "dsi\|mipi\|panel\|display"
```

## 经验总结

### 1. 调试原则
1. **区分阶段**：明确问题是启动阶段还是运行阶段
2. **官方文档优先**：参考内核源码注释和设备树绑定文档
3. **最小化修改**：每次只修改一个参数，观察效果

### 2. 针对运行阶段闪屏的检查清单
- [ ] `hsync-len`、`vsync-len` 是否足够宽（通常≥4-8）
- [ ] `rockchip,lane-rate` 是否明确设置且匹配面板规格
- [ ] 面板初始化序列中的关键寄存器（伽马、电源、偏置）是否优化
- [ ] 显示时序参数（porch值）是否合理

### 3. 针对启动阶段闪屏的检查清单
- [ ] 各阶段延迟参数是否满足面板规格要求
- [ ] 电源时序是否正确（enable/reset信号顺序）
- [ ] 初始化命令序列是否完整且顺序正确

### 4. 代码维护建议
1. **移除生产调试代码**：`pr_info`等调试打印应在验证后移除
2. **注释明确**：关键参数修改应添加注释说明原因
3. **版本控制**：保留测试记录和验证结果

## 附录：相关文档
1. **内核源码**：`kernel-6.1/drivers/gpu/drm/panel/panel-simple.c` (delay结构定义)
2. **设备树绑定**：`Documentation/devicetree/bindings/display/rockchip/rockchip,rk618.txt`
3. **显示时序规范**：`Documentation/devicetree/bindings/display/panel/panel-common.yaml`

---
**记录时间**：2026-04-21  
**调试人员**：joeshua  
**平台**：RK3576 Android 14  
**内核版本**：6.1  
**文件位置**：`kernel-6.1/drivers/gpu/drm/panel/display_flickering_debug_notes.md`