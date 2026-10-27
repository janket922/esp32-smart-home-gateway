set(src_dirs
			App_Cloud_Upload
			App_Video_Streaming
			App_Sensor_Sync)

set(include_dirs
			App_Cloud_Upload
			App_Video_Streaming
			App_Sensor_Sync) 
            
set(requires
            cjson
			espressif__mqtt
			esp-tls
			mqtt
			Svc
			Bsp
			esp_rom
			esp_driver_uart
            esp_driver_gpio
			nvs_flash
            esp_wifi
			esp_event
			esp_video
			example_video_common
			esp_cam_sensor
			)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
component_compile_options(-ffast-math -O3 -Wno-error=format -Wno-format)