find_package(Qt6 COMPONENTS Core REQUIRED)

add_library(ht_watch STATIC
    FileWatcher.cpp
    WatchSession.cpp
    BackupScheduler.cpp
)

target_include_directories(ht_watch PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ht_watch PUBLIC
    ht_core
    ht_transfer
    ht_logger
    Qt6::Core
)

get_target_property(MOC_EXE Qt6::moc LOCATION)

set(WATCH_MOC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/BackupScheduler.h
)

set(WATCH_MOC_SOURCES)
foreach(MOC_HEADER ${WATCH_MOC_HEADERS})
    get_filename_component(MOC_BASE ${MOC_HEADER} NAME_WE)
    set(MOC_OUT ${CMAKE_CURRENT_BINARY_DIR}/moc_${MOC_BASE}.cpp)
    add_custom_command(
        OUTPUT ${MOC_OUT}
        COMMAND ${MOC_EXE} ${MOC_HEADER} -o ${MOC_OUT}
        DEPENDS ${MOC_HEADER}
        COMMENT "Running moc on ${MOC_HEADER}"
    )
    list(APPEND WATCH_MOC_SOURCES ${MOC_OUT})
endforeach()

target_sources(ht_watch PRIVATE ${WATCH_MOC_SOURCES})
