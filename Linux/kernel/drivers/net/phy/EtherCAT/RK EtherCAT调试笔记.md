---
tags:
  - EtherCAT
---
rk3568案例，可用做其他型号的同内核版本的参考;


## 源码：
	见压缩包
## 编译
	编译ethercat所需驱动，进入到RK提供的ethercat_igh源码进行如下操作
```
$ export PATH=/home/eiddie/MYD-LR3568/kekong/MYD-LR3568_L610/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/:$PATH

$ ./bootstrap

$ ./configure --prefix=(输出目录) --host=aarch64-none-linux-gnu --with-linux-dir=SDK/kernel-6.1(SDK内核目录) --enable-8139too=no --enable-stmmac=yes --enable-generic=no --enable-wildcards=yes

$ make

$ make ARCH=arm64 CROSS_COMPILE=~/tool_chain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- modules -j8

$ make install systemdsystemunitdir=（编译后你需要存放的目录，和configure命令上的prefix下跟的参数需要一致）
```
