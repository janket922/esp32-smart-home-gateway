set(src_dirs
			KEY
			XL9555
            LED
			WIFI_CONNECT
			VIDEO)
set(include_dirs
			KEY
			XL9555
            LED
			WIFI_CONNECT
			VIDEO) 
            
set(requires
			vfs
			fatfs
			esp_blockdev
			esp_driver_i2c
			esp_rom
			esp_driver_uart
            esp_driver_gpio
			esp_wifi
			esp_event
			esp_video)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
component_compile_options(-ffast-math -O3 -Wno-error=format -Wno-format)