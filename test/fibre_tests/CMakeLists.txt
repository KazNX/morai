add_executable(fibre_tests
  FibreTests.cpp
  MoveTests.cpp
  ThreadPoolTests.cpp
)

target_link_libraries(fibre_tests
  PRIVATE
    morai
    gtest_main
)
