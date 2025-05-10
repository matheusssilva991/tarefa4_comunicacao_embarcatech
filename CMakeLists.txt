# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)
set(FREERTOS_KERNEL_PATH "/home/matheus/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)


project(tarefa4_comunicacao_embarcatech C CXX ASM)

pico_sdk_init()

add_executable(${PROJECT_NAME} src/main.c
        lib/button/button.c # Button library
        lib/led/led.c # LED library
        #lib/ssd1306/ssd1306.c # SSD1306 library
        #lib/ssd1306/display.c # Display library
        lib/ws2812b/ws2812b.c # WS2812B library
        #lib/buzzer/buzzer.c # Buzzer library)
)

pico_set_program_name(${PROJECT_NAME} "tarefa4_comunicacao_embarcatech")
pico_set_program_version(tarefa4_comunicacao_embarcatech "0.1")

# Generate PIO header
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lib/ws2812b/pio/ws2812b.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Add the standard library to the build
target_link_libraries(${PROJECT_NAME}
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/lib
        ${CMAKE_CURRENT_LIST_DIR}/config
        ${PICO_SDK_PATH}/lib/lwip/src/include
        ${PICO_SDK_PATH}/lib/lwip/src/include/arch
        ${PICO_SDK_PATH}/lib/lwip/src/include/lwip
)

target_sources(${PROJECT_NAME} PRIVATE
    ${PICO_SDK_PATH}/lib/lwip/src/apps/http/httpd.c
    ${PICO_SDK_PATH}/lib/lwip/src/apps/http/fs.c
)


# Add any user requested libraries
target_link_libraries(${PROJECT_NAME}
        #hardware_i2c
        hardware_pio
        #hardware_timer
        #hardware_clocks
        pico_cyw43_arch_lwip_threadsafe_background
        #hardware_adc
        #hardware_pwm
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        )

pico_add_extra_outputs(${PROJECT_NAME})

