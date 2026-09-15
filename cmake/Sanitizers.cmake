function(pulsar_apply_sanitizers target_name)
  if(NOT PULSAR_ENABLE_ASAN AND NOT PULSAR_ENABLE_UBSAN)
    return()
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "PULSAR sanitizers require GCC or Clang.")
  endif()

  set(sanitizers)
  if(PULSAR_ENABLE_ASAN)
    list(APPEND sanitizers "address")
  endif()
  if(PULSAR_ENABLE_UBSAN)
    list(APPEND sanitizers "undefined")
  endif()

  list(JOIN sanitizers "," sanitizer_flags)

  target_compile_options(
    ${target_name}
    PRIVATE
      "-fsanitize=${sanitizer_flags}"
      -fno-omit-frame-pointer
      -g
  )
  target_link_options(${target_name} PRIVATE "-fsanitize=${sanitizer_flags}")

  if(PULSAR_ENABLE_UBSAN)
    target_compile_options(${target_name} PRIVATE -fno-sanitize-recover=undefined)
  endif()
endfunction()
