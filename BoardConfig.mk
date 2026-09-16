#
# Copyright (C) 2026 The OnePlus Pad TWRP Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Building with minimal manifest
DEVICE_PATH                                  := device/oplus/opd2407
ALLOW_MISSING_DEPENDENCIES                   := true
BUILD_BROKEN_DUP_RULES                       := true
BUILD_BROKEN_ELF_PREBUILT_PRODUCT_COPY_FILES := true
BUILD_BROKEN_PLUGIN_VALIDATION               := soong-libaosprecovery_defaults soong-libguitwrp_defaults soong-libminuitwrp_defaults soong-vold_defaults

# Architecture
TARGET_ARCH         := arm64
TARGET_ARCH_VARIANT := armv8-2a
TARGET_CPU_ABI      := arm64-v8a
TARGET_CPU_VARIANT  := cortex-a55

# A/B
AB_OTA_PARTITIONS := \
    boot \
    init_boot \
    vendor_boot \
    dtbo \
    odm \
    product \
    system \
    system_ext \
    system_dlkm \
    vbmeta \
    vbmeta_system \
    vbmeta_vendor \
    vendor \
    vendor_dlkm

# A/B partitions for oplus
AB_OTA_PARTITIONS += \
    my_bigball \
    my_carrier \
    my_engineering \
    my_heytap \
    my_manifest \
    my_preload \
    my_product \
    my_region \
    my_stock

# MTK platform
BOARD_USES_MTK_HARDWARE := true
BOARD_HAS_MTK_HARDWARE  := true
TARGET_BOARD_PLATFORM   := mt6897
BOARD_VENDOR            := oplus
TARGET_BOOTLOADER_BOARD_NAME := mt6897

# Crypto
BOARD_USES_METADATA_PARTITION := true
TW_INCLUDE_CRYPTO             := true
TW_INCLUDE_CRYPTO_FBE         := true
TW_INCLUDE_OMAPI              := true

# File systems
TARGET_USERIMAGES_USE_F2FS := true
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USES_MKE2FS         := true
TW_USE_DMCTL               := true
TW_INCLUDE_FUSE_EXFAT      := true
TW_INCLUDE_FUSE_NTFS       := true
TW_INCLUDE_NTFS_3G         := true
TW_NO_EXFAT_FUSE           := true

# Kernel (prebuilt from stock boot/vendor_boot)
BOARD_KERNEL_IMAGE_NAME     := Image
TARGET_PREBUILT_KERNEL      := $(DEVICE_PATH)/prebuilt/Image
TARGET_PREBUILT_DTB         := $(DEVICE_PATH)/prebuilt/dtb.img
BOARD_BOOT_HEADER_VERSION   := 4
BOARD_KERNEL_PAGESIZE       := 4096
BOARD_MKBOOTIMG_ARGS        += --header_version $(BOARD_BOOT_HEADER_VERSION)
BOARD_MKBOOTIMG_ARGS        += --pagesize $(BOARD_KERNEL_PAGESIZE)
BOARD_RAMDISK_USE_LZ4       := true

# Partitions
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 0x8000000
BOARD_BOOTIMAGE_PARTITION_SIZE     := 0x4000000
BOARD_INIT_BOOT_IMAGE_PARTITION_SIZE := 0x800000
BOARD_DTBOIMG_PARTITION_SIZE       := 0x200000
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 0x4000000

# Recovery
BOARD_EXCLUDE_KERNEL_FROM_RECOVERY_IMAGE := true
TARGET_RECOVERY_PIXEL_FORMAT            := RGBX_8888
TW_INCLUDE_FASTBOOTD                    := true
TW_INCLUDE_REPACKTOOLS                  := true
TW_INCLUDE_RESETPROP                    := true
TW_USE_TOOLBOX                          := true
TW_INCLUDE_ZSTD                         := true

# TWRP display
TW_BRIGHTNESS_PATH      := /sys/class/leds/lcd-backlight/brightness
TW_DEFAULT_BRIGHTNESS   := 400
TW_MAX_BRIGHTNESS       := 2047
TW_FRAMERATE            := 144
TW_SCREEN_BLANK_ON_BOOT := true
TW_ROTATION             := 0
TW_THEME                := landscape_hdpi

# TWRP file system
RECOVERY_SDCARD_ON_DATA := true
TW_ENABLE_FS_COMPRESSION := true

# Version
PLATFORM_VERSION             := 99.87.36
PLATFORM_VERSION_LAST_STABLE := $(PLATFORM_VERSION)
PLATFORM_SECURITY_PATCH      := 2099-12-31
VENDOR_SECURITY_PATCH        := $(PLATFORM_SECURITY_PATCH)
TW_DEVICE_VERSION            := OPD2407

# Verified Boot
BOARD_AVB_ENABLE := true

# Misc TWRP configuration
TW_EXCLUDE_APEX                         := true
TW_EXCLUDE_DEFAULT_USB_INIT             := true
TW_EXTRA_LANGUAGES                      := true
TW_NO_SCREEN_BLANK                      := true
TW_SKIP_ADDITIONAL_FSTAB                := true
TW_USE_SERIALNO_PROPERTY_FOR_DEVICE_ID  := true
