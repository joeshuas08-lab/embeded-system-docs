
issue：
![](TELEC-5.3G--(1)%201.docx)
solution：
![](nvram_ap6256.txt)

1.无需重新编译整个 Android 固件，直接使用adb 覆盖开发板上的运行文件即可快速查看频谱仪结果：

将开发板通过usb连接到电脑并获取权限 (请确保系统开启了ADB权限)：

adb root
adb remount

2\.将您电脑上修改好的 nvram\_ap6256.txt 推送到开发板上：

adb push nvram\_ap6256.txt /vendor/firmware/nvram\_ap6256.txt

adb push nvram\_ap6256.txt /recovery/root/vendor/etc/firmware/nvram\_ap6256.txt

sync

3\.重启 Wi-Fi 以重新加载参数：
在开发板的系统设置中将 Wi-Fi 开关“关闭后重新开启”，
或直接重启开发板：
adb reboot

4： 在实验室中反复调试并调整，使板子重新发射 5260MHz（ac20 或 a 模式），观察频谱仪上的 Deviation (ppm) ：
如果数值下降了，但仍在 +10 ppm 左右：说明力度不足，请继续加大参数，改为 xtal\_cap=22 。 如果数值下降过多，变成 -15 ppm：说明电容值过大，请将参数调小，改为 xtal\_cap=14。
理想目标：将 xtal\_cap 调整到使频谱仪显示在 ±3 ppm 以内，这是最安全的黄金区间，这样无论冷机还是热机都能稳定通过 TELEC 测试。

