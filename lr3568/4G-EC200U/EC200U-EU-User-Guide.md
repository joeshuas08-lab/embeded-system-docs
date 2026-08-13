# MYD-LR3568 Platform: Quectel EC200U-EU 4G Module User Guide

Version: V1.0 | Date: 2026-08-13
Applicable to: MYD-LR3568 development board (GK-B baseboard), Rockchip Linux SDK (kernel 6.1 / Buildroot), Quectel EC200U-EU 4G module

---

## 1. Overview

This guide covers installation, configuration, and PPP dial-up for the EC200U-EU 4G LTE module on the MYD-LR3568 development platform.

Module information:

| Item | Details |
|---|---|
| Module | Quectel EC200U-EU |
| Firmware version | EC200UEUAAR02A02M08_HJ |
| USB VID/PID | `2c7c:0901` (ECM mode) |
| Supported bands | LTE-FDD: B1/B3/B7/B8/B20/B28 |
| USB interfaces | 7×AT/DIAG serial ports (ttyUSB0-6) + 1×ECM network device (usb0) |

Band notes (EU firmware):

- LTE-FDD B1 (2100), B3 (1800), B7 (2600), B8 (900), B20 (800), B28 (700 MHz) — the primary LTE bands of major European operators (Vodafone, Deutsche Telekom, Orange, Telefonica, etc.)
- B20 (800 MHz) and B28 (700 MHz) are the low-band frequencies used across Europe for rural and indoor coverage — both supported
- TD-LTE bands (B38/B39/B40/B41, used by some Asian operators) are not supported
- The validation in this document used a China Mobile SIM in the lab (FDD B3/B8 refarming coverage). For European deployment, use a local operator SIM and set the operator's APN (Section 5.2)

---

## 2. Hardware Installation

Bill of materials:

| Item | Description |
|---|---|
| MYD-LR3568 development board (GK-B baseboard) | 4G interface: mini-PCIe socket, SIM card socket adjacent |
| EC200U-EU module (mini-PCIe form factor) | Insert into the baseboard 4G socket |
| SIM card | micro-SIM (3FF); nano-SIM requires an adapter |
| 4G antenna | Connect to the module MAIN antenna port |

Installation steps:

1. Insert the 4G module: align the EC200U-EU bevel with the mini-PCIe socket, press down, secure both screws
2. Insert the SIM card:
   - The socket opening faces outward; insert the card from the opening
   - Align the card's chamfered corner (notch) with the silkscreen icon above the socket
   - Push fully until it clicks (push-push socket)
   - If inserted 180° the wrong way, the module cannot detect the card: `AT+CPIN?` returns `SIM not inserted`, and pressing the card has no effect
3. Connect the antenna to the module MAIN port; check the cable for damage
4. Power on the board

Board power: the 4G rail `vcc_4g_pwr` (GPIO0_PC2, 3.3 V, always-on) is controlled automatically by the baseboard. No manual intervention required.

---

## 3. Software Environment

SDK requirements:

| Component | Version / Configuration |
|---|---|
| Kernel | 6.1.99 (Rockchip BSP `linux-6.1-stan-rkr5`) |
| Root filesystem | Buildroot (`rockchip_rk3568_myir_full_defconfig`) |
| Kernel config | `myd_lr3568x_defconfig` (all required drivers included, no modification needed) |

Relevant kernel configuration:

| Config | Purpose |
|---|---|
| `CONFIG_USB_SERIAL_OPTION` | AT/DIAG serial ports (ttyUSB0-6) |
| `CONFIG_USB_NET_QMI_WWAN` / `CONFIG_USB_NET_CDC_MBIM` | QMI/MBIM dial-up (reserved) |
| `CONFIG_USB_NET_CDC_ETHER` | ECM network device (usb0) |
| `CONFIG_PPP` / pppd / chat | PPP dial-up (used in this solution) |

Module enumeration check after boot:

```sh
lsusb                          # should show: 2c7c:0901
ls -l /dev/ttyUSB*             # should show: ttyUSB0 ~ ttyUSB6
ip link show usb0              # ECM network device (not used by this solution)
```

---

## 4. AT Command Verification

Use `microcom` (pre-installed on the target):

```sh
printf 'AT\r\n' | microcom -s 115200 -t 2500 /dev/ttyUSB0
```

Note: the AT port is ttyUSB0 (ttyUSB1/2 do not respond). If the module does not respond to AT shortly after power-up, re-enumerate the module's USB:

```sh
echo 0 > /sys/bus/usb/devices/3-1.1/authorized; sleep 1; echo 1 > /sys/bus/usb/devices/3-1.1/authorized
```

