add_executable(ht_transfer_tests
    TransferEngineTest.cpp
)

target_link_libraries(ht_transfer_tests PRIVATE ht_transfer GTest::gtest GTest::gtest_main)

add_test(NAME TransferTests COMMAND ht_transfer_tests)