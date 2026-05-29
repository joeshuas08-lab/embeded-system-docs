# SEAndroid 权限修复：调试目录访问与 dlopen 执行权限

## 问题现象

- system_app 调用 dlopen 加载 `/data/vendor/debug/` 下的 so 失败
- log 报 avc denied
- APK 无法执行 /data/vendor/debug/ 下的二进制

## 排查路径

```
dlopen 失败
→ logcat 搜 avc （别搜 dmesg，app 的 avc 在 logcat）
→ 看 denied 的 scontext/tcontext/tclass

常见：
  denied { execute } for tclass=file
  → file.te 缺少 execute 权限

  denied { ioctl } for tclass=file cmd=9409
  → 需要 allowxperm
```

## 根因分析

### 1. dlopen 需要 execute 权限

SELinux 默认策略下，`system_app` 域对 `data_file_type` 只有 `{ read write }` 权限。但要 dlopen 一个 so，还需要 `{ execute execute_no_trans map }`。

**修复** — `file.te` 中为 `vendor_storage_data_file` 添加：
```
allow system_app vendor_storage_data_file:file {
    execute execute_no_trans map     # dlopen 需要
    create open read write getattr   # 基础访问
    setattr unlink rename            # 文件管理
    ioctl lock                       # binder/ashmem
};
allowxperm system_app vendor_storage_data_file:file ioctl { 0x9409 };
```

**关于 0x9409**：这是 `BINDER_IOCTL_GET_NODE_INFO_FOR_REF` 的 ioctl 命令号。system_app 如果没有这个特定 ioctl 的 allowxperm，调用 binder 时会触发 `denied { ioctl }`，即使 file 权限里给了 `ioctl`，SELinux 还会进一步检查 ioctl 的具体命令号。

### 2. directory setattr

```
allow system_app vendor_storage_data_file:dir { ... setattr ... };
```
没有 `setattr` 时，chmod/chown 目录会静默失败，表现为 APK 在目录下创建文件后无法读写。

### 3. 清理残留 debug rc 文件

`init.myir_debug.rc` 是调试过程中创建的，在 `init.rk30board.rc` 中被 import。清理后需要同步删除 import 语句和 `BoardConfigVendor.mk` 中的 `PRODUCT_COPY_FILES`，否则编译报错。

## avc 日志速查

```bash
# 实时监控 avc
adb logcat -b events | grep avc
# 或
adb shell dmesg | grep avc  # kernel 侧 avc

# 解读 avc log
# avc: denied { execute } for pid=1234 comm="app_process"
#   scontext=u:r:system_app:s0
#   tcontext=u:object_r:vendor_storage_data_file:s0
#   tclass=file
#   → system_app 对 vendor_storage_data_file 类型的文件缺少 execute 权限
```

## 验证清单

```bash
# 1. avc 清零
logcat -b events | grep avc | wc -l   # 应为 0 或少量预期拒绝

# 2. dlopen 验证
# APK 调用 System.loadLibrary 从 /data/vendor/debug/ 加载 so

# 3. 目录权限
ls -laZ /data/vendor/debug/
# context 应为 u:object_r:vendor_storage_data_file:s0

# 4. 清理确认
ls vendor/rockchip/common/init.myir_debug.rc   # 不应存在
grep -r "init.myir_debug" device/rockchip/common/  # 无引用
```

## 失败模式速查

| 症状 | avc 信息 | 修复方向 |
|------|---------|---------|
| dlopen 失败 | denied { execute } | add execute to file.te |
| ioctl crash | denied { ioctl } cmd=9409 | add allowxperm |
| chmod 目录失败 | denied { setattr } for dir | add setattr to dir |
| rename 文件失败 | denied { rename } | add rename to file |
| 读目录失败 | denied { search } for dir | add search to dir |

## Commit 索引

- `e1ff1984aa5` — 创建 /data/vendor/crysound + debug 目录 + file_contexts + file.te 基础权限
- `4a91ccbb071` — 追加 execute/ioctl/rename/setattr 权限 + allowxperm 0x9409 + app.te neverallow 例外
- `a560ebb4ed6` — 删除 init.myir_debug.rc 残留 + BoardConfigVendor.mk 清理