Common verification commands:

| Command | Expected | Meaning |
|---|---|---|
| `AT` | `OK` | Communication OK |
| `ATI` | Quectel / EC200U | Module model |
| `AT+CPIN?` | `+CPIN: READY` | SIM card detected |
| `AT+CSQ` | `+CSQ: xx,99` (xx≥10) | Signal strength |
| `AT+CEREG?` | `+CEREG: 0,1` | Registered to LTE network |
| `AT+COPS?` | `+COPS: 0,0,"<operator>",7` | Current operator |

---

## 5. PPP Dial-Up

On this firmware (R02A02) the ECM network bridge is not functional (usb0 stays without carrier), and the QMI dial-up tool (quectel-CM) is not included in the rootfs. PPP dial-up is the recommended method.

### 5.1 One-Time Configuration (written to /etc, persists across reboots)

```sh
# 1) Create the lock directory (/var/run is tmpfs — must be recreated after reboot)
mkdir -p /var/run/pppd/lock

# 2) Write chat script /etc/ppp/chat/qc
printf "ABORT ERROR\nABORT BUSY\n'' AT\nOK ATD*99#\nCONNECT ''\n" > /etc/ppp/chat/qc

# 3) Write peers config /etc/ppp/peers/qc
printf "connect '/usr/sbin/chat -v -f /etc/ppp/chat/qc'\n/dev/ttyUSB0\n115200\nnoipdefault\nusepeerdns\ndefaultroute\n" > /etc/ppp/peers/qc
```

### 5.2 Configure the APN (per your operator; example below is the lab test value)

```sh
printf 'AT+CGDCONT=1,"IP","cmnet"\r\n' | microcom -s 115200 -t 2500 /dev/ttyUSB0
```

Replace `cmnet` with your operator's APN (Vodafone, Telekom, Orange, etc.). The APN is the only operator-specific setting.

### 5.3 Dial

```sh
nohup pppd call qc >/tmp/ppp.log 2>&1 &
# Wait ~10 s, then confirm:
ip addr show ppp0        # should show an inet address
route -n                 # should show default route 0.0.0.0 -> ppp0
```

### 5.4 DNS and Connectivity Test

```sh
echo "nameserver 8.8.8.8" > /etc/resolv.conf
ping -c 3 8.8.8.8              # IP connectivity
ping -c 3 www.google.com       # DNS resolution + connectivity
```

(Or use your local DNS, e.g. 1.1.1.1.)

### 5.5 Recovery After Reboot

```sh
mkdir -p /var/run/pppd/lock                       # /var/run is tmpfs
nohup pppd call qc >/tmp/ppp.log 2>&1 &           # re-dial
```

For automatic dial-up at boot, add these commands to an init script (S99 level) in the rootfs overlay.

---

## 6. Troubleshooting

| Symptom | Possible cause | Remedy |
|---|---|---|
| `lsusb` shows no 2c7c device | Module not seated / power issue | Re-seat the module, check the 4G socket |
| `AT+CPIN?` -> `SIM not inserted` | SIM card inserted the wrong way / not fully inserted / wrong card size | Re-insert per Section 2 (notch aligned with silkscreen); use micro-SIM or adapter |
| `AT+CSQ` -> `99,99` | Antenna not connected / damaged / no coverage | Check antenna, relocate |
| `+CEREG: 0,2` searching for long | Band mismatch (see Section 1) | Use a SIM whose operator's bands are supported |
| `+CEREG: 0,3` | Network rejection | Verify SIM is active, IMSI not blocked |
| pppd exits immediately, log shows `Can't create lock file` | `/var/run/pppd/lock` missing | `mkdir -p /var/run/pppd/lock` |
| pppd does not respond | ttyUSB0 busy | Ensure no other process (e.g. microcom) holds the port |

---

## 7. Notes and Limitations

1. The AT port is ttyUSB0; ttyUSB1/2 do not respond. After power-up the module may not respond to AT — re-enumerate USB if needed (Section 4)
2. ECM mode is not functional on this firmware (usb0 has no carrier). For ECM/QMI, update the module firmware or enable quectel-CM
3. RNDIS mode is not supported (kernel `CONFIG_USB_NET_RNDIS_HOST` not enabled). Do not switch the module to usbnet=2
4. One module crash-restart ~70 s after power-on was observed in three power cycles (~3 s downtime, auto-recovered; see test report Section 3.7). Check the 3.3 V supply under transmit load; consider upgrading the firmware (current R02A02 is an early release — request the latest EU firmware from Quectel)
5. The 4G power rail `vcc_4g_pwr` is always-on via GPIO0_PC2; there is no separate reset GPIO
