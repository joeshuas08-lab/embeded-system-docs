# MYD-LR3568 + EC200U-EU 4G Deliverables — Package Manifest

Version: V1.0 | Date: 2026-08-13

## Package Contents

| File | Description |
|---|---|
| `EC200U-EU-User-Guide.docx` | User guide (Word, for customer): installation, AT, PPP dial-up, troubleshooting |
| `EC200U-EU-Test-Report.docx` | Test report (Word, for customer) with measured data |
| `EC200U-EU-使用说明.docx` | 使用说明（中文 Word 版） |
| `EC200U-EU-测试报告.docx` | 测试报告（中文 Word 版） |
| `EC200U-EU-User-Guide.md` / `EC200U-EU-Test-Report.md` | Markdown sources (editable originals) |
| `ppp/chat/qc` | pppd chat script (verified on target) |
| `ppp/peers/qc` | pppd peers config (verified on target) |
| `scripts/dial4g.sh` | One-command dial helper (sets APN, dials, DNS) |
| `scripts/check4g.sh` | Diagnostic script (enumeration/SIM/signal/registration/link) |
| `scripts/rootfs-overlay/` | Rootfs overlay for automatic dial at boot (`etc/init.d/S99quectel4g`) |

## Software Status

- **No source code changes required.** All drivers are built into the SDK kernel config (`myd_lr3568x_defconfig`): `CONFIG_USB_SERIAL_OPTION`, `CONFIG_USB_NET_CDC_ETHER`, `CONFIG_USB_NET_QMI_WWAN`, `CONFIG_PPP`.
- No DTS, U-Boot or Buildroot modifications were made.
- Firmware delivered with this package is the standard SDK build (kernel 6.1.99).

## Deployment

Copy to target (or into rootfs overlay):

```sh
mkdir -p /etc/ppp/chat /etc/ppp/peers
cp ppp/chat/qc /etc/ppp/chat/
cp ppp/peers/qc /etc/ppp/peers/
chmod +x scripts/*.sh

# manual dial
./scripts/dial4g.sh <apn>

# auto dial at boot: copy scripts/rootfs-overlay/etc/init.d/S99quectel4g
# into the rootfs overlay's /etc/init.d/ and rebuild the rootfs
```

A customized rootfs image with automatic 4G dial-up enabled at boot can be provided on request.

## EU Deployment Notes

1. **Bands**: LTE-FDD B1/B3/B7/B8/B20/B28 — covers the primary LTE bands of major European operators (Vodafone, Deutsche Telekom, Orange, Telefonica). B20 (800 MHz) and B28 (700 MHz) low-band coverage supported.
2. **APN**: set per operator in `dial4g.sh` / Section 5.2 of the user guide (e.g. `web.vodafone.de`, `internet.telekom`).
3. **Carrier acceptance**: confirm with the customer that EC200U-EU is on the operator's approved module list.
4. **Live-network validation**: the test report data was collected with a China Mobile SIM (lab). Before volume shipment, re-validate with an EU operator SIM, paying attention to B20/B28 coverage and LTE registration behavior.
5. **Known limitation**: ECM mode is not functional on module firmware R02A02 (usb0 without carrier) — PPP is the supported dial method. RNDIS mode is not supported by the kernel config.
6. **Occasional crash-restart ~70 s after power-on** was observed once in three power cycles (test report 3.7). Verify 3.3 V supply under transmit load; consider firmware update to R03A1x+ (contact Quectel).
