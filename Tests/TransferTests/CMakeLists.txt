add_executable(ht_transfer_tests
    TransferEngineTest.cpp
    DataChunkTest.cpp
    ConcurrentQueueTest.cpp
    SmbDetectionTest.cpp
    ReaderWriterTest.cpp
    TransferEngineRwTest.cpp
)

target_link_libraries(ht_transfer_tests PRIVATE ht_transfer ht_core ht_resume ht_logger ht_verify GTest::gtest GTest::gtest_main)

add_test(NAME TransferTests COMMAND ht_transfer_tests)
