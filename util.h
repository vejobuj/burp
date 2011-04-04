/* Copyright (c) 2010-2011 Dave Reisner
 *
 * util.h
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

#ifndef _BURP_UTIL_H
#define _BURP_UTIL_H

#include <stdarg.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define ALLOC_FAIL(s) do { fprintf(stderr, "could not allocate %lu bytes\n", s); } while(0)
#define CALLOC(p, l, s, action) do { p = calloc(l, s); if(!p) { ALLOC_FAIL(s); action; } } while(0)
#define FREE(x) do { if(x) free((void*)x); x = NULL; } while(0)
#define STREQ(x,y) strcmp(x,y) == 0

void debug(const char*, ...);
char *read_stdin(const char*, size_t, int);
char *get_tmpfile(const char*);
char *strtrim(char*);
int touch(const char*);

#endif /* _BURP_UTIL_H */
