if(PULSAR_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)

  if(NOT CLANG_TIDY_EXE)
    message(
      FATAL_ERROR
      "PULSAR_ENABLE_CLANG_TIDY=ON was requested, but clang-tidy was not found in PATH."
    )
  endif()

  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()
