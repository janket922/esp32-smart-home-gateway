set(srcs "example_init_video.c" "example_storage.c")
set(inc_dirs "include")

if(NOT CONFIG_IDF_TARGET_ESP32C61)
    list(APPEND srcs "example_encoder.c")
endif()

if(CONFIG_EXAMPLE_SELECT_CUSTOMIZED_DEV_BOARD)
    list(APPEND inc_dirs "include/boards/customized")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${inc_dirs}"
                       REQUIRES "esp_video" "fatfs")

if(CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER)
    idf_component_optional_requires(PRIVATE "esp_driver_jpeg")
endif()

if(CONFIG_TINYUSB_MSC_ENABLED)
    idf_component_optional_requires(PRIVATE "espressif__esp_tinyusb")
endif()

if(CONFIG_EXAMPLE_SDMMC_DEBUG_PIN_CONNECTIONS)
    idf_component_optional_requires(PRIVATE "sd_card")
endif()
