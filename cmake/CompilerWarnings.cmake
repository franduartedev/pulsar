function(pulsar_apply_compiler_warnings target_name)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)

    if(PULSAR_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE -Werror)
    endif()
  elseif(MSVC)
    # Sources contain UTF-8 Spanish UI literals. Without /utf-8 MSVC decodes
    # them using the ANSI code page and silently corrupts the strings.
    # MSVC reports __cplusplus as 199711L unless /Zc:__cplusplus is enabled,
    # which breaks the C++20 feature checks in the domain headers.
    target_compile_options(${target_name} PRIVATE /utf-8 /Zc:__cplusplus)

    if(PULSAR_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE /WX)
    endif()
  endif()
endfunction()
