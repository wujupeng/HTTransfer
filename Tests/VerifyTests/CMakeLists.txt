add_executable(ht_verify_tests
    VerifyEngineTest.cpp
)

target_link_libraries(ht_verify_tests PRIVATE ht_verify GTest::gtest GTest::gtest_main)

add_test(NAME VerifyTests COMMAND ht_verify_tests)