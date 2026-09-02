cmake_minimum_required(VERSION 3.25)
project(HunterTransfer VERSION 0.1.0 LANGUAGES C CXX)

set(HT_VERSION_PRERELEASE "alpha.10" CACHE STRING "Pre-release identifier" FORCE)

if(HT_VERSION_PRERELEASE)
    set(HT_VERSION_STRING "${PROJECT_VERSION}-${HT_VERSION_PRERELEASE}")
else()
    set(HT_VERSION_STRING "${PROJECT_VERSION}")
endif()
set(HT_VERSION_FULL "HunterTransfer v${HT_VERSION_STRING}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    @ONLY
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(VCPKG_APPLOCAL_DEPS_INSTALL OFF CACHE BOOL "Disable vcpkg applocal" FORCE)

if(WIN32)
    set(Qt6_DIR "C:/Users/DELL/Documents/Qt6/6.8.2/msvc2022_64/lib/cmake/Qt6" CACHE PATH "Qt6 cmake dir")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime" FORCE)
endif()

if(WIN32)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
        HT_PLATFORM_WINDOWS
    )
    message(STATUS "Platform: Windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(HT_PLATFORM_LINUX)
    add_compile_definitions(_FILE_OFFSET_BITS=64)
    add_compile_definitions(HT_BLAKE3_STATIC)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)
    message(STATUS "Platform: Linux")
else()
    add_compile_definitions(HT_PLATFORM_UNSUPPORTED)
    message(WARNING "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

add_compile_definitions(
    HT_CHUNK_SIZE=16777216
    HT_BUFFER_POOL_SIZE=67108864
    HT_MAX_PARALLELISM=8
    HT_DEFAULT_PARALLELISM=4
    HT_ENABLE_FULL_BUILD
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2 /DNDEBUG)
    else()
        add_compile_options(-O2 -DNDEBUG)
    endif()
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)

find_package(zstd REQUIRED)

add_subdirectory(Core)
add_subdirectory(Transfer)
add_subdirectory(Resume)
add_subdirectory(Verify)
add_subdirectory(Logger)
add_subdirectory(Config)
add_subdirectory(Watch)
add_subdirectory(PAL)
add_subdirectory(GUI)
add_subdirectory(App)
add_subdirectory(Plugins)
add_subdirectory(Tests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(third_party/blake3)
endif()
