/* Copyright (c) 2010-2011 Dave Reisner
 *
 * util.c
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

#define _GNU_SOURCE
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "conf.h"
#include "util.h"

void debug(const char *format, ...) {
  va_list args;

  if (config->verbose < 2) {
    return;
  }

  fprintf(stderr, "debug: ");
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

char *read_stdin(const char *prompt, size_t maxlen, int echo) {
  struct termios t;
  char *buf;

  MALLOC(buf, ++maxlen, return NULL);

  printf("%s: ", prompt);

  if (!echo) {
    /* turn off the echo flag */
    tcgetattr(fileno(stdin), &t);
    t.c_lflag &= ~ECHO;
    tcsetattr(fileno(stdin), TCSANOW, &t);
  }

  if (!fgets(buf, maxlen, stdin)) {
    fprintf(stderr, "failed to read from stdin\n");
    return NULL;
  }

  /* fgets() will leave a newline char on the end */
  *(buf + strlen(buf) - 1) = '\0';

  if (!echo) {
    putchar('\n');
    t.c_lflag |= ECHO;
    tcsetattr(fileno(stdin), TCSANOW, &t);
  }

  return buf;
}

size_t strtrim(char *str)
{
  char *left = str, *right;

  if(!str || *str == '\0') {
    return 0;
  }

  while(isspace((unsigned char)*left)) {
    left++;
  }
  if(left != str) {
    memmove(str, left, (strlen(left) + 1));
  }

  if(*str == '\0') {
    return 0;
  }

  right = (char*)rawmemchr(str, '\0') - 1;
  while(isspace((unsigned char)*right)) {
    right--;
  }
  *++right = '\0';

  return right - left;
}

int touch(const char *filename) {
  int fd = open(filename, O_WRONLY|O_CREAT|O_CLOEXEC|O_NOCTTY, 0644);

  if (fd == -1) {
    return 1;
  }

  close(fd);

  return 0;
}

/* vim: set et sw=2: */
