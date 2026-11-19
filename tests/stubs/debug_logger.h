#pragma once

// Host-test stub of include/debug_logger.h: same public API shape, but
// the implementation lives in tests/host_test.cpp so the tests can
// count log calls, capture the messages, and stay quiet during the fuzz
// pass. No file is written (the real one opens debug.log in cwd).

class DebugLogger {
public:
  static void Init();
  static void Log(const char *format, ...);
  static void Close();

  // Test hooks (not part of the real API).
  static int logsEmitted;
  static char lastLog[256];
  static bool quiet;     // suppress printing (set during the fuzz pass)
  static bool capturing; // append every message to logBuffer
  static char logBuffer[8192];
};
