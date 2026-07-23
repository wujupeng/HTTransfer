add_executable(ht_resume_tests
    ResumeEngineTest.cpp
)

target_link_libraries(ht_resume_tests PRIVATE ht_resume GTest::gtest GTest::gtest_main)

add_test(NAME ResumeTests COMMAND ht_resume_tests)