add_library(morai)
target_sources(morai
  PUBLIC
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
  PRIVATE
    Fibre.cpp
    FibreQueue.cpp
    Log.cpp
    Scheduler.cpp
    SharedQueue.cpp
    ThreadPool.cpp
)

target_compile_features(morai
  PRIVATE cxx_std_20
  INTERFACE cxx_std_20
)

target_include_directories(morai
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
