
function(_doxygen_map_var TO FROM)
  cmake_parse_arguments(ARG "BOOL;LIST" "" "" ${ARGN})

  set(${TO})
  if(ARG_BOOL)
    set(${TO} NO)
    if(${FROM})
      set(${TO} YES)
    endif()
  elseif(ARG_LIST)
    foreach(ITEM IN LISTS ${FROM})
      set(${TO} "${${TO}} \\\n    ${ITEM}")
    endforeach()
  elseif(${FROM})
    set(${TO} ${${FROM}})
  endif()

  set(${TO} ${${TO}} PARENT_SCOPE)
endfunction()

function(doxygen_add_target PROJECT)
  cmake_parse_arguments(ARG "" "DOXYFILE;MD_MAIN;NAME;OUTPUT;RECURSIVE" "DIRS;TARGETS;EXCLUDE" ${ARGN})

  find_package(Doxygen REQUIRED)

  if (NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is required to build the documentation.")
  endif()

  # Build list of sources
  set(DOXYGEN_SOURCES)
  set(DOXYGEN_RECURSIVE NO)

  if(NOT ARG_OUTPUT)
    set(ARG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/html")
  endif()

  if(NOT ARG_NAME)
    set(ARG_NAME "${PROJECT}")
  endif()

  if(ARG_MD_MAIN)
    file(REAL_PATH "${ARG_MD_MAIN}" ARG_MD_MAIN BASE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
  endif()

  _doxygen_map_var(DOXYGEN_PROJECT ARG_NAME)
  _doxygen_map_var(DOXYGEN_RECURSIVE ARG_RECURSIVE BOOL)
  _doxygen_map_var(DOXYGEN_EXCLUDE ARG_EXCLUDE LIST)
  _doxygen_map_var(DOXYGEN_MD_MAIN ARG_MD_MAIN)
  _doxygen_map_var(DOXYGEN_OUTPUT ARG_OUTPUT)

  if(NOT ARG_DIRS AND NOT ARG_TARGETS)
    message(FATAL_ERROR "One or both of DIRS or TARGETS must be specified.")
  endif()

  set(DOXYGEN_SOURCES)
  if(ARG_TARGETS)
    foreach(TARGET IN LISTS ARG_TARGETS)
      get_target_property(TARGET_SOURCE_DIR ${TARGET} SOURCE_DIR)
      list(APPEND DOXYGEN_SOURCES "${TARGET_SOURCE_DIR}")
    endforeach()
  endif(ARG_TARGETS)

  if(ARG_DIRS)
    foreach(DIR IN LISTS ARG_DIRS)
      file(REAL_PATH "${DIR}" DIR BASE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
      list(APPEND DOXYGEN_SOURCES "${DIR}")
    endforeach()
  endif()

  _doxygen_map_var(DOXYGEN_INPUT DOXYGEN_SOURCES LIST)

  set(DOXYGEN_OUT "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
  if(ARG_DOXYFILE)
    set(DOXYGEN_IN "${ARG_DOXYFILE}")
  else()
    set(DOXYGEN_IN "${CMAKE_CURRENT_SOURCE_DIR}/cmake/doxygen.in")
  endif()

  configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

  add_custom_target(${PROJECT}-doc ALL
    COMMAND "${DOXYGEN_EXECUTABLE}" "${DOXYGEN_OUT}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
  )
endfunction(doxygen_add_target)
