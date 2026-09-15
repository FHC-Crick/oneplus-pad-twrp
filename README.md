# TWRP for OnePlus Pad 3 CN (OPD2407)

自编译 TWRP 设备树,适配一加平板 3 国行(OPD2407 / OP615AL1,MTK 天玑 8350 / mt6897,Android 15 ColorOS)。

## 状态

- [x] 调研与路线定案(fastboot boot 被 lk 砍,走 init_boot 通道)
- [x] 提取官方 boot/vendor_boot/init_boot,kernel + dtb prebuilt
- [ ] 云编跑通
- [ ] 真机验证

## 构建(GitHub Actions)

`.github/workflows/build.yml`,手动触发。产物:
- `TWRP-init_boot.img`:TWRP ramdisk + 官方 header 参数,`fastboot flash init_boot_<slot>` 刷入
- `recovery.img`:无内核的 recovery 镜像(调试用)

## 刷入与回滚

```bash
fastboot flash init_boot_a TWRP-init_boot.img   # 按 current-slot 填 a/b
# 回滚:fastboot flash init_boot_a stock_init_boot.img 或 set_active b
```

## 目录

```
device/oplus/opd2407/
├── BoardConfig.mk      # MTK 平台配置,prebuilt kernel/dtb
├── recovery.fstab      # 官方 fstab.mt6897 改编
└── prebuilt/           # Image(官方内核,35MB)+ dtb.img
tools/parse_boot.py     # 镜像头解析/提取工具
```

参考树:[AuroraRecoveryProject/twrp_device_oplus_ossi](https://github.com/AuroraRecoveryProject/twrp_device_oplus_ossi)
源码树:[TWRP-Test/platform_manifest_twrp_aosp](https://github.com/TWRP-Test/platform_manifest_twrp_aosp) branch `twrp-16.0`
