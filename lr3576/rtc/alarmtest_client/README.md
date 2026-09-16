# AlarmTest 测试工具

用途：验证"AlarmManager 定时唤醒 + echo mem 休眠"链路。

## 组件
- MainActivity：启动设 alarm（`am start ... --es delay 秒数`）
- AlarmSetReceiver：广播设 alarm（`am broadcast -a com.myir.alarmtest.SET --es delay 秒数`）
- AlarmReceiver：alarm 到期时 AlarmManager 自动派发（写 fired.txt 供验证）

## 使用（配合手册）
```sh
adb install AlarmTest.apk
# 设 5 分钟定时唤醒
adb shell am broadcast -n com.myir.alarmtest/.AlarmSetReceiver \
  -a com.myir.alarmtest.SET --es delay 300
# 下发休眠（root）
adb shell "echo mem > /sys/power/state"
```

## 源码
src/ 下为全部源码（3 个 Java 文件 + AndroidManifest.xml），
核心逻辑与产品 app 推荐实现一致：
`AlarmManager.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, ...)`

## 修复记录（2026-09）

**AlarmReceiver 缺 intent-filter**：AlarmSetReceiver 用隐式广播
（action=com.myir.alarmtest.FIRE + setPackage）设置 alarm，但原
AndroidManifest 中 AlarmReceiver 没有声明对应 intent-filter，
导致 alarm 到点后被系统唤醒、广播却找不到接收者 —— fired.txt
永远不写，无法验证 alarm 是否触发。已补上 intent-filter 并重打包。

重打包命令（无需 gradle）：
```sh
JDK17=prebuilts/jdk/jdk17/linux-x86   # d8 需要 Java 17
aapt package -f -M src/AndroidManifest.xml -I prebuilts/sdk/34/system/android.jar -F unsigned.apk
javac -source 8 -target 8 -bootclasspath <android.jar> -d classes src/com/myir/alarmtest/*.java
d8 --lib <android.jar> --min-api 29 --output dex classes/com/myir/alarmtest/*.class
cd dex && aapt add ../unsigned.apk classes.dex
zipalign -f 4 unsigned.apk aligned.apk
# 必须用平台密钥签名（和原版一致）：SCHEDULE_EXACT_ALARM 依赖平台签名自动授予，
# debug 签名装的包该权限 granted=false，setExactAndAllowWhileIdle 会抛 SecurityException
apksigner sign --key build/make/target/product/security/platform.pk8 \
  --cert build/make/target/product/security/platform.x509.pem \
  --min-sdk-version 29 --out AlarmTest.apk aligned.apk
```
（d8/apksigner/zipalign 用 out/host/linux-x86/bin/ 下已编译的版本；平台密钥指纹
SHA-256 2d370c21... 与原版 APK 一致）
