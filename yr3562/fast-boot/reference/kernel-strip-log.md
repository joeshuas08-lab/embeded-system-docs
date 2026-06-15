# 內核瘦身記錄

## 日期
2026-06-15

## 瘦身前
- Kernel Image: 14 MB
- Defconfig lines: 483

## 瘦身後
- Kernel Image: **12 MB** (-14%)
- Defconfig lines: 388 (-20%)

## 裁減項

| 選項 | 原值 | 說明 | 效果 |
|------|------|------|------|
| `CONFIG_DEBUG_INFO` | y | DWARF 調試信息 | **最大收益** |
| `CONFIG_MODULES` | y | 模塊加載支持 | 中型 |
| `CONFIG_KALLSYMS` | y | 內核符號表 | 中型 |
| `CONFIG_DEBUG_FS` | y | debugfs 文件系統 | 小型 |
| `CONFIG_MAGIC_SYSRQ` | y | 魔術鍵 | 小型 |
| `CONFIG_FW_CACHE` | y | 固件緩存 | 小型 |
| `CONFIG_FUSE_FS` | y | FUSE | 小型 |
| `CONFIG_EFIVAR_FS` | m | EFI 變量（無用於 ARM） | 小型 |
| `CONFIG_SYMBOLIC_ERRNAME` | y | 錯誤號名稱映射 | 小型 |
| `CONFIG_LOG_BUF_SHIFT` | 14→12 | 日誌緩衝 16KB→4KB | 小型 |

## 被依賴拉回的選項
- `CONFIG_DEBUG_KERNEL` — Rockchip RGA/MPP 驅動依賴 `debugfs`
- 這些只是 debug 基礎設施，不佔用大量空間（沒有 DEBUG_INFO）

## 保留的關鍵選項
- `CONFIG_NET_CORE=y` ✅
- `CONFIG_STMMAC_ETH=y` ✅
- `CONFIG_STMMAC_FULL=y` ✅
- `CONFIG_DWMAC_ROCKCHIP=y` ✅
- `CONFIG_REALTEK_PHY=y` ✅
- `CONFIG_MOTORCOMM_PHY=y` ✅
