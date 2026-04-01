---
tags:
  - Display
---
H: AW T113X(S3)
S: Linux 

**Got the datasheet**

![](SH07ZQGI45-07119L（40PIN%20MIPI%209V%20180MA%20规格书)
**Got the initial MIPI command for the REG**
![301](Pasted%20image%2020260330160723.png)


## 1. add a driver in uboot 

### 1.1 put the files under path : brandy/brandy-2.0/u-boot-2018/drivers/video/sunxi/disp2/disp/lcd/

![](myir_sh07zqg.c)

![](myir_sh07zqg.h)

modify panels.c, add lines below:
```
#ifdef CONFIG_LCD_SUPPORT_MYIR_SH07ZQG_1024X600
	&myir_sh07zqg_mipi_panel,
#endif
```


modify panels.h, add lines below:
```
#ifdef CONFIG_LCD_SUPPORT_MYIR_SH07ZQG_1024X600
extern __lcd_panel_t myir_sh07zqg_mipi_panel;
#endif

```


### 1.2 add a new COMPILE CONFIG 

in brandy/brandy-2.0/u-boot-2018/drivers/video/sunxi/disp2/disp/Makefile

```
disp-$(CONFIG_LCD_SUPPORT_MYIR_SH07ZQG_1024X600) += lcd/myir_sh07zqg.o
```

in brandy/brandy-2.0/u-boot-2018/drivers/video/sunxi/disp2/disp/lcd/Kconfig
```
config LCD_SUPPORT_MYIR_SH07ZQG_1024X600
	bool "LCD support LCD_SUPPORT_MYIR_SH07ZQG_1024X600 panel"
	default n
	help
		If you want to support LCD_SUPPORT_MYIR_SH07ZQG_1024X600 panel for display driver, select it.
```

### 1.3 add lcd devicetree, brandy/brandy-2.0/u-boot-2018/arch/arm/dts/myir-t113-mipi.dtsi
![](myir-t113-mipi.dtsi)

## 2. kernel basically same as uboot
2.1 source code for initial 
![](myir_sh07zqg%201.c)

![](myir_sh07zqg%201.h)
2.2 devicetree
![](myir-t113-mipi%201.dtsi)

## 3. add backlight at DTS

```
backlight0: backlight0 {
		compatible = "pwm-backlight";
		status = "okay";
		brightness-levels = <
			0 1 2 3 4 5 6 7
			8 9 10 11 12 13 14 15
			16  17  18  19  20  21  22  23
			24  25  26  27  28  29  30  31
			32  33  34  35  36  37  38  39
			40  41  42  43  44  45  46  47
			48  49  50  51  52  53  54  55
			56  57  58  59  60  61  62  63
			64  65  66  67  68  69  70  71
			72  73  74  75  76  77  78  79
			80  81  82  83  84  85  86  87
			88  89  90  91  92  93  94  95
			96  97  98  99  100 101 102 103
			104 105 106 107 108 109 110 111
			112 113 114 115 116 117 118 119
			120 121 122 123 124 125 126 127
			128 129 130 131 132 133 134 135
			136 137 138 139 140 141 142 143
			144 145 146 147 148 149 150 151
			152 153 154 155 156 157 158 159
			160 161 162 163 164 165 166 167
			168 169 170 171 172 173 174 175
			176 177 178 179 180 181 182 183
			184 185 186 187 188 189 190 191
			192 193 194 195 196 197 198 199
			200 201 202 203 204 205 206 207
			208 209 210 211 212 213 214 215
			216 217 218 219 220 221 222 223
			224 225 226 227 228 229 230 231
			232 233 234 235 236 237 238 239
			240 241 242 243 244 245 246 247
			248 249 250 251 252 253 254 255>;
		default-brightness-level = <200>;
		pwms = <&pwm 0 50000 1>;
	};
};
```

uboot CONFIG enable

```
CONFIG_LCD_SUPPORT_MYIR_SH07ZQG_1024X600=y
```

kernel CONFIG enable

```
CONFIG_BACKLIGHT_CLASS_DEVICE=y
CONFIG_BACKLIGHT_PWM=yCONFIG_LCD_SUPPORT_MYIR_SH07ZQG_1024X600=y
```