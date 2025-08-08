cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(estacao_meteriologica C CXX ASM)

pico_sdk_init()

include_directories( ${CMAKE_SOURCE_DIR}/lib )

add_executable(${PROJECT_NAME}  
    estacao_meteriologica.c
    lib/ssd1306.c
    lib/ws2812.c
    lib/buzzer.c
    lib/aht20.c 
    lib/bmp280.c 
)

pico_set_program_name(${PROJECT_NAME} "estacao_meteriologica")
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

target_link_libraries(${PROJECT_NAME} 
        pico_stdlib
        hardware_i2c
        hardware_adc
        hardware_pio
        hardware_pwm
        hardware_spi
    )

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lib/ws2812.pio)

pico_add_extra_outputs(${PROJECT_NAME})

