/*
 *  conf.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

