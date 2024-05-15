set(PICO_BOARD pico_w)
add_executable(main
        main.c
)


target_link_libraries(main 
                      pico_stdlib 
                      freertos 
                      hardware_adc 
                      pico_cyw43_arch_none 
                      pico_cyw43_arch_lwip_threadsafe_background
                      )


pico_add_extra_outputs(main)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_LIST_DIR} )