add_library(blake3_static STATIC
    blake3.c
    blake3_dispatch.c
    blake3_portable.c
    blake3_sse2.c
    blake3_sse41.c
    blake3_avx2.c
    blake3_avx512.c
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    target_sources(blake3_static PRIVATE
        blake3_sse2_x86-64_unix.S
        blake3_sse41_x86-64_unix.S
        blake3_avx2_x86-64_unix.S
        blake3_avx512_x86-64_unix.S
    )
endif()

set_target_properties(blake3_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
)

target_include_directories(blake3_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_source_files_properties(blake3_sse2.c PROPERTIES COMPILE_OPTIONS "-msse2")
set_source_files_properties(blake3_sse41.c PROPERTIES COMPILE_OPTIONS "-msse4.1")
set_source_files_properties(blake3_avx2.c PROPERTIES COMPILE_OPTIONS "-mavx2")
set_source_files_properties(blake3_avx512.c PROPERTIES COMPILE_OPTIONS "-mavx512f;-mavx512vl")
