# LVGL 顯示中斷的兩個坑

## 日期
2026-06-25

## 背景
在合併 131 Debian SDK 快速啟動優化到 Buildroot SDK 時，逐步排查發現兩項改動會導致 LVGL 畫面不出。

---

## 坑 1：去 OP-TEE (BL32)

### 現象
禁用 OP-TEE 後（U-Boot `CONFIG_OPTEE_CLIENT=n` + rkbin `SEC=0`），LVGL 不出畫面。

### 原因
RK3562 的 ATF (bl31) 需要 OP-TEE 安全環境才能正常初始化顯示子系統。去掉 OP-TEE 後，DRM/GPU 初始化失敗。

### 驗證
131 Debian SDK 的 CHANGE_REPORT 也有警告：
> "If the board fails to boot (hangs at ATF stage), re-enable CONFIG_OPTEE_V2=y"

### 涉及文件
- `u-boot/configs/myd_yr3562_ub_defconfig`
- `rkbin/RKTRUST/RK3562TRUST.ini`（BL32 SEC=1 必須保持）

---

## 坑 2：console 波特率 + quiet

### 現象
修改 `bootargs` 中任何以下改動都會導致 LVGL 不出畫面：

| 改動 | 正常 | 異常 |
|------|------|------|
| 去波特率 | `console=ttyFIQ0,115200` | `console=ttyFIQ0` |
| 去 quiet | `loglevel=3 quiet` | `loglevel=3` |

### 原因
ttyFIQ0 的 FIQ debugger 需要明確波特率 `,115200` 才能正常初始化。不指定時 UART 時序錯亂，影響顯示子系統的初始化。

`quiet` 參數影響內核日誌輸出級別，間接影響 DRM 驅動的初始化順序。去掉 `quiet` 後內核輸出的日誌量增加，導致初始化時序變化。

### 涉及文件
- `kernel-6.1/arch/arm64/boot/dts/rockchip/myd-yr3562.dts`
- 或 `myd-yr3562.dtsi` 中的 `chosen/bootargs`

---

## 安全可用的優化

| 優化 | 安全 |
|------|:--:|
| 加 `earlycon` | ✅ |
| 去 RSA signature (boot.its) | ✅ |
| 去 FIT/HW crypto | ✅ |
| LZO 支援 | ✅ |
| 去 Android/AVB bootloader | ✅ |
| 去 CMD_BOOT_ANDROID | ✅ |
| `rootdelay=0` | ✅ |
| NPU disabled | ✅ |
| `PRINTK_TIME=n` | ✅ |
| **去 OP-TEE** | ❌ |
| **去 console 波特率** | ❌ |
| **去 quiet** | ❌ |
| 131 DDR binary (V2) | ❌ V1 不兼容 |
