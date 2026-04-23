# BQ25890 充电芯片驱动 —— 寄存器值与物理量映射机制详解

> 源文件：`kernel-6.1/drivers/power/supply/bq25890_charger.c`
>
> 适用芯片：BQ25890 / BQ25892 / BQ25895 / BQ25896 / SY6970

---

## 一、驱动整体架构

```
Device Tree (物理量 uA/uV)
        │
        ▼
 bq25890_find_idx()  ←── 物理量 → 寄存器索引
        │
        ▼
  regmap_field_write()  ←── 通过 I2C 写入硬件寄存器
        │
        ▼
    BQ25890 芯片
        │
        ▼
  regmap_field_read()   ←── 通过 I2C 读取硬件寄存器
        │
        ▼
 bq25890_find_val()  ←── 寄存器索引 → 物理量
        │
        ▼
  power_supply 框架 (返回给用户态)
```

驱动使用 Linux `regmap` 框架管理 I2C 寄存器，通过 `regmap_field` 实现按位域（bit-field）读写，避免手动位操作。

---

## 二、寄存器字段定义

### 2.1 IINLIM 字段

```c
[F_IINLIM] = REG_FIELD(0x00, 0, 5),
```

| 属性       | 值         |
|-----------|-----------|
| 寄存器地址  | 0x00      |
| 位域范围   | bit[5:0]  |
| 位宽       | 6 bit     |
| 取值范围   | 0 ~ 63    |

### 2.2 REG00 完整布局

```
REG00 (0x00):
  bit[7]   : EN_HIZ    - 高阻态使能
  bit[6]   : EN_ILIM   - ILIM 引脚使能
  bit[5:0] : IINLIM    - 输入电流限制
```

---

## 三、值映射机制

### 3.1 两种映射策略

驱动使用一个 union 数组同时容纳两种映射方式：

| 策略                          | 结构体              | 适用字段                          | 原理             |
|------------------------------|--------------------|---------------------------------|-----------------|
| **Range Table**（线性范围表）  | `bq25890_range`    | IINLIM, ICHG, ITERM, VREG 等    | `val = min + idx × step` |
| **Lookup Table**（查找表）     | `bq25890_lookup`   | TREG, BOOSTI, TSPCT             | 直接查表，非线性   |

### 3.2 Range Table 结构

```c
struct bq25890_range {
    u32 min;    // 最小物理量值
    u32 max;    // 最大物理量值
    u32 step;   // 每个索引的步进值
};
```

### 3.3 IINLIM 的 Range Table

```c
[TBL_IINLIM] = { .rt = {100000, 3250000, 50000} },  /* uA */
```

| 参数   | 值        | 含义                    |
|--------|----------|------------------------|
| `min`  | 100000   | 最小 100mA (100000 uA) |
| `max`  | 3250000  | 最大 3250mA (3250000 uA)|
| `step` | 50000    | 步进 50mA (50000 uA)    |

**映射公式：**

```
电流值 (uA) = 100000 + idx × 50000
idx = (电流值 - 100000) / 50000
```

---

## 四、查阅Datasheet+对照Calculator->得到IINLIM 完整映射表
![](Pasted%20image%2020260302185344.png)
![](Pasted%20image%2020260302185503.png)

