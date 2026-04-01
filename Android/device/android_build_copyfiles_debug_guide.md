# Android Build Copy Rules: Quick Debug Guide (RK3576 / Android 14)

## Scope
Use this guide to quickly answer:
- Where does a file under /vendor/etc come from?
- Which mk/bp rule copied it?
- Why can I see the file in out/ but cannot find a literal source:dest line?

Note (中文): /vendor/etc 是安装目标路径，不是固定源码目录。

---

## 1) Core Mental Model
Build-time file install is rule-driven:
1. Product/device rules define install mappings (often via PRODUCT_COPY_FILES).
2. Core make logic validates and expands mappings.
3. Partition variables (for example TARGET_COPY_OUT_VENDOR) decide final destination prefix.
4. Outputs land in out/target/product/<product>/..., then get packed into images.

---

## 2) Key Files You Actually Need
1. Copy rule execution:
- build/make/core/Makefile

2. Low-level copy macros:
- build/make/core/definitions.mk

3. Partition path expansion and defaults:
- build/make/core/board_config.mk

4. Rockchip vendor partition override (RK3576 flow):
-device/rockchip/common/BoardConfig.mk:106 有include Partitions.mk进来
- 
device/rockchip/common/build/rockchip/Partitions.mk

中文：
1.最關鍵的拷貝規則入口
Makefile:20
這裡直接處理 PRODUCT_COPY_FILES，並在 Makefile:82 開始把 src:dest 轉成真正 copy 規則（copy-one-file / copy-xml-file-checked / init script checked 等）。

2.真正執行 copy 的底層宏
definitions.mk:3065 的 copy-one-file
definitions.mk:3115 的 copy-many-files

3.vendor 路徑展開規則（為什麼會到 vendor/etc）
board_config.mk:666 先規範 TARGET_COPY_OUT_VENDOR
board_config.mk:670 把 placeholder 展開到 PRODUCT_COPY_FILES

4.你板子上實際定義拷貝項的地方（例）
device.mk:33
這裡是你產品自己的 PRODUCT_COPY_FILES 定義，最終由上面的 core Makefile 規則執行。

---

## 3) Why vendor vs system/vendor Happens
If not overridden, the build may use system/vendor as a default path in core logic.
Rockchip board config overrides this to vendor in Partitions.mk.

So for RK3576 products, install destination is typically:
- /vendor/...

Note (中文): 结论要以最终 include 链生效结果为准。

---

## 4) Fast Debug Workflow
1. Confirm target path on device/image, for example /vendor/etc/xxx.
2. Confirm final TARGET_COPY_OUT_VENDOR value.
3. Search PRODUCT_COPY_FILES rules in device/ and vendor/.
4. If no explicit result, check dynamic generators:
- find-copy-subdir-files
- foreach/wildcard-generated mappings
- prebuilt_etc in Android.bp
5. Verify final output under out/target/product/<product>/vendor/etc/...

---

## 5) High-Value Commands
Find vendor install rules:
```bash
rg -n "PRODUCT_COPY_FILES|TARGET_COPY_OUT_VENDOR|vendor/etc|:vendor/etc/" device/rockchip vendor/rockchip --glob '*.{mk,bp,xml,rc,conf}'
```

Find final TARGET_COPY_OUT_VENDOR assignments:
```bash
rg -n "TARGET_COPY_OUT_VENDOR\s*[:?+]?=" build device vendor --glob '*.{mk,bp}'
```

Find core copy machinery:
```bash
rg -n "PRODUCT_COPY_FILES|copy-one-file|copy-many-files" build/make/core/Makefile build/make/core/definitions.mk
```

Verify install result in out:
```bash
find out/target/product/rk3576_u/vendor/etc -type f | head
```

---

## 6) Appendix Case: gc05a2 JSON (Real Example)
Target in question:
- /vendor/etc/camera/rkisp2/gc05a2_KYT-11210-V2_default.json

### What happened
You cannot find one explicit mapping line because this file is copied by batch rule expansion, not a single hardcoded source:dest entry.

### Actual rule path
1. device/rockchip/common/modules/camera.mk includes:
- hardware/rockchip/camera/etc/camera_etc.mk

2. In hardware/rockchip/camera/etc/camera_etc.mk (rk3576 branch):
- IQ_FILES_PATH := $(TOP)/external/camera_engine_rkaiq/iqfiles/isp39
- PRODUCT_COPY_FILES += $(call find-copy-subdir-files,*,$(IQ_FILES_PATH)/,$(TARGET_COPY_OUT_VENDOR)/etc/camera/rkisp2/)

This copies all files in isp39 to vendor/etc/camera/rkisp2.

### Minimal trace steps
```bash
find external/camera_engine_rkaiq/iqfiles -name 'gc05a2_KYT-11210-V2_default*.json'
rg -n "find-copy-subdir-files|IQ_FILES_PATH|rkisp2" hardware/rockchip/camera/etc/camera_etc.mk device/rockchip/common/modules/camera.mk
find out/target/product/rk3576_u/vendor/etc/camera/rkisp2 -name 'gc05a2_KYT-11210-V2_default*.json'
```

Expected source:
- external/camera_engine_rkaiq/iqfiles/isp39/gc05a2_KYT-11210-V2_default.json

Expected output:
- out/target/product/rk3576_u/vendor/etc/camera/rkisp2/gc05a2_KYT-11210-V2_default.json

Note (中文): 搜不到完整 destination 字符串时，优先怀疑动态批量规则。
