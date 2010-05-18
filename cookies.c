#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "conf.h"
#include "cookies.h"
#include "util.h"

long cookie_expire_time(const char *cookie_file,
                        const char *domain,
                        const char *name) {
  FILE *fd;
  char buf[COOKIE_SIZE + 1];
  struct cookie_t *cookie;
  long expire;

  cookie = calloc(1, sizeof *cookie);
  if (buf == NULL) {
    fprintf(stderr, "Error allocating %zd bytes.\n", sizeof *cookie);
    return 0;
  }

  expire = 0;

  fd = fopen(cookie_file, "r");
  while ((fgets(&buf[0], COOKIE_SIZE, fd)) != NULL) {
    strtrim(&buf[0]);
    if (*buf == '#' || strlen(buf) == 0)
      continue;

    cookie = cookie_to_struct(buf, &cookie);

    if (STREQ(domain, cookie->domain) && STREQ(name, cookie->name)) {
      expire = cookie->expire;
      if (config->verbose > 1) {
        printf("::DEBUG:: Appropriate cookie found with expire time of %ld\n", 
               cookie->expire);
        break;
      }
    }
  }
  fclose(fd);

  free(cookie);

  return expire;
}

struct cookie_t *cookie_to_struct(char *co, struct cookie_t **cookie) {
  if (*cookie == NULL) {
    *cookie = calloc(1, sizeof **cookie);
    if (*cookie == NULL) {
      fprintf(stderr, "Error allocating %zd bytes.\n", sizeof **cookie);
      return NULL;
    }
  }

  (*cookie)->domain = strsep(&co, "\t");
  (*cookie)->secure = STREQ(strsep(&co, "\t"), "TRUE") ? 1 : 0;
  (*cookie)->path = strsep(&co, "\t");
  (*cookie)->hostonly = STREQ(strsep(&co, "\t"), "TRUE") ? 1: 0;
  (*cookie)->expire = strtol(strsep(&co, "\t"), NULL, 10);
  (*cookie)->name = strsep(&co, "\t");
  (*cookie)->value = strsep(&co, "\t");

  return *cookie;
}

