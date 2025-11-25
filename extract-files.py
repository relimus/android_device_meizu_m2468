#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../tools/extract-utils'))

from extract_utils.file import File
from extract_utils.fixups_blob import (
    BlobFixupCtx,
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixup_remove,
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'device/meizu/m2468',
    'hardware/qcom-caf/sm8550',
    'hardware/qcom-caf/wlan',
    'vendor/qcom/opensource/commonsys/display',
    'vendor/qcom/opensource/commonsys-intf/display',
    'vendor/qcom/opensource/dataservices',
]

def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None


lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (
    ): lib_fixup_vendor_suffix,
    (
        'com.dsi.ant@1.0',
    ): lib_fixup_remove,
}

blob_fixups: blob_fixups_user_type = {
         'vendor/bin/hw/android.hardware.power-service': blob_fixup()
        .replace_needed('android.hardware.power-V3-ndk', 'android.hardware.power-V4-ndk'),

        'vendor/bin/rkp_factory_extraction_tool': blob_fixup()
        .replace_needed('android.hardware.security.keymint-V2-ndk', 'android.hardware.security.keymint-V4-ndk'),

        'vendor/bin/hw/android.hardware.health-service.qti': blob_fixup()
        .replace_needed('android.hardware.health-V1-ndk', 'android.hardware.health-V4-ndk'),

        'vendor/bin/hw/android.hardware.sensors-service.multihal': blob_fixup()
        .replace_needed('android.hardware.sensors-V1-ndk', 'android.hardware.sensors-V3-ndk'),

        'vendor/bin/hw/wpa_supplicant': blob_fixup()
        .replace_needed('android.hardware.wifi.supplicant-V1-ndk', 'android.hardware.wifi.supplicant-V4-ndk'),

        'vendor/lib64/libbluetooth_audio_session_aidl.so': blob_fixup()
        .replace_needed('android.hardware.bluetooth.audio-V2-ndk', 'android.hardware.bluetooth.audio-V5-ndk'),

        'vendor/lib64/hw/audio.bluetooth.default.so': blob_fixup()
        .replace_needed('android.hardware.bluetooth.audio-V2-ndk', 'android.hardware.bluetooth.audio-V5-ndk'),

       (
        'system/lib64/graphicbuffersource-aidl-ndk.so',
        'system/lib64/libstagefright_graphicbuffersource_aidl.so',
        ): blob_fixup()
        .replace_needed('android.hardware.graphics.common-V5-ndk', 'android.hardware.graphics.common-V6-ndk'),

        (
            'system_ext/lib64/libmiracastsystem.so',
            'system_ext/lib64/libwfdservice.so',
        ): blob_fixup()
        .replace_needed('android.media.audio.common.types-V2-cpp', 'android.media.audio.common.types-V4-cpp'),
        
}  # fmt: skip

module = ExtractUtilsModule(
    'm2468',
    'meizu',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
    add_firmware_proprietary_file=True,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()