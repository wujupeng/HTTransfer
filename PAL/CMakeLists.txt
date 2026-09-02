add_library(ht_pal STATIC
    PlatformFactory.cpp
)

target_include_directories(ht_pal PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ht_pal PUBLIC
    ht_core
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

if(MSVC)
    target_compile_options(ht_pal PRIVATE /Zc:__cplusplus)
endif()