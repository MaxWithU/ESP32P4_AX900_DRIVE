foreach(firmware IN ITEMS fw_adid_8800d80_u02.bin fw_patch_8800d80_u02.bin
                          fw_patch_table_8800d80_u02.bin fmacfw_8800d80_u02.bin)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/firmware/${firmware}")
        message(FATAL_ERROR "AX900 firmware missing: ${firmware}. Run python3 tools/fetch_firmware.py in the AX900 repository first.")
    endif()
endforeach()

set(ax900_supplicant_sources
    "supplicant/src/eapol_supp/eapol_supp_sm.c"
    "supplicant/src/eap_peer/eap.c"
    "supplicant/src/eap_peer/eap_methods.c"
    "supplicant/src/eap_peer/eap_peap.c"
    "supplicant/src/eap_peer/eap_mschapv2.c"
    "supplicant/src/eap_peer/mschapv2.c"
    "supplicant/src/eap_peer/eap_tls_common.c"
    "supplicant/src/eap_common/eap_common.c"
    "supplicant/src/eap_common/eap_peap_common.c"
    "supplicant/src/crypto/ms_funcs.c"
    "supplicant/src/crypto/md4-internal.c"
    "supplicant/src/crypto/des-internal.c"
    "supplicant/src/crypto/rc4.c"
    "supplicant/src/utils/base64.c"
    "supplicant/src/rsn_supp/wpa.c"
    "supplicant/src/rsn_supp/wpa_ie.c"
    "supplicant/src/rsn_supp/pmksa_cache.c"
    "supplicant/src/common/wpa_common.c"
    "supplicant/src/common/ieee802_11_common.c"
    "supplicant/src/utils/common.c"
    "supplicant/src/utils/wpabuf.c"
    "supplicant/src/utils/wpa_debug.c"
    "supplicant/src/crypto/sha1-prf.c"
    "supplicant/src/crypto/sha1-pbkdf2.c"
    "supplicant/src/crypto/sha256-prf.c"
    "supplicant/src/crypto/sha256-kdf.c"
    "supplicant/src/crypto/aes-wrap.c"
    "supplicant/src/crypto/aes-unwrap.c"
    "supplicant/src/crypto/aes-omac1.c"
)

idf_component_register(SRCS ${ax900_supplicant_sources} "ax900_help.c" "ax900_pool.c" "ax900_metrics.c" "ax900_test.c" "ax900_ping.c" "ax900_profile.c" "ax900_probe.c" "ax900_tls.c" "ax900_net.c" "ax900_supplicant_port.c" "ax900_usb.c" "ax900_firmware.c" "ax900_wifi.c"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "." "supplicant/src" "supplicant/src/utils" REQUIRES usb esp_timer esp_netif esp_event esp_hw_support mbedtls lwip nvs_flash esp_partition bootloader_support
    EMBED_FILES "firmware/fw_adid_8800d80_u02.bin"
                "firmware/fw_patch_8800d80_u02.bin"
                "firmware/fw_patch_table_8800d80_u02.bin"
                "firmware/fmacfw_8800d80_u02.bin")

target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_NO_RANDOM_POOL CONFIG_NO_STDOUT_DEBUG CONFIG_NO_WPA_MSG CONFIG_NO_TKIP IEEE8021X_EAPOL EAP_PEAP EAP_MSCHAPv2 bswap_16=__builtin_bswap16 bswap_32=__builtin_bswap32 bswap_64=__builtin_bswap64 CONFIG_NO_SOCKLEN_T_TYPEDEF __BYTE_ORDER=__ORDER_LITTLE_ENDIAN__ __LITTLE_ENDIAN=__ORDER_LITTLE_ENDIAN__ __BIG_ENDIAN=__ORDER_BIG_ENDIAN__)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-parameter -Wno-misleading-indentation)

# Keep this vendored Hostap independent from ESP-IDF/native Wi-Fi supplicants.
target_compile_options(${COMPONENT_LIB} PRIVATE -include "${CMAKE_CURRENT_LIST_DIR}/ax900_symbols.h")
