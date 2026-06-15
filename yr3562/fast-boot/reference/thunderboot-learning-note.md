# Thunderboot 學習筆記

## 來源
飞凌、米尔、创龙三家 RK3506 快速启动方案調研，2026-06-05

## 什麼是 Thunderboot

```
常規啟動: BootROM → SPL(DDR init) → ATF → U-Boot → Kernel → Rootfs → App
                            ↑ ~2s 初始化 USB/網絡/存儲/環境變量
Thunderboot: BootROM → SPL(DDR init) → ATF → Kernel → Rootfs → App
                            ↑ 直接加載 Kernel+DTB，跳過 U-Boot
```

SPL 不走傳統 U-Boot proper，直接引導 Linux 內核。節省約 1.5-2s。

## 技術實現

### U-Boot mainline 方案：Falcon Mode
- `CONFIG_SPL_OS_BOOT=y`
- `spl_start_uboot()` 返回 0 表示直接引導 OS
- SPL 從存儲加載 kernel+DTB，跳過 U-Boot

### RK3506 方案：Rockchip 客製化
- RK3506 用 AMP 架構（Cortex-A7 ×3 + M0），M0 協處理器輔助快速啟動
- Rockchip U-Boot 2017 fork 可能有客製化的 Thunderboot 補丁
- MiniLoaderAll 合併 DDR init + SPL，SPL 可直接 load FIT image（kernel+DTB）

### RK3562 可行性
- U-Boot 版本: 2017.09（Rockchip fork）
- `CONFIG_SPL_KERNEL_BOOT` 存在於 `include/spl.h`（僅定義 partition 輔助函數）
- `CONFIG_SPL_OS_BOOT` 存在於通用 SPL 代碼（`common/spl/spl_mmc.c`）
- **但是 RK3562 客製化 spl.c 未實現 `spl_start_uboot()`**
- 需要做：在 Rockchip spl.c 中實現 `spl_start_uboot()`，返回 0 表示 Falcon mode

## 關鍵代碼路徑

```
common/spl/spl_mmc.c:
  spl_mmc_load_image() → spl_start_uboot() 檢查
    → 返回 0: spl_load_image_fit_os() 加載 kernel
    → 返回 1: 正常流程，加載 U-Boot

arch/arm/mach-rockchip/spl.c:
  需要實現 spl_start_uboot() (目前未實現)
  + 需要 spl_perform_fixups() 設置 kernel bootargs
```

## 下一步實現計劃

1. 在 `myd_yr3562_ub_defconfig` 啟用 `CONFIG_SPL_OS_BOOT=y`
2. 在 Rockchip `spl.c` 實現 `spl_start_uboot()`
3. 修改 FIT image 配置，讓 SPL 能直接加載 kernel+DTB
4. 測試能否從 SPL 直跳到 kernel

## 2026-06-15 實現進展

### RK3562 已有 Thunderboot 代碼！
在 `u-boot/arch/arm/mach-rockchip/spl.c` 中發現完整的 Thunderboot 實現：

```c
void spl_next_stage(struct spl_image_info *spl)
{
    // 默認: SPL_NEXT_STAGE_KERNEL ← 直接啟動 kernel!
    // 按鍵/低電量/loader模式: SPL_NEXT_STAGE_UBOOT ← 回退 U-Boot
    reg_boot_mode = readl((void *)CONFIG_ROCKCHIP_BOOT_MODE_REG);
    switch (reg_boot_mode) {
    case BOOT_LOADER: ... → U-Boot
    default: → SPL_NEXT_STAGE_KERNEL
    }
}
```

### 實現步驟

1. ✅ 創建 `u-boot/configs/rk3562_tb.config` (fragment)
   ```
   CONFIG_SPL_KERNEL_BOOT=y
   # CONFIG_SPL_KERNEL_BOOT_PREBUILT is not set
   ```

2. ✅ 編譯成功：`make.sh myd_yr3562_ub rk3562_tb RK3562MINIALL.ini --spl-new`
   - MiniLoaderAll.bin: 465KB (MD5: dfd1b782...)

3. ✅ 固件打包完成

### 啟動流程

```
正常:  DDR → SPL → ATF → U-Boot → Kernel → Rootfs
TB:    DDR → SPL → ATF → Kernel → Rootfs
                  ↑ 跳過 U-Boot (~1.5-2s)
```

### 安全機制
- 按住音量鍵 → 進入 U-Boot（恢復模式）
- Ctrl+C → 進入 U-Boot
- Boot mode 寄存器設為 loader → 進入 U-Boot

### 待測試
- [ ] 燒錄後確認能否直接啟動 kernel
- [ ] 測量啟動時間改善
- [ ] 如果失敗，回退機制是否正常

### v5 成功啟動內核！ (2026-06-15 15:49)

SPL→ATF→Kernel 全鏈路打通：

| 階段 | 耗時 | 狀態 |
|------|------|:--:|
| DDR init | ~64ms | ✅ |
| SPL + ATF + OP-TEE 加載 | ~120ms | ✅ |
| Kernel load (FIT from boot) | ~65ms | ✅ |
| ATF → Kernel jump | entry=0x400000 | ✅ |
| CPU0-3 初始化 (OP-TEE) | - | ✅ |

**三處關鍵修復：**

1. **boot.its config description**: `fit_find_config_node` 要求 config node 有 `description` property
2. **FDT load address**: `0xffffff00` → `0x0a000000` (佔位符 → 有效物理地址)
3. **Kernel load/entry**: `0xffffff01` → `0x00400000` (佔位符 → 有效物理地址)

### 殘留問題

- Kernel console (`ttyFIQ0,115200`) 和 SPL console (UART2, 1.5M) 在不同串口，無法看到 kernel log
- `loglevel=3 quiet` 使 kernel 輸出極少
- 未確認 rootfs 是否正常掛載
