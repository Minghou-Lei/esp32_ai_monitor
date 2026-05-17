# 项目级 override：复用 registry 里的 `espressif/esp_codec_dev` 源码，
# 仅补齐 ESP-IDF 6.x 需要的细粒度 `esp_driver_*` 依赖声明。
set(ESP_CODEC_DEV_UPSTREAM_DIR "${PROJECT_DIR}/managed_components/espressif__esp_codec_dev")

set(COMPONENT_PRIV_INCLUDEDIRS
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/api_lib"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/firmware"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/priv_include")

set(COMPONENT_ADD_INCLUDEDIRS
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/include"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/interface"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/include")

set(COMPONENT_SRCS
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/esp_codec_dev.c"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/esp_codec_dev_vol.c"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/esp_codec_dev_if.c"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/audio_codec_sw_vol.c")

list(APPEND COMPONENT_SRCS
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/platform/audio_codec_gpio.c"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/platform/audio_codec_ctrl_i2c.c"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/platform/audio_codec_data_i2s.c"
    "platform/audio_codec_ctrl_spi.c"
    "${ESP_CODEC_DEV_UPSTREAM_DIR}/platform/esp_codec_dev_os.c")

if (CONFIG_CODEC_ES8311_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/es8311/es8311.c")
endif()

if (CONFIG_CODEC_ES8156_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/es8156/es8156.c")
endif()

if (CONFIG_CODEC_ES7243E_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/es7243e/es7243e.c")
endif()

if (CONFIG_CODEC_ES7210_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/es7210/es7210.c")
endif()

if (CONFIG_CODEC_ES7243_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/es7243/es7243.c")
endif()

if (CONFIG_CODEC_ES8388_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/es8388/es8388.c")
endif()

if (CONFIG_CODEC_TAS5805M_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/tas5805m/tas5805m.c")
endif()

if (CONFIG_CODEC_ES8374_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/es8374/es8374.c")
endif()

if (CONFIG_CODEC_AW88298_SUPPORT)
    list(APPEND COMPONENT_SRCS "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/aw88298/aw88298.c")
endif()

if (CONFIG_CODEC_ZL38063_SUPPORT)
    list(APPEND COMPONENT_SRCS
        "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/zl38063.c"
        "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/api_lib/vprocTwolf_access.c"
        "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/api_lib/vproc_common.c"
        "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/example_apps/tw_hal_verify.c"
        "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/example_apps/tw_ldcfg.c"
        "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/example_apps/tw_ldfw.c"
        "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/example_apps/tw_ldfwcfg.c"
        "${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/example_apps/tw_spi_access.c")
endif()

idf_component_register(
    SRCS "${COMPONENT_SRCS}"
    INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
    PRIV_INCLUDE_DIRS "${COMPONENT_PRIV_INCLUDEDIRS}"
    REQUIRES driver
    PRIV_REQUIRES freertos esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_spi)

if (CONFIG_CODEC_ZL38063_SUPPORT)
    if (NOT ((CONFIG_IDF_TARGET STREQUAL "esp32c6") OR (CONFIG_IDF_TARGET STREQUAL "esp32c3")))
        target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L${ESP_CODEC_DEV_UPSTREAM_DIR}/device/zl38063/firmware")
        target_link_libraries(${COMPONENT_TARGET} INTERFACE firmware)
    endif()
endif()
