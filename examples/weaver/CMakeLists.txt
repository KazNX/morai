add_library(weaver SHARED)

include(GenerateExportHeader)

generate_export_header(weaver
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/weaver/WeaverExport.hpp"
)

target_sources(weaver
  PRIVATE
    Weaver.cpp
    Weaver.hpp
)

target_link_libraries(weaver
  PRIVATE
    "${CURSES_LIBRARY}"
)

target_include_directories(weaver
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/weaver>
    $<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_include_directories(weaver
  PRIVATE SYSTEM
    "${CURSES_INCLUDE_DIRS}"
)
