foreach(firmware IN ITEMS fw_adid_8800d80_u02.bin fw_patch_8800d80_u02.bin
                          fw_patch_table_8800d80_u02.bin fmacfw_8800d80_u02.bin)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/firmware/${firmware}")
        message(FATAL_ERROR "AX900 firmware missing: ${firmware}. Run python3 tools/fetch_firmware.py in the AX900 repository first.")
    endif()
endforeach()

idf_component_register(SRCS "ax900_usb.c" "ax900_firmware.c" "ax900_wifi.c"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "." REQUIRES usb esp_timer
    EMBED_FILES "firmware/fw_adid_8800d80_u02.bin"
                "firmware/fw_patch_8800d80_u02.bin"
                "firmware/fw_patch_table_8800d80_u02.bin"
                "firmware/fmacfw_8800d80_u02.bin")
