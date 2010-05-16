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

void delete_file(const char*);
void get_password(char**, int);
int get_tmpfile(char**, const char*);
void get_username(char**, int);
char *strtrim(char*);

#endif /* _BURP_UTIL_H */
