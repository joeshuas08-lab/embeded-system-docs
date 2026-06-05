# Fast Boot: DDR 初始化失敗修復

## 日期
2026-06-05

## 現象
全編燒錄 update.img 後串口：
```
### ERROR ### Please RESET the board
 revision is too low
ddrconfig:0
LPDDR4, 324MHz, Size=1024MB
```
DDR 訓練階段卡死，無法進入 U-Boot。

## 根因
**rkbin 倉庫處於錯誤 commit**：

| 狀態 | commit | 說明 |
|------|--------|------|
| 錯誤 HEAD | `6ac72a43 FEAT: Modify the baud rate to 115200` | 多了一個修改 |
| 正確 | `2c1be105 RKBOOT: Add RK3576MINIALL_IPC.ini` | `remotes/m/rk3562` |

rkbin 是 prebuilt binary 倉庫，MiniLoaderAll.bin 由以下合併生成：
1. **DDR binary** (`rk3562_ddr_1332MHz_v1.07.bin`) — 來自 rkbin
2. **U-Boot SPL** — `--spl-new` 從源碼編譯

全編時 U-Boot make.sh 用 `--spl-new` 重新打包，若 rkbin 的 DDR binary 有問題，整個 MiniLoaderAll.bin 就壞了。

## 修復
```bash
git -C rkbin stash push -m "auto: local changes"
git -C rkbin checkout remotes/m/rk3562  # 恢復到正確 commit
git -C rkbin checkout -b fast-boot       # 建立分支防止再次游離
./build.sh uboot                          # 重編 U-Boot
./build.sh firmware                       # 重打包
```

## 預防
- **rkbin 必須保持在 `m/rk3562` 遠程分支**，不能隨意前進
- 所有子倉庫統一在 `fast-boot` 分支，避免 detached HEAD
- 使用 `./repos.sh branch` 檢查所有倉庫狀態
