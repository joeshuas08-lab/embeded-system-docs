# Fast Boot: 以太網修復

## 日期
2026-06-05

## 現象
`ip link show` 只有 `lo`，`ifconfig` 無輸出。DTS 層 GMAC0/GMAC1 已 `status = "okay"`。

## 根因
TB 內核 defconfig (`myd_yr3562_tb_defconfig`) 缺三個選項：

| 選項 | 作用 |
|------|------|
| `CONFIG_NET_CORE=y` | 核心網絡驅動支持（TB 原本 `# CONFIG_NET_CORE is not set`） |
| `CONFIG_STMMAC_FULL=y` | 完整 STMMAC GMAC 功能 |
| `CONFIG_REALTEK_PHY=y` | RTL8211F PHY 驅動（GMAC0 千兆口用） |

`CONFIG_DWMAC_ROCKCHIP=y` 和 `CONFIG_MOTORCOMM_PHY=y` 原本已開，但缺上面三個導致 GMAC probe 靜默失敗。

## 修復
在 `kernel-6.1/arch/arm64/configs/myd_yr3562_tb_defconfig`：
```
CONFIG_NET_CORE=y          # 替換 # CONFIG_NET_CORE is not set
CONFIG_STMMAC_FULL=y       # 新增在 CONFIG_STMMAC_ETH=y 之後
CONFIG_REALTEK_PHY=y       # 追加在文件末尾
```

## Rootfs
`board/myir/myir_buildroot_yr3562/etc/init.d/S40network_static`：
```sh
ip addr add 192.168.1.100/24 dev eth0
ip link set eth0 up
```

## 鏡像大小
| 配置 | Image | boot.img |
|------|-------|----------|
| TB 原版 | 13 MB | 15 MB |
| TB + 網絡 | 14 MB | 26 MB |
| BSP 完整 | 39 MB | 52 MB |

只增大 1MB。
