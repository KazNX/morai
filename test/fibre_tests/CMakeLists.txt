add_executable(fibre_tests
  fibre_tests.cpp
)

target_link_libraries(fibre_tests
  PRIVATE
    fibre
    gtest_main
)
