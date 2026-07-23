find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

if(WIN32)
    add_executable(HunterTransfer WIN32
        main.cpp
        Application.cpp
    )
else()
    add_executable(HunterTransfer
        main.cpp
        Application.cpp
    )
endif()

target_include_directories(HunterTransfer PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(HunterTransfer PRIVATE
    ht_core
    ht_transfer
    ht_resume
    ht_verify
    ht_logger
    ht_config
    ht_gui
    Qt6::Core
    Qt6::Widgets
)

get_target_property(MOC_EXE_APP Qt6::moc LOCATION)
set(MOC_OUT_APP ${CMAKE_CURRENT_BINARY_DIR}/moc_Application.cpp)
add_custom_command(
    OUTPUT ${MOC_OUT_APP}
    COMMAND ${MOC_EXE_APP} ${CMAKE_CURRENT_SOURCE_DIR}/Application.h -o ${MOC_OUT_APP}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Application.h
    COMMENT "Running moc on Application.h"
)
set(MOC_OUT_ABOUT ${CMAKE_CURRENT_BINARY_DIR}/moc_AboutDialog.cpp)
add_custom_command(
    OUTPUT ${MOC_OUT_ABOUT}
    COMMAND ${MOC_EXE_APP} ${CMAKE_SOURCE_DIR}/GUI/AboutDialog.h -o ${MOC_OUT_ABOUT}
    DEPENDS ${CMAKE_SOURCE_DIR}/GUI/AboutDialog.h
    COMMENT "Running moc on AboutDialog.h"
)
target_sources(HunterTransfer PRIVATE ${MOC_OUT_APP} ${MOC_OUT_ABOUT})