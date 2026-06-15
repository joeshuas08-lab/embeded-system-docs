# RK3506 三家厂商快速启动方案调研 & RK3562 可行性评估

## 日期
2026-06-05

## 调研结果

### 三家方案对比

| 维度 | 飞凌 FET3506J-S | 米尔 MYD-YR3506 | 创龙 TL3506-EVM |
|------|----------------|-----------------|----------------|
| **LVGL 启动** | ~2.0s | ~2.0s | **1.97s** |
| **Qt 启动** | - | - | 2.66s |
| **核心板价格** | - | - | ¥79 |
| **方案名称** | Thunder Boot | Fast Boot Scheme (SDK V1.2.0) | 闪电启动 (Thunder Boot) |

**共同技術路線（全链路瘦身三板斧）：**

### 1. U-Boot 階段 — Thunderboot
```
常規: BootROM → SPL → U-Boot → Kernel → Rootfs → App
優化: BootROM → SPL ────────→ Kernel → Rootfs(最小) → App
                      ↑ 跳過 U-Boot
```
SPL 直接引導內核，繞開 U-Boot proper 的大量初始化（USB、網絡、存儲枚舉等）。

### 2. Kernel 階段 — 猛瘦身
- 關閉 `console=` 串口日誌輸出（`loglevel=3 quiet`）
- 禁用非必要外設驅動
- 僅保留 HDMI/LVDS/MIPI-DSI + USB + 基礎網絡

### 3. Rootfs 階段 — Buildroot 最小配置
- Buildroot 而非 Debian/Ubuntu
- 僅保留 LVGL 主程序 + 觸摸後台 + 系統核心進程
- LVGL 9.x 直接跑在 DRM 上（跳過 Framebuffer 層），雙緩衝
- 用 evdev 對接觸摸，響應延遲 < 15ms

---

## RK3562 現狀 (4.7s) vs RK3506 (2.0s) — 差距分析

### 已實現
| 優化項 | RK3562 現狀 |
|--------|-----------|
| U-Boot 跳過 | **未實現** ❌ |
| 內核瘦身 | 已做：無 WiFi/BT/TEE/OPTEE，13MB Image ✅ |
| 串口靜默 | `loglevel=3 quiet` ✅ |
| Rootfs 精簡 | Buildroot TB 配置，去 SSH/WiFi/BT/NET 服務 ✅ |
| LVGL 框架 | LVGL 8.x on DRM ✅ |
| 顯示接口 | MIPI DSI (LT8912) ✅ |

### 差距 = 2.7s 從哪來？

| 階段 | 預估耗時 | 可優化空間 |
|------|---------|-----------|
| DDR 初始化 | ~200ms | 固定，無法優化 |
| SPL | ~100ms | 固定 |
| **U-Boot proper** | **~1.5-2.0s** | ⭐ **Thunderboot 可完全消除** |
| Kernel 解壓+啟動 | ~1.5s | 可再裁減，~0.5s |
| Rootfs init + LVGL | ~0.8-1.0s | 已較優，空間有限 |

### 可行性結論

| 技術 | RK3562 可行性 | 難度 | 預期收益 |
|------|:--:|:--:|:--:|
| **Thunderboot** | ⚠️ 待確認 | 高 | **1.5-2.0s** |
| SPARSE image 格式優化 | ✅ | 低 | 0.1-0.2s |
| initramfs 代替 rootfs 掛載 | ✅ | 中 | 0.3-0.5s |
| 進一步步裁減 kernel | ✅ | 低 | 0.2-0.3s |

**核心問題：RK3562 的 Thunderboot 支援**

RK3506 (3×A7) 和 RK3562 (4×A53) 架構不同：
- RK3506 的 Thunderboot 利用其 AMP 架構（M0 協處理器輔助啟動）
- RK3562 U-Boot 源碼中未見 `CONFIG_THUNDERBOOT` 或等效選項
- RK3562 的 SPL → Kernel 直接跳轉需要：ATF (BL31) 初始化被 SPL 完成、DDR 已在 MiniLoaderAll 階段初始化
- **理論上可行**，但需要深度適配：修改 SPL 的 FIT image handling，讓其加載 kernel + DTB 而非 uboot

**預估：如果能實現 Thunderboot，RK3562 可達 2.5-3.0s**

---

## 建議下一步

1. **查 RK3562 是否官方支持 Thunderboot** — 問瑞芯微 FAE
2. **嘗試 SPL → Kernel 直跳** — 修改 U-Boot SPL FIT 配置
3. **initramfs 代替 squashfs rootfs** — 減少存儲讀取延遲
4. **grabserial 精確測量各階段** — 定位瓶頸

---

## 參考來源
- [创龙科技：LVGL 1.97s / Qt 2.66s](https://www.tronlong.com/Article/show/428.html)
- [米尔 MYD-YR3506 SDK V1.2.0 Fast Boot](https://en.myir.cn/ProductUpdates/150.html)
- [飞凌 FET3506J-S 核心板](https://www.forlinx.com/article-search-rk-article-8.html)
