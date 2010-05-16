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

void delete_file(const char*);
int file_exists(const char*);
int cookie_expire_time(const char*, const char*, const char*);
void get_password(char**, int);
int get_tmpfile(char**, const char*);
void get_username(char**, int);
int line_starts_with(const char*, const char*);
char *strtrim(char*);
int touch(const char*);

#endif /* _BURP_UTIL_H */
