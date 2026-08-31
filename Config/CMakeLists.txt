add_library(ht_config STATIC
    ConfigManager.cpp
    PresetRepository.cpp
    ScheduleRepository.cpp
)

target_include_directories(ht_config PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ht_config PUBLIC ht_core ht_logger)

find_package(Qt6 COMPONENTS Core REQUIRED)
target_link_libraries(ht_config PUBLIC Qt6::Core)