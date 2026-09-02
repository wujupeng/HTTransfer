find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

if(WIN32)
    add_executable(HTTransfer WIN32
        main.cpp
        Application.cpp
        ${CMAKE_SOURCE_DIR}/Resources/app.rc
    )
else()
    add_executable(HTTransfer
        main.cpp
        Application.cpp
    )
endif()

set(QRC_FILE ${CMAKE_SOURCE_DIR}/Resources/resources.qrc)
qt6_add_resources(QRC_OUTPUT ${QRC_FILE})
target_sources(HTTransfer PRIVATE ${QRC_OUTPUT})

target_include_directories(HTTransfer PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(HTTransfer PRIVATE
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(HTTransfer PRIVATE blake3_static)
    target_link_options(HTTransfer PRIVATE "-Wl,--enable-new-dtags")
endif()

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
target_sources(HTTransfer PRIVATE ${MOC_OUT_APP} ${MOC_OUT_ABOUT})

if(WIN32)
    get_target_property(QT_WINDEPLOYQT_EXECUTABLE Qt6::windeployqt LOCATION)
    add_custom_command(TARGET HTTransfer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Running windeployqt..."
        COMMAND ${QT_WINDEPLOYQT_EXECUTABLE}
            --release
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            --compiler-runtime
            $<TARGET_FILE:HTTransfer>
        COMMENT "Deploying Qt runtime dependencies"
    )
endif()