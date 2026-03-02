
# 1.preview预览:
## 1.1确认/dev下的设备号是media几
```
 media-ctl -d /dev/media* -p | grep -i -A 5 video
 出现什么结果是确认的设备号？有帶(1 pad, 1 link)的media
 但是怎么会有预览节点是video0的呢

###确认设备(AHD摄像头)是否在线
 通过上述media-ctl的打印确认：sensor是否在线 → rkcif 是否 link = ENABLED  
```
## 1.2 确认抓图的节点(这个才是重点,其实可以不用前面那步)
### 首先像ov13855的，出isp的图：
```
ls -l /sys/class/video4linux/* | grep rkisp-vir0
得到节点是video33
```
### 其他如果没走isp的，出cif的图：
```
ls -l /sys/class/video4linux/* | grep rkcif-mipi-lvds1
得到节点是video11
```

## 1.3 确认格式
```
 v4l2-ctl -d /dev/video0 --list-formats-ext 
 videoX根据上一步来；
 ###結果解析:
 (节点只有支持NV12和YUVY才能支持在綫預覽)
 (节点只有GB10的這種RAW格式的不行)
```
## 1.4 预览（Gstreamer，不适用于Android（Android适合v4l2））
```
米尔的自动匹配通用图像格式gst-launch（verified）:
gst-launch-1.0 v4l2src device=/dev/video0 ! 'video/x-raw,width=1920,height=1080,\
framerate=30/1' ! waylandsink
手动指定特定图像格式但自动匹配通用显示框架和兼容低分辨率gst-launch(verified):
gst-launch-1.0 v4l2src device=/dev/video0 ! 'video/x-raw,width=1280,height=720,\
framerate=30/1,format=NV12' ! autovideosink
通用显示框架和通用图像格式gst-launch(verified):
gst-launch-1.0 v4l2src device=/dev/video0 ! videoconvert ! autovideosink
```
# 2.capture拍照:
## 2.1 拍照
```


(unverified)
gst-launch-1.0 \
v4l2src \
device=/dev/video11 \
num-buffers=3 ! \
jpegenc ! \
filesink location=/usr/test_001-1.jpg
```