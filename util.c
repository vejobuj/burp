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
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#include "conf.h"
#include "util.h"

void debug(const char *format, ...) {
  va_list args;
  time_t t;
  struct tm *tmp;
  char timestr[10] = { 0 };

  if (config->verbose < 2) {
    return;
  }

  t = time(NULL);
  tmp = localtime(&t);
  strftime(timestr, 9, "%H:%M:%S", tmp);
  timestr[8] = '\0';

  printf("[%s] debug: ", timestr);

  va_start(args, format);
  vfprintf(stdout, format, args);
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

char *get_tmpfile(const char *format) {
  char *buf;

  if (asprintf(&buf, format, getpid()) < 0 || !buf) {
    fprintf(stderr, "unable to allocate tmpfile name\n");
    return NULL;
  }

  return buf;
}

char *strtrim(char *str) {
  char *pch = str;

  if (str == NULL || *str == '\0') {
    return str;
  }

  while (isspace(*pch)) {
    pch++;
  }

  if (pch != str) {
    memmove(str, pch, (strlen(pch) + 1));
  }

  if (*str == '\0') {
    return str;
  }

  pch = (str + strlen(str) - 1);

  while (isspace(*pch)) {
    pch--;
  }

  *++pch = '\0';

  return str;
}

int touch(const char *filename) {
  int fd;

  fd = open(filename, O_WRONLY | O_CREAT | O_NONBLOCK | O_NOCTTY,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

  if (fd == -1) {
    return 1;
  }

  return close(fd);
}

