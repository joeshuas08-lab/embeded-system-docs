# Docker Kernel Verification — MYD-YR3562

**Date**: 2026-07-24  
**Branch**: arm-binary-verification (based on myd)

## Verified on Target

| Item | Result | Details |
|------|--------|---------|
| Kernel | ✅ 6.1.99-rt36 | SDK-built via ./build.sh kernel |
| Docker daemon | ✅ 27.5.1 | overlay2 storage driver |
| cyclictest | ✅ Max 23µs | 2-thread, priority 99, 10s |
| 32-bit ARM binary | ✅ EXIT:0 | malloc+float+file_io+uname all PASS |
| iptables | ✅ running | Docker chains active |
| telnet | ✅ /usr/bin/telnet | factory rootfs |

## Not Verified (config-only, needs ./build.sh rootfs)

| Item | Status |
|------|--------|
| PREEMPT_RT | factory config: # CONFIG_PREEMPT_RT is not set |
| OpenSSL | not in factory rootfs |
| systemd | factory uses busybox init |
| PAM | not configured |
| docker-compose | not installed (no internet on target) |

## Key Technical Decisions

### Kernel config: use merge_config.sh, NOT olddefconfig
- `scripts/kconfig/merge_config.sh -m factory_config docker_fragment`
- `olddefconfig` silently drops Rockchip options (ROCKCHIP_PM_DOMAINS, etc.)
- `merge_config.sh` preserves all existing options

### Boot.img: use SDK mk-fitimage.sh, NOT hand-written ITS
- SDK's ITS includes signature node (sha256,rsa2048:dev) + configurations
- mkimage flags: `-E -p 0x800`
- Flash via `upgrade_tool WL 0x8000` (absolute LBA), not `dd` to partition

### PM domain fix
- `drivers/soc/rockchip/pm_domains.c`: panic → return ret (non-fatal)
- Poll timeout: 10ms → 500ms (GCC compatibility)
- Without fix: kernel panic on CPU3 bringup

### Harness
- serial-probe.sh: auto-identify boards by uname, multiple boards default
- net-setup.sh: auto-discover Ethernet, configure 192.168.100.x link  
- relay.sh: direct CH340 control via /dev/ttyUSB0
- recover.sh: factory image UF flash + partition WL recovery
- All hooked via PreToolUse in settings.json

### Branch creation rule
- ALWAYS based on myd: `git branch arm-binary-verification myd`
- Hook enforced: blocks git checkout -b / repo start unless from myd

## 2026-07-24 Update: Full Verification Round

### Verified on Target
| Item | Result | Method |
|------|--------|--------|
| Docker daemon 27.5.1 | ✅ | static binary, overlay2 driver |
| cyclictest | ✅ | Max 23µs (no PREEMPT_RT) |
| 32-bit ARM binary | ✅ | base64 decode + execute, exit:0 |
| iptables | ✅ | factory rootfs, Docker chains active |
| telnet | ✅ | /usr/bin/telnet in factory rootfs |

### Buildroot Config (verified in .config, pending rootfs flash)
| Item | Config |
|------|--------|
| BR2_INIT_SYSTEMD | =y |
| BR2_PACKAGE_LIBOPENSSL | =y (with OPENSSL_BIN) |
| BR2_PACKAGE_IPTABLES | =y |
| BR2_PACKAGE_LINUX_PAM | =y |
| BR2_PACKAGE_SUDO | =y |
| BR2_PACKAGE_NTP | =y |
| BR2_PACKAGE_BUSYBOX_TELNETD | =y |
| BR2_PACKAGE_DOCKER_COMPOSE | =y |

### Pending
- PREEMPT_RT: NOT in factory config, needs RT patch
- docker-compose: binary download blocked (no internet on target/bridge)
- Full rootfs flash: clean rebuild needed, incremental build cached old rootfs

### Key Learnings
1. `merge_config.sh` preserves Rockchip configs; `olddefconfig` drops them
2. SDK mc-fitimage.sh produces correctly signed FIT (signature node required)
3. Flash via `upgrade_tool WL <LBA>` NOT `dd` to GPT partition
4. A/B slots: must flash BOTH mmcblk0p3 AND mmcblk0p5
5. Buildroot config goes to `rockchip_myd_yr3562_br_defconfig` (SDK path)
