set(src_dirs
			Svc_Encode_Base64
			Svc_Isotp
			Svc_Mqtt
			Svc_Video)

set(include_dirs
			Svc_Encode_Base64
			Svc_Isotp
			Svc_Mqtt
			Svc_Video
			) 
            
set(requires
            cjson
			espressif__mqtt
			esp-tls
			mqtt
            Bsp
			esp_rom
			esp_driver_uart
            esp_driver_gpio
			nvs_flash
            esp_wifi
			esp_event
			esp_driver_twai
			esp_timer
			esp_isotp
			)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
component_compile_options(-ffast-math -O3 -Wno-error=format -Wno-format)