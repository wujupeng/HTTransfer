find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(ht_gui STATIC
    MainWindow.cpp
    TaskPanel.cpp
    ProgressView.cpp
    ConfigDialog.cpp
    SpeedScheduleWidget.cpp
    LogViewer.cpp
)

target_include_directories(ht_gui PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ht_gui PUBLIC ht_core ht_watch Qt6::Core Qt6::Widgets)

if(MSVC)
    target_compile_options(ht_gui PRIVATE /Zc:__cplusplus)
endif()

get_target_property(MOC_EXE Qt6::moc LOCATION)

set(MOC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WatchConfigDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SystemTrayManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TaskPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ProgressView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ConfigDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SpeedScheduleWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.h
)

set(MOC_SOURCES)
foreach(MOC_HEADER ${MOC_HEADERS})
    get_filename_component(MOC_BASE ${MOC_HEADER} NAME_WE)
    set(MOC_OUT ${CMAKE_CURRENT_BINARY_DIR}/moc_${MOC_BASE}.cpp)
    add_custom_command(
        OUTPUT ${MOC_OUT}
        COMMAND ${MOC_EXE} ${MOC_HEADER} -o ${MOC_OUT}
        DEPENDS ${MOC_HEADER}
        COMMENT "Running moc on ${MOC_HEADER}"
    )
    list(APPEND MOC_SOURCES ${MOC_OUT})
endforeach()

target_sources(ht_gui PRIVATE ${MOC_SOURCES})