#include "debug_logger.h"
#include <cstring>

FILE *DebugLogger::logFile = nullptr;

void DebugLogger::Init() {
  logFile = fopen("debug.log", "w");
  if (logFile) {
    fprintf(logFile, "=== PSP-BookReader Debug Log ===\n");
    fflush(logFile);
  }
}

void DebugLogger::Log(const char *format, ...) {
  if (!logFile)
    return;

  va_list args;
  va_start(args, format);
  vfprintf(logFile, format, args);
  va_end(args);
  fprintf(logFile, "\n");
  fflush(logFile); // Always flush immediately
}

void DebugLogger::Close() {
  if (logFile) {
    fprintf(logFile, "=== Log End ===\n");
    fclose(logFile);
    logFile = nullptr;
  }
}
