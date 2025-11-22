#
# Copyright (C) 2025 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit from m2468 device
$(call inherit-product, device/meizu/m2468/device.mk)

PRODUCT_DEVICE := m2468
PRODUCT_NAME := lineage_m2468
PRODUCT_BRAND := Meizu
PRODUCT_MODEL := MEIZU 21 Note
PRODUCT_MANUFACTURER := Meizu

PRODUCT_GMS_CLIENTID_BASE := android-meizu

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRIVATE_BUILD_DESC="qssi-user 15 AQ3A.250129.001 1737086932 release-keys"

BUILD_FINGERPRINT := meizu/Meizu_21Note_CN/Meizu21Note:15/AQ3A.241229.001/1737086932:user/release-keys
