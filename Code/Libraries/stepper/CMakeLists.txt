# makes stepper folder a library
add_library(stepper
    stepper.h
    stepper.c
)

# Generate PIO header
pico_generate_pio_header(stepper ${CMAKE_CURRENT_LIST_DIR}/stepper.pio) # Absolutely req'd for stepper lib


# allows use of pio header
target_link_libraries(stepper 
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_irq
    pico_cyw43_arch_none
)

# lets project cmake find this shit
target_include_directories(stepper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})