# 驱动调试方法：日志 + 串口桥接定位问题

**日期:** 2026-06-04

## 策略

内核驱动问题三板斧：

1. **关键路径打 `dev_info` 日志**（非 `dev_dbg`），不依赖动态调试开关
2. **串口桥接（SSH → socat → tty）** 远程读 `dmesg` 和寄存器
3. **延迟采样脚本** 在不插线状态下捕获寄存器快照

## 日志要点

- 用统一前缀如 `[STAT]` 方便 `dmesg | grep` 过滤
- 写入后立即读回硬件寄存器验证（`i2c_smbus_read_byte_data` 绕过 regmap 缓存）
- 日志里打印前后值差异：`hwREG07 0x8d->0xcd bit6=0->1`
- 定位完毕后移除日志，保持生产代码干净

## 串口桥接

```
PC --SSH--> RK3568 --USB-CH340--> RK3576 UART
              |
         socat TCP:2000 --> /dev/ttyUSB3
```

关键脚本：`~/.claude/skills/embedded-debug/scripts/ssh-bridge/send_cmd.sh`

## 延迟采样（不插线调试）

```bash
cat > /data/check_stat.sh << 'EOF'
sleep 10
for r in 07 0b 0c 10; do
  echo REG$r=$(i2cget -f -y 6 0x6a 0x$r) >> /data/stat.log
done
EOF
/data/check_stat.sh &  # 运行后立刻拔USB，10s后采样
```

## 案例

BQ25890 STAT_DIS 调试中用到了以上全部方法：
- 日志定位到驱动读 REG0C 触发故障闪烁循环
- 延迟采样验证不插线时 REG07=0xcd 已写入
- 串口桥接在拔线状态下远程读寄存器和 dmesg
