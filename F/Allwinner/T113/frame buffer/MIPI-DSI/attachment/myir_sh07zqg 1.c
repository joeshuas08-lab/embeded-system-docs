/*
 * drivers/video/sunxi/disp2/disp/lcd/JD9365DA_SAT080BO31I21Y03_26114M018IB.c
 *
 * Copyright (c) 2007-2018 Allwinnertech Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
//#include "JD9365DA_CC10127007_31AB.h"
//#include <mach/sys_config.h>
#include "panels.h"
#include "myir_sh07zqg.h"
/*
&lcd0 {
	lcd_used            = <1>;
	status              = "okay";
	lcd_driver_name     = "JD9365DA_CC10127007_31AB";
	lcd_backlight       = <50>;
	lcd_if              = <4>;

	lcd_x               = <800>;
	lcd_y               = <1280>;
	lcd_width           = <135>;
	lcd_height          = <216>;
	lcd_dclk_freq       = <75>;

	lcd_pwm_used        = <1>;
	lcd_pwm_ch          = <0>;
	lcd_pwm_freq        = <50000>;
	lcd_pwm_pol         = <1>;
	lcd_pwm_max_limit   = <255>;

	lcd_hbp             = <88>;
	lcd_ht              = <960>;
	lcd_hspw            = <4>;
	lcd_vbp             = <12>;
	lcd_vt              = <1300>;
	lcd_vspw            = <4>;

	lcd_frm             = <0>;
	lcd_gamma_en        = <0>;
	lcd_bright_curve_en = <0>;
	lcd_cmap_en         = <0>;

	deu_mode            = <0>;
	lcdgamma4iep        = <22>;
	smart_color         = <90>;

	lcd_dsi_if          = <0>;
	lcd_dsi_lane        = <4>;
	lcd_dsi_format      = <0>;
	lcd_dsi_te          = <0>;
	lcd_dsi_eotp        = <0>;

	lcd_pin_power = "dcdc1";
	lcd_pin_power1 = "eldo3";

	lcd_power = "eldo3";

	lcd_power1 = "dcdc1";
	lcd_power2 = "dc1sw";

	lcd_gpio_1 = <&pio PD 22 1 0 3 1>;

	pinctrl-0 = <&dsi4lane_pins_a>;
	pinctrl-1 = <&dsi4lane_pins_b>;

	lcd_bl_en = <&pio PB 8 1 0 3 1>;
	lcd_bl_0_percent	= <15>;
	lcd_bl_100_percent  = <100>;
};
*/

extern s32 bsp_disp_get_panel_info(u32 screen_id, struct disp_panel_para *info);
static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);
 
#define panel_reset(val) sunxi_lcd_gpio_set_value(sel, 2, val)
 
