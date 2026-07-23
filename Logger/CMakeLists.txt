add_library(ht_logger STATIC
    Logger.cpp
    AuditLogRepository.cpp
)

target_include_directories(ht_logger PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ht_logger PUBLIC ht_core)

find_package(SQLite3 REQUIRED)
target_link_libraries(ht_logger PUBLIC SQLite::SQLite3)
