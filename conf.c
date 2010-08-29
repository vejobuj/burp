/* Copyright (c) 2010 Dave Reisner
 *
 * conf.c
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

#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "conf.h"
#include "curl.h"
#include "util.h"

struct config_t *config = NULL;

void config_free(struct config_t *config) {
  if (config->user)
    free(config->user);
  if (config->password)
    free(config->password);
  if (config->cookies)
    free(config->cookies);
  if (config->category && STRNEQ(config->category, "None"))
    free(config->category);

  free(config);
}

struct config_t *config_new(void) {
  struct config_t *config = xcalloc(1, sizeof *config);

  config->user = config->password = config->cookies = config->category = NULL;
  config->persist = FALSE;
  config->verbose = 0;
  config->catnum = 1;

  return config;
}

