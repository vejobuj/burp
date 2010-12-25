/* Copyright (c) 2010 Dave Reisner
 *
 * cookies.c
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include "conf.h"
#include "cookies.h"
#include "util.h"

long cookie_expire_time(const char *cookie_file,
                        const char *domain,
                        const char *name) {
  FILE *fd;
  char line[COOKIE_SIZE + 1];
  char *lptr;
  cookie_t *cookie;
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

int cookie_still_valid(long expire) {
  return (time(NULL) < expire);
}

cookie_t *cookie_to_struct(char *co, cookie_t **cookie) {
  (*cookie)->domain   = strtok(co, "\t");
  (*cookie)->secure   = STREQ(strtok(NULL, "\t"), "TRUE") ? 1 : 0;
  (*cookie)->path     = strtok(NULL, "\t");
  (*cookie)->httponly = STREQ(strtok(NULL, "\t"), "TRUE") ? 1 : 0;
  (*cookie)->expire   = strtol(strtok(NULL, "\t"), NULL, 10);
  (*cookie)->name     = strtok(NULL, "\t");
  (*cookie)->value    = strtok(NULL, "\t");

  return *cookie;
}

