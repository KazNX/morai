add_library(arachne)
target_sources(arachne
  PUBLIC
    Common.hpp
    Fibre.hpp
    FibreQueue.hpp
    Finally.hpp
    Resumption.hpp
    Scheduler.hpp
  PRIVATE
    Fibre.cpp
    FibreQueue.cpp
    Scheduler.cpp
)

target_compile_features(arachne
  PRIVATE cxx_std_20
  INTERFACE cxx_std_20
)

target_include_directories(arachne
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
