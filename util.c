#include <ctype.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "util.h"

void delete_file(const char *filename) {
  struct stat st;

  if (stat(filename, &st) == 0)
    unlink(filename);
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
