cmake_minimum_required(VERSION 3.13)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(mfd)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

add_executable(mfd
        mfd.c
        usb_descriptors.c
        )

target_include_directories(mfd PRIVATE ${CMAKE_CURRENT_LIST_DIR})
# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(mfd pico_stdlib hardware_gpio tinyusb_device tinyusb_board)

