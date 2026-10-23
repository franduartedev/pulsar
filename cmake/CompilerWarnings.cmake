function(pulsar_apply_compiler_warnings target_name)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)

    if(PULSAR_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE -Werror)
    endif()
  elseif(MSVC)
    # Sources contain UTF-8 Spanish UI literals. Without /utf-8 MSVC decodes
    # them using the ANSI code page and silently corrupts the strings.
    target_compile_options(${target_name} PRIVATE /utf-8)

    if(PULSAR_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE /WX)
    endif()
  endif()
endfunction()
