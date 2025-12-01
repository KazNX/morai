
function(morai_configure_target TARGET)
  target_compile_options(${TARGET}
    PRIVATE
      # Enable warnings, conformance and debug info.
      $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /Zc:__cplusplus /WX /Zi>
      # More warnings.
      $<$<CXX_COMPILER_ID:AppleClang,Clang,GCC,IntelLLVM>:-Wall -Wextra -Wpedantic -Werror -Wno-logical-op-parentheses>
  )
endfunction(morai_configure_target)
