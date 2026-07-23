enable_testing()

if(WIN32)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL "https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
else()
    find_package(GTest CONFIG REQUIRED)
endif()

add_subdirectory(CoreTests)
add_subdirectory(TransferTests)
add_subdirectory(ResumeTests)
add_subdirectory(VerifyTests)
