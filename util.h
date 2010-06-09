/*
 *  util.h
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

#ifndef _BURP_UTIL_H
#define _BURP_UTIL_H

#define TRUE 1
#define FALSE 0

#define FREE(x) do { free(x); x = NULL; } while (0)
#define STREQ(x,y) strcmp(x,y) == 0
#define STRNEQ(x,y) strcmp(x,y) != 0

void delete_file(const char*);
void die(const char*, ...);
int file_exists(const char*);
char *get_password(size_t);
char *get_tmpfile(const char*);
char *get_username(size_t);
int line_starts_with(const char*, const char*);
char *strtrim(char*);
int touch(const char*);
void *xcalloc(size_t, size_t);
void *xmalloc(size_t);

#endif /* _BURP_UTIL_H */