| 寄存器值 (idx) | 十六进制 | 电流 (mA) | 电流 (uA)  |
|:-------------:|:-------:|:---------:|:---------:|
| 0             | 0x00    | 100       | 100000    |
| 1             | 0x01    | 150       | 150000    |
| 2             | 0x02    | 200       | 200000    |
| 3             | 0x03    | 250       | 250000    |
| 4             | 0x04    | 300       | 300000    |
| 5             | 0x05    | 350       | 350000    |
| 6             | 0x06    | 400       | 400000    |
| 7             | 0x07    | 450       | 450000    |
| 8             | 0x08    | 500       | 500000    |
| 9             | 0x09    | 550       | 550000    |
| 10            | 0x0A    | 600       | 600000    |
| 11            | 0x0B    | 650       | 650000    |
| 12            | 0x0C    | 700       | 700000    |
| 13            | 0x0D    | 750       | 750000    |
| 14            | 0x0E    | 800       | 800000    |
| 15            | 0x0F    | 850       | 850000    |
| 16            | 0x10    | 900       | 900000    |
| 17            | 0x11    | 950       | 950000    |
| 18            | 0x12    | 1000      | 1000000   |
| 19            | 0x13    | 1050      | 1050000   |
| 20            | 0x14    | 1100      | 1100000   |
| 21            | 0x15    | 1150      | 1150000   |
| 22            | 0x16    | 1200      | 1200000   |
| 23            | 0x17    | 1250      | 1250000   |
| 24            | 0x18    | 1300      | 1300000   |
| 25            | 0x19    | 1350      | 1350000   |
| 26            | 0x1A    | 1400      | 1400000   |
| 27            | 0x1B    | 1450      | 1450000   |
| 28            | 0x1C    | 1500      | 1500000   |
| 29            | 0x1D    | 1550      | 1550000   |
| 30            | 0x1E    | 1600      | 1600000   |
| **31**        | **0x1F**| **1650**  | **1650000**|
| 32            | 0x20    | 1700      | 1700000   |
| 33            | 0x21    | 1750      | 1750000   |
| 34            | 0x22    | 1800      | 1800000   |
| 35            | 0x23    | 1850      | 1850000   |
| 36            | 0x24    | 1900      | 1900000   |
| 37            | 0x25    | 1950      | 1950000   |
| 38            | 0x26    | 2000      | 2000000   |
| 39            | 0x27    | 2050      | 2050000   |
| 40            | 0x28    | 2100      | 2100000   |
| 41            | 0x29    | 2150      | 2150000   |
| 42            | 0x2A    | 2200      | 2200000   |
| 43            | 0x2B    | 2250      | 2250000   |
| 44            | 0x2C    | 2300      | 2300000   |
| 45            | 0x2D    | 2350      | 2350000   |
| 46            | 0x2E    | 2400      | 2400000   |
| 47            | 0x2F    | 2450      | 2450000   |
| 48            | 0x30    | 2500      | 2500000   |
| 49            | 0x31    | 2550      | 2550000   |
| 50            | 0x32    | 2600      | 2600000   |
| 51            | 0x33    | 2650      | 2650000   |
| 52            | 0x34    | 2700      | 2700000   |
| 53            | 0x35    | 2750      | 2750000   |
| 54            | 0x36    | 2800      | 2800000   |
| 55            | 0x37    | 2850      | 2850000   |
| 56            | 0x38    | 2900      | 2900000   |
| 57            | 0x39    | 2950      | 2950000   |
| 58            | 0x3A    | 3000      | 3000000   |
| 59            | 0x3B    | 3050      | 3050000   |
| 60            | 0x3C    | 3100      | 3100000   |
| 61            | 0x3D    | 3150      | 3150000   |
| 62            | 0x3E    | 3200      | 3200000   |
| 63            | 0x3F    | 3250      | 3250000   |

> **注意：** 驱动中 `shutdown` 函数写入 `0x1F`，注释标注 "1.5A"，但实际对应 **1650mA (1.65A)**，注释不准确。

---

## 五、转换函数源码分析

### 5.1 bq25890_find_val —— 寄存器值 → 物理量

```c
static u32 bq25890_find_val(u8 idx, enum bq25890_table_ids id)
{
    const struct bq25890_range *rtbl;

    /* lookup table 走查表路径 */
    if (id >= TBL_TREG)
        return bq25890_tables[id].lt.tbl[idx];

    /* range table 走线性计算路径 */
    rtbl = &bq25890_tables[id].rt;
    return (rtbl->min + idx * rtbl->step);
}
```

**调用示例：**
```
bq25890_find_val(31, TBL_IINLIM)
= 100000 + 31 × 50000
= 1650000 (uA)
= 1650 mA
```

### 5.2 bq25890_find_idx —— 物理量 → 寄存器值

```c
static u8 bq25890_find_idx(u32 value, enum bq25890_table_ids id)
{
    u8 idx;

    if (id >= TBL_TREG) {
        /* lookup table: 顺序查找 */
        const u32 *tbl = bq25890_tables[id].lt.tbl;
        u32 tbl_size = bq25890_tables[id].lt.size;
        for (idx = 1; idx < tbl_size && tbl[idx] <= value; idx++)
            ;
    } else {
        /* range table: 线性搜索 (等价于 (value - min) / step 向下取整) */
        const struct bq25890_range *rtbl = &bq25890_tables[id].rt;
        u8 rtbl_size = (rtbl->max - rtbl->min) / rtbl->step + 1;
        for (idx = 1;
             idx < rtbl_size && (idx * rtbl->step + rtbl->min <= value);
             idx++)
            ;
    }
    return idx - 1;
}
```

**关键行为：** 向下取整（floor），即返回不超过目标值的最大索引。

**调用示例：**
```
bq25890_find_idx(1000000, TBL_IINLIM)
→ (1000000 - 100000) / 50000 = 18
→ 寄存器写入 18 (0x12)
→ 实际电流 = 100 + 18 × 50 = 1000 mA ✓
```

