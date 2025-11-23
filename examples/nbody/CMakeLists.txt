add_executable(nbody)

target_sources(nbody
  PRIVATE
    NBody.cpp
)

target_link_libraries(nbody
  PRIVATE
    morai
    weaver
    cxxopts
)

target_include_directories(nbody
  PRIVATE SYSTEM
    "${CURSES_INCLUDE_DIRS}"
)
