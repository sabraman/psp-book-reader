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

  va_list args, args2;
  va_start(args, format);
  va_copy(args2, args);
  vfprintf(logFile, format, args);
  vprintf(format, args2);
  printf("\n");
  va_end(args2);
  va_end(args);
  fprintf(logFile, "\n");
  // Removed: fflush(logFile);
  // Removed: fflush(stdout);
}

void DebugLogger::Close() {
  if (logFile) {
    fprintf(logFile, "=== Log End ===\n");
    fclose(logFile);
    logFile = nullptr;
  }
}
