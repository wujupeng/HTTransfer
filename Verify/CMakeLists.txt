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

find_package(blake3 CONFIG REQUIRED)
target_link_libraries(ht_verify PUBLIC BLAKE3::blake3)
