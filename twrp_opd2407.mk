#
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/oplus/opd2407

# Inherit from device.mk configuration
$(call inherit-product, $(DEVICE_PATH)/device.mk)

## Device identifier
PRODUCT_DEVICE  := opd2407
PRODUCT_NAME    := twrp_opd2407
PRODUCT_BRAND   := OnePlus
PRODUCT_MANUFACTURER := OnePlus
PRODUCT_MODEL   := OPD2407

# Theme offsets (tablet landscape)
TW_STATUS_ICONS_ALIGN := center
TW_Y_OFFSET           := 0
TW_H_OFFSET           := 0
