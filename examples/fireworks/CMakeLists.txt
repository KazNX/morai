add_executable(fireworks)

target_sources(fireworks
  PRIVATE
    Fireworks.cpp
)

target_link_libraries(fireworks
  PRIVATE
    morai
    weaver
    cxxopts
)

target_include_directories(fireworks
  PRIVATE SYSTEM
    "${CURSES_INCLUDE_DIRS}"
)
