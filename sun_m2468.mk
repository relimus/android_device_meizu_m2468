# Inherit from products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit generic system.
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_system.mk)

# Inherit some common SunOS stuff.
$(call inherit-product, vendor/sun/config/common_full_phone.mk)

$(call inherit-product, vendor/sun/config/BoardConfigSoong.mk)
$(call inherit-product, vendor/sun/config/BoardConfigSun.mk)

$(call inherit-product, device/meizu/m2468/device.mk)

## Device identifier
PRODUCT_BRAND := Meizu
PRODUCT_DEVICE := m2468
PRODUCT_MANUFACTURER := Meizu
PRODUCT_NAME := lineage_m2468
PRODUCT_MODEL := Meizu 21 Note

PRODUCT_CHARACTERISTICS := device

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildDesc="qssi-user 15 AQ3A.250129.001 1737086932 release-keys" \
    BuildFingerprint="meizu/Meizu_21Note_CN/Meizu21Note:15/AQ3A.241229.001/1737086932:user/release-keys"

PRODUCT_GMS_CLIENTID_BASE := android-meizu