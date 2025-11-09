add_executable(fireworks)

target_sources(fireworks
  PRIVATE
    Fireworks.cpp
)

target_link_libraries(fireworks
  PRIVATE
    arachne
    weaver
)

target_include_directories(fireworks
  PRIVATE SYSTEM
    "${CURSES_INCLUDE_DIRS}"
)
