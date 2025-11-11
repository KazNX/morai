
if(WIN32)
  find_package(unofficial-pdcurses REQUIRED)

  # set(CURSES_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/_deps/pdcurses-src")
  set(CURSES_LIBRARY unofficial::pdcurses::pdcurses)
else()
  find_package(Curses REQUIRED)
endif()

find_package(SDL3 REQUIRED)

# List all CMake targets defined so far
get_property(all_targets GLOBAL PROPERTY TARGETS)
message(STATUS "CMake targets: ${all_targets}")

add_subdirectory(fireworks)
add_subdirectory(weaver)
