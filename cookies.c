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
  while ((lptr = fgets(&line[0], COOKIE_SIZE, fd))) {
    strtrim(lptr);

    if (*lptr == '#' || strlen(lptr) == 0)
      continue;

    cookie = cookie_to_struct(lptr, &cookie);

    if (STREQ(domain, cookie->domain) && STREQ(name, cookie->name)) {
      expire = cookie->expire;
      if (config->verbose > 1)
        printf("::DEBUG:: Cookie found (expires %ld)\n", expire);
      break;
    }
  }
  fclose(fd);

  free(cookie);

  return expire;
}

struct cookie_t *cookie_to_struct(char *co, struct cookie_t **cookie) {
  (*cookie)->domain   = strtok(co, "\t");
  (*cookie)->secure   = STREQ(strtok(NULL, "\t"), "TRUE") ? 1 : 0;
  (*cookie)->path     = strtok(NULL, "\t");
  (*cookie)->httponly = STREQ(strtok(NULL, "\t"), "TRUE") ? 1 : 0;
  (*cookie)->expire   = strtol(strtok(NULL, "\t"), NULL, 10);
  (*cookie)->name     = strtok(NULL, "\t");
  (*cookie)->value    = strtok(NULL, "\t");

  return *cookie;
}

