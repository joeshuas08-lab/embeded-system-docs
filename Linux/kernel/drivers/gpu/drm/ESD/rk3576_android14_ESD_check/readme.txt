本补丁，适用于rk3576 rk3588 MIPI-DSI2.
ESD-check：用于轮询的读屏端寄存器状态。若读出来的值不符合预期 则复位当前显示链路。  


1、补丁说明
    1) 合上如下补丁
patchs/
├── kernel-6.1
│   └── 0001-display-ESD-check-verify-that-The-MIPI-DSI-polling-s.patch
└── u-boot
    └── 0001-u-boot-display-ESD-check-verify-that-The-MIPI-DSI-po.patch

    2）SDK进入到对应的补丁目录下，输入如下命令可自动合入补丁 

      eg：  ~/SDK/kernel$  patch -p1 <  0001-display-ESD-check-verify-that-The-MIPI-DSI-polling-s.patch
               ~/SDK/uboot$  patch -p1 <  0001-u-boot-display-ESD-check-verify-that-The-MIPI-DSI-po.patch

    3)若出现补丁合入不上的情况，可自行对比查看源文件。
src/
├── kernel-6.1
│   └── drivers
│       └── gpu
│           └── drm
│               ├── panel
│               │   └── panel-simple.c
│               └── rockchip
│                   └── dw-mipi-dsi2-rockchip.c
└── u-boot
    └── drivers
        └── video
            └── drm
                └── dw_mipi_dsi2.c


2、使用说明  

  1) rk3588 rk3576  HSS同步信号会强制在hsync时期发送 ，故mipi读命令 必须 在一行的时间内完成，否则回读会出现异常。 

  2)  esd-check 默认读的屏端寄存器地址：0x0a 预期正常状态值为：0x9c。 
            具体见补丁中的如下函数：
	          err = mipi_dsi_dcs_get_power_mode(dsi, &value);
	          bool normal = (esd_check_value_read(p) == 0x9c) ? true : false;

   3) 如果想读其他的屏端寄存器地址可以使用如下函数，并参考 上述函数进行修改。 
           mipi_dsi_generic_read() @ kernel-6.1/drivers/gpu/drm/drm_mipi_dsi.c

   4)补丁中的 test_gpio 主要是用于示波器通过test_gpio沿变，抓取异常时的mipi读命令波形。 方便观察问题， 并无实质用处。

 






