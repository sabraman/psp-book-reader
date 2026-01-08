#pragma once

#include <cstdarg>
#include <cstdio>

class DebugLogger {
public:
  static void Init();
  static void Log(const char *format, ...);
  static void Close();

private:
  static FILE *logFile;
};
