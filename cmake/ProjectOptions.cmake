function(pulsar_apply_project_options target_name)
  target_compile_features(${target_name} PUBLIC cxx_std_20)
  set_target_properties(
    ${target_name}
    PROPERTIES
      CXX_EXTENSIONS OFF
      CXX_STANDARD_REQUIRED ON
  )

  if(MSVC AND DEFINED ENV{PULSAR_WIN_DIAG})
    target_compile_options(${target_name} PRIVATE "/FI$ENV{PULSAR_WIN_DIAG}")
  endif()

  pulsar_apply_sanitizers(${target_name})
endfunction()
