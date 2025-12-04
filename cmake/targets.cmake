
function(morai_configure_target TARGET)
  target_compile_options(${TARGET}
    PRIVATE
      # Enable warnings, conformance and debug info.
      $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /Zc:__cplusplus /WX /Zi /wd4251>
      # More warnings.
      $<$<CXX_COMPILER_ID:AppleClang,Clang,GCC,IntelLLVM>:-Wall -Wextra -Wpedantic -Werror -Wno-logical-op-parentheses>
  )
  target_link_options(${TARGET} PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/debug>
  )
endfunction(morai_configure_target)
