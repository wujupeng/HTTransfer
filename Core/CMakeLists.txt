add_library(ht_core STATIC
    BufferPool.cpp
    Domain/TransferTask.cpp
    Domain/ChunkManifest.cpp
    Domain/ResumeFile.cpp
    Domain/IntegrityReport.cpp
    Domain/TransferPreset.cpp
    Domain/SpeedSchedule.cpp
    FileEngine.cpp
    SpeedController.cpp
    TaskManager.cpp
    LocalFileSource.cpp
    LocalFileSink.cpp
)

if(WIN32)
    target_link_libraries(ht_core PUBLIC ksuser)
endif()

if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_source_files_properties(BufferPool.cpp PROPERTIES COMPILE_FLAGS "/O0")
endif()

target_include_directories(ht_core PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Domain
    ${CMAKE_CURRENT_SOURCE_DIR}/Common
    ${CMAKE_BINARY_DIR}/generated
)

find_package(OpenSSL REQUIRED)
target_link_libraries(ht_core PUBLIC OpenSSL::Crypto)

