add_library(fibre)
target_sources(fibre
  PUBLIC
    FILE_SET fibre_modules TYPE CXX_MODULES FILES
    Api.cxxm
    Common.cxxm
    Fibre.cxxm
    Resumption.cxxm
    Scheduler.cxxm
PRIVATE
    Fibre.cxx
    Scheduler.cxx
)

# target_sources(fibre 
# )

set_target_properties(fibre PROPERTIES
  CXX_MODULE_STD 1
  CXX_SCAN_FOR_MODULES ON
)

target_compile_features(fibre
  PRIVATE cxx_std_23
  INTERFACE cxx_std_20
)

target_compile_options(fibre
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/experimental:module>
)
