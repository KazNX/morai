# Project setup and minimum cmake version.
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(arachne LANGUAGES CXX)

# C++ standards setup.
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Add cmake directory to the modules path.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
list(APPEND CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")

include(tools)

# Add project level options here.
option(FIBRE_BUILD_TESTS "Build unit tests?" ON)

set(ConfigPackageLocation lib/cmake/arachne)

# Ensure debug libraries are built with different named to release builds. This is to address issues such as MSVC
# having different debug and release runtime libraries. For a well setup API, one which hides resource allocation and
# ensures symmetrical deallocation occurs from the same allocator, this won't be a problem, but the consistency is
# useful.
ensure_set(CMAKE_DEBUG_POSTFIX "_d")
# Marshall all binaries to the same directory. This is expecially useful on Windows when trying to run exectuables from
# this project with shared libraries. Otherwise those shared libraries aren't on the path. Note that other package
# binaries should be on the path already.
ensure_set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
ensure_set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY, "${CMAKE_BINARY_DIR}/lib")
ensure_set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY, "${CMAKE_BINARY_DIR}/bin")
ensure_set(CMAKE_LIBRARY_OUTPUT_DIRECTORY, "${CMAKE_BINARY_DIR}/$<IF:$<PLATFORM_ID:Windows,WindowsCE,WindowsStore>,bin,lib>")

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
  ensure_set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CONFIG}")
  ensure_set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY, "${CMAKE_BINARY_DIR}/lib/${CONFIG}")
  ensure_set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY, "${CMAKE_BINARY_DIR}/bin/${CONFIG}")
  ensure_set(CMAKE_LIBRARY_OUTPUT_DIRECTORY, "${CMAKE_BINARY_DIR}/$<IF:$<PLATFORM_ID:Windows,WindowsCE,WindowsStore>,bin,lib>/${CONFIG}")
endforeach()

# Allow the use of folders to group targets in supporting environments.
# For example Visual Studio solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Include sub projects.
add_subdirectory(arachne)

if(FIBRE_BUILD_TESTS)
  # We can enable testing here and/or in the subdirectory, but doing it here allows us to run CTest from the build root.
  # To run the tests, we execute:
  #   CTest -C [Debug|Release|RelWithDebInfo|MinSizeRel] --output-on-failure
  # CTest normally shows only a very terse test output, but we make sure failed tests show all output by adding
  #   --output-on-failure
  # The full test output is always available in:
  #   <build>/Testing/Temporary/LastTest.log
  # Note: not all projects under tests actually add something for CTest.
  enable_testing()
  add_subdirectory(test)
endif(FIBRE_BUILD_TESTS)
