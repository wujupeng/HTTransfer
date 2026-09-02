add_library(ht_verify STATIC
    VerifyEngine.cpp
    CRC32Calculator.cpp
    SHA256Calculator.cpp
    Blake3Calculator.cpp
)

target_include_directories(ht_verify PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ht_verify PUBLIC ht_core)

find_package(OpenSSL REQUIRED)
target_link_libraries(ht_verify PUBLIC OpenSSL::Crypto)

if(WIN32)
    find_package(blake3 CONFIG QUIET)
    if(blake3_FOUND)
        target_link_libraries(ht_verify PUBLIC BLAKE3::blake3)
    else()
        find_library(BLAKE3_LIB NAMES blake3 PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)
        find_path(BLAKE3_INCLUDE NAMES blake3.h PATHS /usr/include /usr/local/include)
        if(BLAKE3_LIB AND BLAKE3_INCLUDE)
            add_library(blake3::blake3 SHARED IMPORTED)
            set_target_properties(blake3::blake3 PROPERTIES
                IMPORTED_LOCATION "${BLAKE3_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${BLAKE3_INCLUDE}"
            )
            target_link_libraries(ht_verify PUBLIC blake3::blake3)
        else()
            message(FATAL_ERROR "blake3 library not found")
        endif()
    endif()
else()
    target_link_libraries(ht_verify PUBLIC blake3_static)
endif()
