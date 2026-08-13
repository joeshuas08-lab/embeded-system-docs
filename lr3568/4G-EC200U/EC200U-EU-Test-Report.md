# MYD-LR3568 + Quectel EC200U-EU 4G Module Test Report

**Report version**: V1.0　　**Test date**: 2026-08-13
**Testers**: (TBD)　　**Test location**: Shenzhen (indoor laboratory)

---

## 1. Test Environment

| Item | Specification |
|---|---|
| Development board | MYD-LR3568 (baseboard **GK-B**, mini-PCIe 4G socket + SIM card socket) |
| SoC | Rockchip RK3568 (Quad-core Cortex-A55) |
| Kernel | Linux 6.1.99 (Rockchip BSP, custom build #37) |
| Root filesystem | Buildroot (full configuration) |
| 4G module | Quectel **EC200U-EU** (mini-PCIe form factor) |
| Module firmware | `EC200UEUAAR02A02M08_HJ` (R02A02) |
| Module USB ID | `2c7c:0901` (ECM mode, bcdDevice 3.18) |
| SIM card | China Mobile (IMSI `460025191757605`, APN: cmnet) — lab test SIM; for EU deployment use a local operator SIM |
| Antenna | External 4G main antenna (indoor environment) |

---

## 2. Test Results Summary

| # | Test item | Result | Verdict |
|---|---|---|---|
| 1 | USB enumeration | `2c7c:0901` enumerated correctly | PASS |
| 2 | Driver binding | option → ttyUSB0-6 (7 ports); cdc_ether → usb0 | PASS |
| 3 | AT communication | All commands answered correctly on ttyUSB0 | PASS |
| 4 | SIM card detection | `+CPIN: READY` | PASS |
| 5 | Signal strength | `+CSQ: 22,99` (approx. -68 dBm) | PASS |
| 6 | Network registration | `+CEREG: 0,1`; `+COPS: 0,0,"CHINA MOBILE",7` (LTE) | PASS |
| 7 | PPP data link | ppp0 obtained IP, default route established | PASS |
| 8 | External connectivity | 0% packet loss on both IP and domain pings | PASS |
| 9 | Stability | 3 power cycles: 1 occasional crash-restart, 2 without anomaly | CONDITIONAL PASS |
| 10 | ECM mode | usb0 without carrier, data link cannot be established | N/A (firmware limitation) |
| 11 | RNDIS mode | Kernel `CONFIG_USB_NET_RNDIS_HOST` not enabled | NOT SUPPORTED |

---

## 3. Detailed Test Results

### 3.1 USB Enumeration and Drivers (PASS)

```sh
$ lsusb
Bus 003 Device 003: ID 2c7c:0901   <- Quectel EC200U

$ dmesg
usb 3-1.1: New USB device found, idVendor=2c7c, idProduct=0901, bcdDevice=3.18
cdc_ether 3-1.1:1.0 usb0: register 'cdc_ether'  (ECM network device)
option 3-1.1:1.2~1.8: GSM modem -> ttyUSB0 ~ ttyUSB6 (7 serial ports)
```

### 3.2 Module Information (PASS)

```
ATI -> Quectel / EC200U / Revision: EC200UEUAAR02A02M08_HJ
```

### 3.3 SIM Detection (PASS)

```
AT+CPIN?      -> +CPIN: READY
AT+CIMI       -> 460025191757605
```

> Note: when the SIM card was inserted 180° the wrong way, the module reported `+CME ERROR: SIM not inserted`; re-inserting per the socket silkscreen and rebooting resolved it.

### 3.4 Signal and Registration (PASS)

```
AT+CSQ        -> +CSQ: 22,99   (indoor, approx. -68 dBm)
AT+CEREG?     -> +CEREG: 0,1   (registered to LTE)
AT+COPS?      -> +COPS: 0,0,"CHINA MOBILE",7
```

> Note: the lab area has China Mobile FDD B3/B8 refarming coverage, so the EU-band module (B1/B3/B7/B8/B20/B28) registered normally. European networks on B1/B3/B7/B8/B20/B28 are likewise supported.

### 3.5 PPP Dial-Up (PASS)

APN configured (cmnet) → pppd + chat (ATD*99#) dial-up:

```
ppp0: <POINTOPOINT,UP,LOWER_UP>
    inet 10.68.75.125 peer 192.168.0.1/32
Route: 0.0.0.0/0 via ppp0 (default route)
```

### 3.6 External Connectivity (PASS)

| Target | Result | Latency |
|---|---|---|
| 223.5.5.5 (IP) | 3/3 packets, 0% loss | 61.7 / 209.5 ms (first packet slower) |
| www.baidu.com (DNS → 183.240.99.224) | 2/2 packets, 0% loss | 56.8 / 285 ms |

DNS resolution worked after configuring `nameserver 223.5.5.5`.

### 3.7 Stability Observation (CONDITIONAL PASS)

| Power cycle | Observation |
|---|---|
| 1st | ~70 s after power-on the module's USB dropped (`usb 3-1.1: USB disconnect`), briefly re-enumerated in Unisoc boot-ROM state (`1782:4d12`) then auto-recovered; ~3 s downtime; stable afterwards |
| 2nd | No anomaly (5+ min observation, zero dmesg disconnects) |
| 3rd | No anomaly (30+ min observation, zero dmesg disconnects, PPP link stayed up throughout) |

**Analysis**: the crash occurs in the module's RF start-up / first-transmit window. Host-side logs show no anomalies (no over-current, no USB errors), so it is attributed to the module side (firmware or supply). Based on Quectel community cases, likely causes are a 3.3 V supply dip under transmit peak current, or a firmware defect (current R02A02 is an early release).

**Recommendations**:
1. Monitor the module 3.3 V rail with an oscilloscope during registration/transmit bursts
2. Request the latest EU firmware (R03A1x or newer) from Quectel and re-test
3. For volume deliveries, extend burn-in testing (e.g., 48 h with repeated power cycles)

---

## 4. Conclusion

1. **The EC200U-EU module is fully functional on the MYD-LR3568 platform**: enumeration, drivers, AT, SIM detection, network registration and PPP dial-up all passed; external connectivity 0% packet loss
2. **Zero software changes required**: all kernel drivers are built in — no DTS/kernel config modification; only PPP dial-up configuration at user space
3. **Notes**:
   - The SIM card must be inserted per the socket silkscreen orientation (wrong orientation prevents detection)
   - ECM bridging is not functional on this firmware — use PPP dial-up; RNDIS mode is not supported
   - An occasional crash-restart ~70 s after power-on was observed (1 in 3 power cycles); follow Section 3.7 recommendations

---

## 5. Appendix: Key Command Quick Reference

```sh
# Enumeration check
lsusb && ls /dev/ttyUSB*

# AT verification (ttyUSB0)
printf 'AT+CPIN?\r\n' | microcom -s 115200 -t 2500 /dev/ttyUSB0

# Configure APN (replace cmnet with your operator's APN)
printf 'AT+CGDCONT=1,"IP","cmnet"\r\n' | microcom -s 115200 -t 2500 /dev/ttyUSB0

# PPP dial-up
mkdir -p /var/run/pppd/lock
nohup pppd call qc >/tmp/ppp.log 2>&1 &

# Verification
ip addr show ppp0 && route -n
ping -c 3 8.8.8.8
```
