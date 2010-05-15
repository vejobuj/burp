/*
 *  conf.h
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

#ifndef _BURP_CONF_H
#define _BURP_CONF_H

#define COOKIEFILE_FORMAT  "/tmp/burp-%d.cookies"

struct config_t {
  char *user;
  char *password;
  char *cookies;
  char *category;
  int verbose;
};

int read_config_file(void);
struct config_t *config_new(void);
void config_free(struct config_t*);

extern struct config_t *config;

#endif /* _BURP_CONF_H */
