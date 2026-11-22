add_subdirectory(../../../modules othermodules)

target_link_libraries(${CMAKE_PROJECT_NAME} 
    #hardware_uart hardware_dma
    DmxPio
)

