add_library(ht_resume STATIC
    ResumeEngine.cpp
    ResumeFileParser.cpp
    ResumeFileWriter.cpp
)

target_include_directories(ht_resume PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ht_resume PUBLIC ht_core ht_logger)