static void lcd_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		{0, 0},     {15, 15},   {30, 30},   {45, 45},   {60, 60},
		{75, 75},   {90, 90},   {105, 105}, {120, 120}, {135, 135},
		{150, 150}, {165, 165}, {180, 180}, {195, 195}, {210, 210},
		{225, 225}, {240, 240}, {255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
		{
			{LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
			{LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
			{LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
		},
		{
			{LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
			{LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
			{LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
		},
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value =
				lcd_gamma_tbl[i][1] +
				((lcd_gamma_tbl[i + 1][1] - lcd_gamma_tbl[i][1]) *
				j) /
				num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
				(value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
		(lcd_gamma_tbl[items - 1][1] << 8) +
		lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));
}
 

static s32 lcd_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, lcd_power_on, 30);
	LCD_OPEN_FUNC(sel, lcd_panel_init, 30);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 100);
	LCD_OPEN_FUNC(sel, lcd_bl_open, 0);
	return 0;
}
 
static s32 lcd_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
	LCD_CLOSE_FUNC(sel, lcd_panel_exit, 10);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, lcd_power_off, 0);

	return 0;
}
 
 static void lcd_power_on(u32 sel)
 {
 
	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_pin_cfg(sel, 1);
	sunxi_lcd_delay_ms(10);
 
	 /* open power by gpio */
	//  sunxi_lcd_gpio_set_direction(sel, 1, 1);
	//  sunxi_lcd_gpio_set_value(sel, 1, 1);
 
	 /* reset lcd by gpio */
	panel_reset(1);
	sunxi_lcd_delay_ms(10);
	panel_reset(0);
	sunxi_lcd_delay_ms(50);
	panel_reset(1);
	sunxi_lcd_delay_ms(100);
 
 }
 
 static void lcd_power_off(u32 sel)
 {
	 sunxi_lcd_pin_cfg(sel, 0);
	 sunxi_lcd_delay_ms(1);
	 panel_reset(0);
	//  sunxi_lcd_delay_ms(1);
	//  sunxi_lcd_gpio_set_value(sel, 1, 0);
	 //	sunxi_lcd_power_disable(sel, 0);
 }
 
 static void lcd_bl_open(u32 sel)
 {
	 /* open power by gpio */
	//  sunxi_lcd_gpio_set_direction(sel, 1, 1);
	//  sunxi_lcd_gpio_set_value(sel, 1, 1);
 
	 	sunxi_lcd_pwm_enable(sel);
	 	sunxi_lcd_backlight_enable(sel);
 }
 
 static void lcd_bl_close(u32 sel)
 {
	 /* open power by gpio */
	//  sunxi_lcd_gpio_set_direction(sel, 1, 1);
	//  sunxi_lcd_gpio_set_value(sel, 1, 0);
 
	 	sunxi_lcd_backlight_disable(sel);
	 	sunxi_lcd_pwm_disable(sel);
 }
 
 #define REGFLAG_DELAY         0XFE
 #define REGFLAG_END_OF_TABLE  0xFC   /* END OF REGISTERS MARKER */
 
 struct LCM_setting_table {
	 u8 cmd;
	 u32 count;
	 u8 para_list[52];
 };
 
 #if 1
 static struct LCM_setting_table lcm_initialization_setting[] = {


	{0x80,0x01,{0x8B}},
	{0x81,0x01,{0x78}},
	{0x82,0x01,{0x84}},
	{0x83,0x01,{0x88}},
	{0x84,0x01,{0xA8}},
	{0x85,0x01,{0xE3}},
	{0x86,0x01,{0x88}},
	
	// {0x87,0x01,{0x5A}},
	// {0xB0,0x01,{0x80}},
	// {0xB1,0x01,{0x30}},/*0x38自测*/
	// {0xB2,0x01,{0x40}},
 
	{0x11, 1, {0} },
	{REGFLAG_DELAY, 120, {0} },
 
	{0x29, 1, {0} },
	{REGFLAG_DELAY, 10, {0} },
 
	{REGFLAG_END_OF_TABLE, 0x00, {0} }
 };
 #endif
 
static void lcd_panel_init(u32 sel)
{
	u32 i = 0;

	// u8 result;
	// u32 data;
	// u32 num=0;
 
	sunxi_lcd_dsi_clk_enable(sel);
	sunxi_lcd_delay_ms(10);

	// sunxi_lcd_dsi_dcs_write_1para(sel, 0x80,0x8B);
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0x81,0x78);
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0x82,0x84);
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0x83,0x88);
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0x84,0xA8);
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0x85,0xE3);
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0x86,0x88);

	// sunxi_lcd_dsi_dcs_write_1para(sel, 0x87,0x5A);
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0xB0,0x80);
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0xB1,0x38);//38-自测
	// sunxi_lcd_dsi_dcs_write_1para(sel, 0xB2,0x40);//40-4lane，50-2lane，60-3lane

	// sunxi_lcd_dsi_dcs_write_0para(sel, 0x11);
	// sunxi_lcd_delay_ms(120);
	// sunxi_lcd_dsi_dcs_write_0para(sel, 0x29);
	// sunxi_lcd_delay_ms(5);
 
	for (i = 0;; i++) {
		if (lcm_initialization_setting[i].cmd == REGFLAG_END_OF_TABLE)
			break;
		else if (lcm_initialization_setting[i].cmd == REGFLAG_DELAY)
			sunxi_lcd_delay_ms(lcm_initialization_setting[i].count);
		else {
		dsi_dcs_wr(0, lcm_initialization_setting[i].cmd,
					lcm_initialization_setting[i].para_list,
					lcm_initialization_setting[i].count);
		}
	}

}
 
static void lcd_panel_exit(u32 sel)
{
sunxi_lcd_dsi_dcs_write_0para(sel, DSI_DCS_SET_DISPLAY_OFF);
sunxi_lcd_delay_ms(10);
sunxi_lcd_dsi_dcs_write_0para(sel, DSI_DCS_ENTER_SLEEP_MODE);
sunxi_lcd_delay_ms(10);
}
 
/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}
 

struct __lcd_panel myir_sh07zqg_mipi_panel = {
	/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex */
	.name = "myir_sh07zqg",
	.func = {
		.cfg_panel_info =lcd_cfg_panel_info,
		.cfg_open_flow = lcd_open_flow,
		.cfg_close_flow = lcd_close_flow,
		.lcd_user_defined_func = lcd_user_defined_func,
		//.set_bright = LCD_set_bright,
	},
};
