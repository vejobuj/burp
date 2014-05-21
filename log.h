#ifndef _LOG_H
#define _LOG_H

#include <stdarg.h>

enum {
  LOG_ERROR,
  LOG_WARN,
  LOG_INFO,
  LOG_DEBUG,
};

int log_meta(int level, const char*file, int line, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

int log_metav(int level, const char*file, int line, const char *format,
    va_list ap) __attribute__((format(printf, 4, 0)));

void log_set_level(int loglevel);
int log_get_max_level(void);

#define log_full(level, ...) \
  do { \
      if (log_get_max_level() >= (level)) \
        log_meta((level), __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)

#define log_debug(...)  log_full(LOG_DEBUG,   __VA_ARGS__)
#define log_info(...)   log_full(LOG_INFO,    __VA_ARGS__)
#define log_notice(...) log_full(LOG_NOTICE,  __VA_ARGS__)
#define log_warn(...)   log_full(LOG_WARN,    __VA_ARGS__)
#define log_error(...)  log_full(LOG_ERROR,   __VA_ARGS__)

/* vim: set et ts=2 sw=2: */

#endif  /* _LOG_H */
