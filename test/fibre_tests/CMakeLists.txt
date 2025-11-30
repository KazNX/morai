add_executable(fibre_tests
  FibreTests.cpp
  MoveTests.cpp
  ThreadPoolTests.cpp
)
morai_configure_target(fibre_tests)

get_target_property(GTEST_INCLUDE_DIRS gtest INCLUDE_DIRECTORIES)

target_include_directories(fibre_tests
  SYSTEM PRIVATE
    "${GTEST_INCLUDE_DIRS}"
)

target_link_libraries(fibre_tests
  PRIVATE
    morai
    gtest_main
)
