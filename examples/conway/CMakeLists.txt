add_executable(conway)
morai_configure_target(conway)

target_sources(conway
  PRIVATE
    Conway.cpp
)

target_link_libraries(conway
  PRIVATE
    morai
    weaver
    cxxopts
)

target_include_directories(conway
  PRIVATE SYSTEM
    "${CURSES_INCLUDE_DIRS}"
)
