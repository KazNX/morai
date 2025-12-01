add_library(morai)
morai_configure_target(morai)

target_compile_options(morai
  PUBLIC
    # Lock free queue requires alignment which raises a padding warning. Disable that warning.
    # Must be public as the header is visible downstream.
    $<$<CXX_COMPILER_ID:MSVC>:/wd4324>
)

target_sources(morai
  PRIVATE
    Fibre.cpp
    FibreQueue.cpp
    Log.cpp
    Scheduler.cpp
    SharedQueue.cpp
    ThreadPool.cpp
  PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      Clock.hpp
      Common.hpp
      Fibre.hpp
      FibreQueue.hpp
      Finally.hpp
      Id.hpp
      Log.hpp
      Move.hpp
      MPMCQueue.hpp
      Resumption.hpp
      Scheduler.hpp
      SharedQueue.hpp
      ThreadPool.hpp
)

target_compile_features(morai
  PRIVATE cxx_std_20
  INTERFACE cxx_std_20
)

target_include_directories(morai
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

install(
  TARGETS morai
  EXPORT morai-targets
  ARCHIVE
    COMPONENT Dev
    DESTINATION lib
  LIBRARY
    COMPONENT Dev
    DESTINATION lib
  RUNTIME
    COMPONENT Runtime
    DESTINATION bin
  FILE_SET HEADERS
    COMPONENT Dev
)
