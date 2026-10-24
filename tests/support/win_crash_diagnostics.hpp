#pragma once
// TEMPORARY Windows CI diagnostics: make silent aborts and crashes visible.
#if defined(_WIN32)
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdlib.h>

namespace {
struct PulsarWinCrashDiagnostics {
  PulsarWinCrashDiagnostics() {
    _set_error_mode(_OUT_TO_STDERR);
    std::set_terminate([] {
      try {
        if (auto error = std::current_exception()) std::rethrow_exception(error);
      } catch (const std::exception& error) {
        std::fprintf(stderr, "[diag] terminate: %s\n", error.what());
      } catch (...) {
        std::fprintf(stderr, "[diag] terminate: non-std exception or none\n");
      }
      std::fflush(stderr);
      std::_Exit(3);
    });
    std::signal(SIGSEGV, [](int) { std::fputs("[diag] SIGSEGV\n", stderr); std::fflush(stderr); std::_Exit(3); });
    std::signal(SIGABRT, [](int) { std::fputs("[diag] SIGABRT\n", stderr); std::fflush(stderr); std::_Exit(3); });
  }
};
const PulsarWinCrashDiagnostics pulsar_win_crash_diagnostics;
} // namespace
#endif
