add_executable(ht_core_tests
    BufferPoolTest.cpp
    TypesTest.cpp
    ResultTest.cpp
    DomainTest.cpp
)

add_executable(ht_simple_buffer_test
    SimpleBufferTest.cpp
)

target_include_directories(ht_simple_buffer_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(ht_simple_buffer_test PRIVATE ht_core)

target_compile_definitions(ht_core_tests PRIVATE GTEST_HAS_PTHREAD=0)

target_link_libraries(ht_core_tests PRIVATE ht_core GTest::gtest GTest::gtest_main)


add_test(NAME CoreTests COMMAND ht_core_tests)