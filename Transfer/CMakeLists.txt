add_library(ht_transfer STATIC
    TransferEngine.cpp
    WorkerPool.cpp
    RetryController.cpp
    ConcurrentQueue.cpp
    ReaderPool.cpp
    WriterThread.cpp
    Adapters/SMBAdapter.cpp
    Adapters/HTTPAdapter.cpp
    Adapters/FTPAdapter.cpp
)

target_include_directories(ht_transfer PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters
)

target_link_libraries(ht_transfer PUBLIC ht_core ht_resume)

find_package(CURL REQUIRED)
if(WIN32)
    find_package(libssh2 CONFIG REQUIRED)
    target_link_libraries(ht_transfer PUBLIC CURL::libcurl libssh2::libssh2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSSH2 REQUIRED IMPORTED_TARGET libssh2)
    target_link_libraries(ht_transfer PUBLIC CURL::libcurl PkgConfig::LIBSSH2)
endif()

if(WIN32)
    target_link_libraries(ht_transfer PUBLIC Mpr)
endif()
