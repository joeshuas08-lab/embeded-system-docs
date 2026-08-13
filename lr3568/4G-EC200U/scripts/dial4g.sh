#!/bin/sh
# dial4g.sh - PPP dial-up helper for EC200U-EU on MYD-LR3568
# Usage: dial4g.sh [apn]    (default APN: cmnet)
# Requires: /etc/ppp/chat/qc, /etc/ppp/peers/qc (included in this package)

APN="${1:-cmnet}"

mkdir -p /var/run/pppd/lock

# Set APN (replace with operator APN for EU deployment, e.g. web.vodafone.de)
printf 'AT+CGDCONT=1,"IP","%s"\r\n' "$APN" | microcom -s 115200 -t 2500 /dev/ttyUSB0

# Dial
nohup pppd call qc >/tmp/ppp.log 2>&1 &
sleep 10

if ip addr show ppp0 2>/dev/null | grep -q "inet "; then
    echo "PPP link UP: $(ip addr show ppp0 | grep 'inet ' | awk '{print $2}')"
    echo "nameserver 8.8.8.8" > /etc/resolv.conf
    echo "DNS set (8.8.8.8). Test: ping -c 3 8.8.8.8"
else
    echo "PPP link FAILED - see /tmp/ppp.log"
fi
