#!/bin/sh
# check4g.sh - EC200U-EU diagnostic: enumeration, SIM, signal, registration, link
# Run on the target board.

echo "== USB enumeration =="
lsusb | grep 2c7c || echo "module not found"

echo "== Serial ports =="
ls /dev/ttyUSB* 2>/dev/null

echo "== SIM / signal / registration =="
printf 'AT+CPIN?\r\n' | microcom -s 115200 -t 2500 /dev/ttyUSB0
printf 'AT+CSQ\r\n' | microcom -s 115200 -t 2500 /dev/ttyUSB0
printf 'AT+CEREG?\r\n' | microcom -s 115200 -t 2500 /dev/ttyUSB0
printf 'AT+COPS?\r\n' | microcom -s 115200 -t 2500 /dev/ttyUSB0

echo "== PPP link =="
ip addr show ppp0 2>&1 | grep -E "inet |state"
route -n | head -3
