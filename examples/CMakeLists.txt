
if(WIN32)
  include(ExternalProject)

  find_program(NMAKE_EXECUTABLE NAMES nmake)

  ExternalProject_Add(
    pdcurses
    GIT_REPOSITORY https://github.com/wmcbrine/PDCurses.git
    GIT_TAG 3.9
    STEP_TARGETS build
    SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/pdcurses-src"
    BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/pdcurses-src/wincon"
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_COMMAND "${NMAKE_EXECUTABLE}" -f "Makefile.vc" WIDE=Y UTF8=Y
    BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/_deps/pdcurses-src/wincon/pdcurses.lib"
  )

  set(CURSES_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/_deps/pdcurses-src")
  set(CURSES_LIBRARY "${CMAKE_BINARY_DIR}/_deps/pdcurses-src/wincon/pdcurses.lib")
else()
  find_package(Curses REQUIRED)
endif()

add_subdirectory(fireworks)
add_subdirectory(weaver)
