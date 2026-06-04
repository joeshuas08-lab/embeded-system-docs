# BQ25895 JEITA WARM 區間充電電壓降低導致無法充滿除錯筆記

**日期:** 2026-05-28
**平台:** RK3576 (LR3576 底板)
**子系統:** 充電管理

---

## 硬體架構

| 元件 | 型號 | I2C 位址 | 功能 |
|------|------|----------|------|
| 充電 IC | BQ25895 | 0x6a (I2C6) | 實際控制充電電壓/電流 |
| 電量計 | BQ27546-G1 | 0x55 (I2C6) | 回報電量、溫度、健康狀態 |
| USB PD | HUSB311 | 0x4e (I2C6) | USB PD 協定溝通 |

**TS 腳位:** BQ25895 的 TS 腳位未接 NTC 熱敏電阻，底板用固定電阻模擬常溫。

---

## 問題現象

- 電池在約 **31°C（WARM 區間）**開始無法充滿，只能充到 **~94%**
- 硬體設定 43°C 為過溫保護（HOT），31°C 進入 WARM 區間
- 溫度資料來自 BQ27546 電量計 I2C，非充電 IC 的 TS 腳位

---

## 除錯過程

### 1. 確認溫度來源

```bash
# BQ25895 TS 腳位讀數（固定電阻模擬，不隨溫度變化）
cat /sys/class/power_supply/bq25890-charger/temp
# → 230 (23.0°C)

# BQ27546 電量計溫度（真實電池溫度）
cat /sys/class/power_supply/bq27546-0/temp
# → 363 (36.3°C)

# 確認 BQ27546 原始暫存器值（0.1K 單位）
i2cget -f -y 6 0x55 0x06   # 低 byte → 0x16
i2cget -f -y 6 0x55 0x07   # 高 byte → 0x0C
# 組合: 0x0C16 = 3094 × 0.1K = 309.4K - 273.1 = 36.3°C

# 確認 TS 腳位永遠讀常溫，硬體 JEITA 不會觸發
i2cget -f -y 6 0x6a 0x10   # TSPCT 暫存器
```

**結論：** BQ25895 硬體 JEITA 因 TS 腳位固定 23°C 不會觸發。問題在軟體層或暫存器配置。

### 2. 檢查 BQ25895 暫存器配置

```bash
i2cget -f -y 6 0x6a 0x09
# → 0x44 (0100_0100)
#    bit4 (JEITA_VSET) = 0 → JEITA 高溫時會降低充電電壓
#    bit6 (TMR2X_EN) = 1
```

對照 BQ25895 資料手冊:

| 暫存器 | 位址 | 位元 | 名稱 | 值 | 說明 |
|--------|------|------|------|----|------|
| REG09 | 0x09 | bit4 | JEITA_VSET | 0 (默認) | 0=JEITA時降壓, 1=JEITA時保持VREG |
| REG07 | 0x07 | bit0 | JEITA_ISET | - | 控制JEITA時是否降流 |

**關鍵發現：** REG09 bit4 (JEITA_VSET) = 0，表示 JEITA 高溫模式下會降低充電電壓。資料手冊說明 bit4=1 才能在高溫時保持正常充電電壓 VREG。

### 3. 確認目前充電電壓配置

```bash
cat /sys/class/power_supply/bq25890-charger/constant_charge_voltage_max
# → 4400000 (4.4V)

cat /sys/class/power_supply/bq27546-0/health
# → Dead (註：此為 BQ27546 flags 狀態，非實際電池健康)
```

DTS 中設定 VREG = 4.4V，但 WARM 區間 JEITA_VSET=0 會自動降壓約 200mV，實際只輸出 ~4.2V，導致電池只能充到 94%。

### 4. 排除其他可能

| 路徑 | 狀態 | 說明 |
|------|------|------|
| charger-manager | 未啟用 | DTS 中已註解 |
| BQ25895 硬體 TS 腳位 | 固定 23°C | 固定電阻，永不觸發 JEITA |
| power_supply sysfs | 健康狀態來自 BQ27546 | 暫存器 flags 回報 |

---

## 修改方案

### 檔案: `kernel-6.1/drivers/power/supply/bq25890_charger.c`

#### 1. hw_init() 中加入（行 869-877）

```c
/*
 * REG09 bit4 (JEITA_VSET): 1 = use VREG during JEITA warm, 0 = reduce voltage (default).
 * Set to 1 to keep full charge voltage up to hardware HOT threshold.
 */
ret = bq25890_field_write(bq, F_JEITA_VSET, 1);
if (ret < 0)
    dev_warn(bq->dev, "Failed to set JEITA_VSET: %d\n", ret);
```

#### 2. shutdown() 中加入（行 1546-1547）

```c
/* Keep JEITA voltage reduction disabled after shutdown */
regmap_field_write(bq->rmap_fields[F_JEITA_VSET], 1);
```

### 修改摘要

| 參數 | 暫存器 | 位元 | 修改前 | 修改後 | 效果 |
|------|--------|------|--------|--------|------|
| JEITA_VSET | REG09 (0x09) | bit4 | 0 | **1** | JEITA WARM 時不降壓，維持 VREG |
| JEITA_ISET | REG07 (0x07) | bit0 | 不動 | 不動 | 保持原樣 |

### 手動驗證命令

```bash
# 將 REG09 bit4 設為 1 (0x44 → 0x54)
i2cset -f -y 6 0x6a 0x09 0x54

# 確認寫入成功
i2cget -f -y 6 0x6a 0x09
# 應回傳 0x54

# 觀察充電能否超過 94%
cat /sys/class/power_supply/bq27546-0/capacity
```

### 關機後保持

BQ25895 由 VBAT（電池）供電，系統關機後暫存器值不會丟失。shutdown() 中重寫一次確保值正確。只有電池物理斷電才會重置為預設值，下次開機驅動載入時 hw_init() 會再次寫入。

---

## 相關檔案

```
kernel-6.1/drivers/power/supply/bq25890_charger.c                        # 充電驅動
kernel-6.1/arch/arm64/boot/dts/rockchip/myd-lr3576-usb-typec.dtsi        # DTS 配置 (充電IC、電量計)
kernel-6.1/arch/arm64/boot/dts/rockchip/rk3576-rk806.dtsi                # PMIC DTS
kernel-6.1/drivers/power/supply/bq27xxx_battery.c                        # BQ27546 驅動
```

## 待驗證

- [ ] 手動 I2C 寫入 REG09=0x54 後充電測試
- [ ] 溫度超過 31°C 時確認不再降壓
- [ ] 關機後重新開機確認暫存器值保持
- [ ] 確認 BQ27546 健康狀態回報 "Dead" 的原因
