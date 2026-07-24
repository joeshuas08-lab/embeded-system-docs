# Full Verification Final Report — MYD-YR3562 Security Pack

**Date**: 2026-07-24  
**Branch**: arm-binary-verification (based on myd)  
**SDK**: Buildroot 2024.02 + Linux 6.1.99

## All Items Verified

### On-Target Runtime
| Item | Version | Method |
|------|---------|--------|
| Docker daemon | 27.5.1 | Static binary, overlay2, running |
| cyclictest | Max 23µs | 2-thread, prio 99, 10s |
| 32-bit ARM binary | - | static compile, exit:0 |
| iptables | legacy | Docker chains active |
| telnet | busybox | factory rootfs |

### Buildroot Rootfs (clean build)
| Item | Status | Evidence |
|------|--------|----------|
| systemd 254.9 | ✅ | /sbin/init -> systemd, boot log confirms |
| OpenSSL | ✅ | ELF aarch64 binary in /usr/bin |
| PAM | ✅ | 6 config files in /etc/pam.d |
| iptables | ✅ | /usr/sbin/iptables |
| sudo | ✅ | PAM sudo config present |
| NTP | ✅ | package enabled in config |

### Pending
- PREEMPT_RT: CONFIG_PREEMPT_RT not set in factory config
- docker-compose: binary download needed (no internet on target)
- SSH on new rootfs: sshd installed but not reachable (need debug)

## Build Process
- `./build.sh kernel`: echo "2" | select myir defconfig
- `./build.sh rootfs`: needs clean build (rm -rf output dir first)
- Buildroot config: `rockchip_myd_yr3562_br_defconfig`
- Flash: `upgrade_tool WL <LBA>` NOT dd; must flash both A/B slots
- Kernel config merge: `merge_config.sh` > `olddefconfig`

## Key Fixes
- `pm_domains.c`: panic → return ret, timeout 10ms → 500ms
- `boot.its`: must include configurations + signature node
- `mkimage`: -E -p 0x800 flags required
