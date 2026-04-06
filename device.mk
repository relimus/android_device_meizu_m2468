$(call inherit-product, device/generic/common/gsi_product.mk)
$(call inherit-product, device/generic/common/gsi_system_ext.mk)
$(call inherit-product, packages/modules/Virtualization/apex/product_packages.mk)

include vendor/lineage/build/target/product/lineage_generic_target.mk

DEVICE_PATH := device/meizu/m2468

# API
PRODUCT_SHIPPING_API_LEVEL := 34

# Based on GSI
BUILDING_GSI := true
MODULE_BUILD_FROM_SOURCE := true

# Bluetooth Audio (System-side HAL, sysbta)
PRODUCT_PACKAGES += \
    audio.sysbta.default \
    android.hardware.bluetooth.audio-service-system

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/bluetooth/audio/config/sysbta_audio_policy_configuration.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/sysbta_audio_policy_configuration.xml \
    $(DEVICE_PATH)/bluetooth/audio/config/sysbta_audio_policy_configuration_7_0.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/sysbta_audio_policy_configuration_7_0.xml

# Fingerprint (UDFPS)
PRODUCT_PACKAGES += \
    mz_fp_gesture_init \
    mz_fp_hbm_daemon

# Hide display cutout
PRODUCT_PRODUCT_PROPERTIES += \
    ro.support_hide_display_cutout=true
PRODUCT_PACKAGES += \
    AvoidAppsInCutoutOverlay \
    NoCutoutOverlay

# IMS
PRODUCT_PACKAGES += \
    ims \
    privapp-permissions-org.codeaurora.ims.xml \
    qcom-overlay-caf-ims \
    qcom-overlay-telephony-caf-ims

# Init
PRODUCT_PACKAGES += \
    init.m2468.rc \
    gsi_skip_mount.cfg

# Memory
PRODUCT_ENABLE_UFFD_GC := true

# Overlays
PRODUCT_PACKAGES += \
    CarrierConfig \
    FrameworksResM2468 \
    TelephonyResCommon

PRODUCT_PACKAGE_OVERLAYS += \
    $(DEVICE_PATH)/overlay \
    $(DEVICE_PATH)/overlay-lineage

ifneq ($(PRODUCT_IS_AUTOMOTIVE),true)
PRODUCT_PACKAGES += \
    gsi_overlay_framework \
    gsi_overlay_systemui

PRODUCT_COPY_FILES += \
    device/generic/common/overlays/overlay-config.xml:$(TARGET_COPY_OUT_SYSTEM_EXT)/overlay/config/config.xml
endif

# Partitions
PRODUCT_USE_DYNAMIC_PARTITIONS := true
PRODUCT_USE_DYNAMIC_PARTITION_SIZE := true

PRODUCT_BUILD_CACHE_IMAGE := false
PRODUCT_BUILD_DEBUG_BOOT_IMAGE := false
PRODUCT_BUILD_DEBUG_VENDOR_BOOT_IMAGE := false
PRODUCT_BUILD_USERDATA_IMAGE := false
PRODUCT_BUILD_VENDOR_IMAGE := false
PRODUCT_BUILD_SUPER_PARTITION := false
PRODUCT_BUILD_SUPER_EMPTY_IMAGE := false
PRODUCT_BUILD_SYSTEM_DLKM_IMAGE := false
PRODUCT_EXPORT_BOOT_IMAGE_TO_DIST := true

PRODUCT_ARTIFACT_PATH_REQUIREMENT_ALLOWED_LIST += \
    system/etc/init/config \
    system/product/% \
    system/system_ext/%

# Properties
PRODUCT_PRODUCT_PROPERTIES += \
    ro.crypto.metadata_init_delete_all_keys.enabled=false \
    debug.codec2.bqpool_dealloc_after_stop=1

# QCOM in-call audio fix from PHH
PRODUCT_PACKAGES += \
    QcRilAm

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += $(DEVICE_PATH)

# Two-pane layout in Settings
$(call inherit-product, $(SRC_TARGET_DIR)/product/window_extensions.mk)
PRODUCT_PRODUCT_PROPERTIES += \
    persist.settings.large_screen_opt.enabled=true

# VNDK
PRODUCT_EXTRA_VNDK_VERSIONS := 33