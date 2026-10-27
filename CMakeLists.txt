# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)


set(EXTRA_COMPONENT_DIRS
	Components/Bsp
    Components/Bsp/example_video_common
    Components/Bsp/esp_video
    Components/Bsp/esp_cam_sensor
    Components/App
	Components/Svc
)

add_compile_options(-fdiagnostics-color=always)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)


project(test_7_20_mqtt_video)
