cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(pico_sdk_import.cmake)

project(Teste_Voltimetro C CXX ASM) 

pico_sdk_init()


add_executable(${PROJECT_NAME}  
        Ohmimetro01.c
        lib/ssd1306.c
        )

target_compile_definitions(${PROJECT_NAME} PRIVATE 
        PICO_PRINTF_SUPPORT_FLOAT=1 
        PICO_STDIO_ENABLE_PRINTF=1
    )

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/pio_matrix.pio)

target_link_libraries(${PROJECT_NAME} 
        pico_stdlib 
        hardware_i2c
        hardware_adc
        hardware_pio
        )

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_add_extra_outputs(${PROJECT_NAME})


