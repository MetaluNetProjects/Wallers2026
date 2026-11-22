add_library(DmxPio INTERFACE)

target_sources(DmxPio INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/DmxOutput.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DmxInput.cpp
)

target_include_directories(DmxPio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(DmxPio INTERFACE
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_irq
)

