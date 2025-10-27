add_executable(fibre_tests
  FibreTests.cpp
)

target_link_libraries(fibre_tests
  PRIVATE
    fibre
    gtest_main
)
