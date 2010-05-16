/*
 *  util.c
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

#include <ctype.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "util.h"

void delete_file(const char *filename) {
  if (filename == NULL) return;

  struct stat st;

  if (stat(filename, &st) == 0)
    unlink(filename);
}

int file_exists(const char *filename) {
  struct stat st;

  return stat(filename, &st) == 0;
}

void get_password(char **buf, int length) {
  struct termios t;

  *buf = calloc(1, length + 1);
  if (*buf == NULL) {
    fprintf(stderr, "Error allocating %d bytes.\n", length + 1);
    return;
  }

  printf("Enter password: ");

  /* turn off the echo flag */
  tcgetattr(fileno(stdin), &t);
  t.c_lflag &= ~ECHO;
  tcsetattr(fileno(stdin), TCSANOW, &t);

  /* fgets() will leave a newline char on the end */
  fgets(*buf, length, stdin);
  *(*buf + strlen(*buf) - 1) = '\0';

  putchar('\n');
  t.c_lflag |= ECHO;
  tcsetattr(fileno(stdin), TCSANOW, &t);
}

int get_tmpfile(char **buf, const char *format) {
  if (*buf != NULL)
    FREE(*buf);

  *buf = calloc(1, PATH_MAX + 1);
  snprintf(*buf, PATH_MAX, format, getpid());

  return *buf == NULL;
}

void get_username(char **buf, int length) {
  *buf = calloc(1, length + 1);
  if (*buf == NULL) {
    fprintf(stderr, "Error allocating %d bytes.\n", length + 1);
    return;
  }

  printf("Enter username: ");

  /* fgets() will leave a newline char on the end */
  fgets(*buf, length, stdin);
  *(*buf + strlen(*buf) - 1) = '\0';
}

char *read_file_first_line(const char *file) {
  FILE *fd;
  char *buf;

  buf = calloc(1, BUFSIZ + 1);

  fd = fopen(file, "r");
  fgets(buf, BUFSIZ, fd);
  fclose(fd);

  /* Remove trailing newline */
  *(buf + strlen(buf) - 1) = '\0';

  return buf;
}

char *strtrim(char *str) {
  char *pch = str;

  if (str == NULL || *str == '\0')
    return str;

  while (isspace(*pch)) pch++;

  if (pch != str)
    memmove(str, pch, (strlen(pch) + 1));

  if (*str == '\0')
    return str;

  pch = (str + strlen(str) - 1);

  while (isspace(*pch))
    pch--;

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
