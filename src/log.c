#include "log.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

static int max_log_level = LOG_WARN;

static const char *get_logprefix(int loglevel) {
  switch (loglevel) {
  case LOG_ERROR:
    return "error: ";
  case LOG_WARN:
    return "warning: ";
  case LOG_INFO:
    return ":: ";
  case LOG_DEBUG:
  default:
    return "debug: ";
  }
}

int log_get_max_level(void) {
  return max_log_level;
}

void log_set_level(int loglevel) {
  max_log_level = loglevel;
}

int log_metav(int level, const char *file, int line, const char *format,
    va_list ap) {
  char buffer[LINE_MAX];
  FILE *stream = level <= LOG_WARN ? stderr : stdout;

  vsnprintf(buffer, sizeof(buffer), format, ap);

  if (level >= LOG_DEBUG)
    return fprintf(stream, "[%s:%d] %s%s\n", file, line, get_logprefix(level),
        buffer);
  else
    return fprintf(stream, "%s%s\n", get_logprefix(level), buffer);
}

int log_meta(int level, const char *file, int line, const char *format, ...) {
  int r;
  va_list ap;

  va_start(ap, format);
  r = log_metav(level, file, line, format, ap);
  va_end(ap);

  return r;
}
