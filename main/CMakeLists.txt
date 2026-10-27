idf_component_register(
    SRCS "app_main.c"
    PRIV_REQUIRES
        esp-tls
        esp_wifi
        esp_event
        nvs_flash
    REQUIRES
        Bsp
		App
		Svc
		esp_video
		esp_cam_sensor
		example_video_common
    INCLUDE_DIRS "."
	EMBED_TXTFILES "mosquitto.org.crt"
)