**边界情况示例：**
```
bq25890_find_idx(1020000, TBL_IINLIM)   // 请求 1020mA
→ floor((1020000 - 100000) / 50000) = floor(18.4) = 18
→ 实际设置为 1000mA（向下取整，不会超过请求值）
```

---

## 六、驱动中 IINLIM 的使用场景

### 6.1 用户态读取输入电流限制

```c
case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    ret = bq25890_field_read(bq, F_IINLIM);  // 读寄存器 idx
    val->intval = bq25890_find_val(ret, TBL_IINLIM);  // 转换为 uA
    break;
```

用户通过 sysfs 节点读取：
```bash
cat /sys/class/power_supply/bq25890-charger/input_current_limit
# 输出单位为 uA，例如 1650000 表示 1650mA
```

### 6.2 用户态设置输入电流限制

```c
case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    lval = bq25890_find_idx(val->intval, TBL_IINLIM);  // uA → idx
    return bq25890_field_write(bq, F_IINLIM, lval);     // 写入寄存器
```

用户通过 sysfs 节点写入：
```bash
echo 1000000 > /sys/class/power_supply/bq25890-charger/input_current_limit
# 设置为 1000mA
```

### 6.3 USB 类型检测后自动设置

```c
case POWER_SUPPLY_USB_TYPE_DCP:
    input_current_limit = bq25890_find_idx(1000000, TBL_IINLIM);  // DCP: 1A
    break;
case POWER_SUPPLY_USB_TYPE_SDP:
default:
    input_current_limit = bq25890_find_idx(500000, TBL_IINLIM);   // SDP: 500mA
```

### 6.4 PD 充电协商后设置

```c
static void bq25890_set_pd_param(struct bq25890_device *bq, int vol, int cur)
{
    if (cur > 1000000)
        cur = 1000000;                            // 限制最大 1A
    iilim = bq25890_find_idx(cur, TBL_IINLIM);   // 转换
    bq25890_field_write(bq, F_IINLIM, iilim);    // 写入
}
```

### 6.5 关机时设置默认值

```c
static void bq25890_shutdown(struct i2c_client *client)
{
    regmap_field_write(bq->rmap_fields[F_IINLIM], 0x1F);  // idx=31 → 1650mA
    regmap_field_write(bq->rmap_fields[F_ICHG], 0x3F);    // idx=63 → 4032mA
}
```

---

## 七、其他常用字段映射表（供参考）

### 7.1 ICHG — 充电电流 (REG04 bit[6:0])

```c
[TBL_ICHG] = { .rt = {0, 5056000, 64000} },  /* uA */
```

`电流(uA) = 0 + idx × 64000`，步进 64mA，范围 0 ~ 5056mA。

| 寄存器值 | 电流 (mA) |
|:--------:|:---------:|
| 0x00     | 0         |
| 0x08     | 512       |
| 0x10     | 1024      |
| 0x20     | 2048      |
| 0x3F     | 4032      |
| 0x4F     | 5056      |

### 7.2 VREG — 充电电压 (REG06 bit[7:2])

```c
[TBL_VREG] = { .rt = {3840000, 4608000, 16000} },  /* uV */
```

`电压(uV) = 3840000 + idx × 16000`，步进 16mV，范围 3.84V ~ 4.608V。

### 7.3 ITERM — 终止电流 (REG05 bit[3:0])

```c
[TBL_ITERM] = { .rt = {64000, 1024000, 64000} },  /* uA */
```

`电流(uA) = 64000 + idx × 64000`，步进 64mA，范围 64mA ~ 1024mA。

---

## 八、调试技巧

### 8.1 通过 I2C 直接读取寄存器

```bash
# 读取 REG00（包含 IINLIM）
i2cget -f -y <bus> <addr> 0x00

# 示例：bus=6, addr=0x6a
i2cget -f -y 6 0x6a 0x00
# 返回例如 0x5F → bit[5:0] = 0x1F = 31 → 1650mA
#                  bit[6] = 1 (EN_ILIM enabled)
#                  bit[7] = 0 (EN_HIZ disabled)
```

### 8.2 解析寄存器值

```bash
# 假设读到 REG00 = 0x52 (二进制 0101_0010)
# bit[7]   = 0 → EN_HIZ = disabled
# bit[6]   = 1 → EN_ILIM = enabled
# bit[5:0] = 010010 = 18 → 100 + 18×50 = 1000mA
```

### 8.3 通过 sysfs 查看

```bash
# 查看当前输入电流限制
cat /sys/class/power_supply/bq25890-charger/input_current_limit

# 查看所有属性
ls /sys/class/power_supply/bq25890-charger/
```
