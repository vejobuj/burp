#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "conf.h"
#include "util.h"

struct config_t *config = NULL;

void config_free(struct config_t *config) {
  if (config->user)
    free(config->user);
  if (config->password)
    free(config->password);
  if (config->cookies)
    free(config->cookies);
  if (config->category && strcmp(config->category, "None") != 0)
    free(config->category);

  free(config);
}

struct config_t *config_new(void) {
  struct config_t *config = malloc(sizeof *config);
  if (config == NULL) {
    fprintf(stderr, "Error allocating %zd bytes for config.\n", sizeof *config);
    return NULL;
  }

  config->user = config->password = config->cookies = config->category = NULL;
  config->verbose = 0;

  return config;
}

int read_config_file() {
  int ret;
  struct stat st;
  char *ptr;
  char config_path[PATH_MAX + 1], line[BUFSIZ + 1];

  snprintf(&config_path[0], PATH_MAX, "%s/%s", 
    getenv("XDG_CONFIG_HOME"), "burp/burp.conf");

  if (stat(config_path, &st) != 0) {
    if (config->verbose > 1)
      printf("::DEBUG:: No config file found\n");
    return 0;
  }

  if (config->verbose > 1)
    printf("::DEBUG:: Found config file\n");

  ret = 0;
  FILE *conf_fd = fopen(config_path, "r");
  while (fgets(line, BUFSIZ, conf_fd)) {
    strtrim(line);

    if (line[0] == '#' || strlen(line) == 0)
      continue;

    if ((ptr = strchr(line, '#'))) {
      *ptr = '\0';
    }

    char *key;
    key = ptr = line;
    strsep(&ptr, "=");
    strtrim(key);
    strtrim(ptr);

    if (strcmp(key, "User") == 0) {
      config->user = strdup(ptr);
    } else if (strcmp(key, "Password") == 0) {
      config->password = strdup(ptr);
    } else {
      fprintf(stderr, "Error parsing config file: bad option '%s'\n", key);
      ret = 1;
      break;
    }
  }

  fclose(conf_fd);

  return ret;
}
