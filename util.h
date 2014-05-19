#ifndef _BURP_UTIL_H
#define _BURP_UTIL_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define _cleanup_(x) __attribute__((cleanup(x)))
#define ARRAYSIZE(x) (sizeof(x)/sizeof(x[0]))

static inline bool streq(const char *a, const char *b) {
  return strcmp(a, b) == 0;
}

static inline void freep(void *p) { free(*(void **)p); }
#define _cleanup_free_ _cleanup_(freep)

static inline void fclosep(FILE **f) { if (*f) fclose(*f); }
#define _cleanup_fclose_ _cleanup_(fclosep)

#endif /* _BURP_UTIL_H */

/* vim: set et sw=2: */
