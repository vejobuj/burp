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
  char line[COOKIE_SIZE + 1];
  char *lptr;
  struct cookie_t *cookie;
  long expire;

  cookie = xcalloc(1, sizeof *cookie);

  expire = 0;

  fd = fopen(cookie_file, "r");
  while ((lptr = fgets(&line[0], COOKIE_SIZE, fd)) != NULL) {
    lptr = strtrim(lptr);

    if (*lptr == '#' || strlen(lptr) == 0)
      continue;

    cookie = cookie_to_struct(lptr, &cookie);

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
  (*cookie)->domain = strsep(&co, "\t");
  (*cookie)->secure = STREQ(strsep(&co, "\t"), "TRUE") ? 1 : 0;
  (*cookie)->path = strsep(&co, "\t");
  (*cookie)->hostonly = STREQ(strsep(&co, "\t"), "TRUE") ? 1: 0;
  (*cookie)->expire = strtol(strsep(&co, "\t"), NULL, 10);
  (*cookie)->name = strsep(&co, "\t");
  (*cookie)->value = strsep(&co, "\t");

  return *cookie;
}

