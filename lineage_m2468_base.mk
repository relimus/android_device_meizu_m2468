$(call inherit-product, vendor/lineage/build/target/product/lineage_gsi_arm64.mk)

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

PRODUCT_ENFORCE_ARTIFACT_PATH_REQUIREMENTS :=