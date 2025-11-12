add_library(weaver SHARED)

include(GenerateExportHeader)

generate_export_header(weaver
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/weaver/WeaverExport.hpp"
)

target_sources(weaver
  PRIVATE
    private/WeaverImp.hpp
    Weaver.cpp
    Weaver.hpp
)

if(WIN32)
  target_sources(weaver
    PRIVATE
      Input_Win32.cpp
  )

  target_compile_definitions(weaver PRIVATE WIN32_LEAN_AND_MEAN)

  target_link_libraries(weaver
    PRIVATE
      user32
  )
else()
  target_sources(weaver
    PRIVATE
      Input_Curses.cpp
  )
endif()

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
