/*
 *  cookies.h
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

#ifndef _BURP_COOKIES_H
#define _BURP_COOKIES_H

#define COOKIE_SIZE   512

struct cookie_t {
  char *domain;
  int secure;
  char *path;
  int httponly;
  long expire;
  char *name;
  char *value;
};

struct cookie_t *cookie_to_struct(char*, struct cookie_t**);
int cookie_still_valid(long);
long cookie_expire_time(const char*, const char*, const char*);

#endif /* _BURP_COOKIES_H */
