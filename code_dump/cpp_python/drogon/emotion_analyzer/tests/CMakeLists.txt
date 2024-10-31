find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_emotion_analyzer test_emotion_analyzer.cpp)
target_link_libraries(
  test_emotion_analyzer PRIVATE GTest::GTest GTest::Main Drogon::Drogon
                                Python3::Python)

add_test(NAME test_emotion_analyzer COMMAND test_emotion_analyzer)
