/* Copyright (c) 2010-2011 Dave Reisner
 *
 * burp.c
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
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>

#include "conf.h"
#include "curl.h"
#include "util.h"

#define NUM_CATEGORIES (sizeof(categories)/sizeof(categories[0]))
#define COOKIE_SIZE 1024

/* structures */
struct category_t {
  const char *name;
  int num;
};

static const struct category_t categories[] = {
  { "daemons",      2 }, { "devel",        3 }, { "editors",      4 },
  { "emulators",    5 }, { "games",        6 }, { "gnome",        7 },
  { "i18n",         8 }, { "kde",          9 }, { "kernels",     19 },
  { "lib",         10 }, { "modules",     11 }, { "multimedia",  12 },
  { "network",     13 }, { "office",      14 }, { "science",     15 },
  { "system",      16 }, { "x11",         17 }, { "xfce",        18 }
};

static int fn_cmp_cat(const void *c1, const void *c2) {
  struct category_t *cat1 = (struct category_t*)c1;
  struct category_t *cat2 = (struct category_t*)c2;

  return strcmp(cat1->name, cat2->name);
}

static int category_is_valid(const char *cat) {
  struct category_t key, *res;

  key.name = cat;

  res = bsearch(&key, categories, NUM_CATEGORIES, sizeof(struct category_t), fn_cmp_cat);

  return res ? res->num : -1;
}

static long cookie_expire_time(const char *cookie_file, const char *domain,
    const char *name) {
  FILE *fp;
  long expire;
  char cdomain[256], cname[256];

  fp = fopen(cookie_file, "r");
  if (!fp) {
    return 0L;
  }

  for (;;) {
    char l[COOKIE_SIZE];

    cdomain[0] = cname[0] = '\0';
    expire = 0L;

    if(!(fgets(l, sizeof(l), fp))) {
      break;
    }

    strtrim(l);

    if (*l == '#' || strlen(l) == 0) {
      continue;
    }

    if (sscanf(l, "%s\t%*s\t%*s\t%*s\t%ld\t%s\t%*s", cdomain, &expire, cname) != 3) {
      continue;
    }

    if (STREQ(domain, cdomain) && STREQ(name, cname)) {
      debug("cookie found (expires %ld)\n", expire);
      break;
    }
  }

  fclose(fp);

  return expire;
}

static void usage(void) {
  fprintf(stderr, "burp %s\n"
  "Usage: burp [options] targets...\n\n"
  " Options:\n"
  "  -h, --help                Shows this help message.\n"
  "  -u, --user                AUR login username.\n"
  "  -p, --password            AUR login password.\n", VERSION);
  fprintf(stderr,
  "  -c CAT, --category=CAT    Assign the uploaded package with category CAT.\n"
  "                              This will default to the current category\n"
  "                              for pre-existing packages and 'None' for new\n"
  "                              packages. -c help will give a list of valid\n"
  "                              categories.\n");
  fprintf(stderr,
  "  -C FILE, --cookies=FILE   Use FILE to store cookies rather than the default\n"
  "                              temporary file. Useful with the -k option.\n"
  "  -k, --keep-cookies        Cookies will be persistent and reused for logins.\n"
  "                              If you specify this option, you must also provide\n"
  "                              a path to a cookie file.\n"
  "  -v, --verbose             be more verbose. Pass twice for debug info.\n\n"
  "  burp also honors a config file. See burp(1) for more information.\n\n");
}

static void usage_categories(void) {
  unsigned i;

  printf("Valid categories are:\n");
  for (i = 0; i < NUM_CATEGORIES; i++) {
    printf("\t%s\n", categories[i].name);
  }
  putchar('\n');
}

static int parseargs(int argc, char **argv) {
  int opt;
  int option_index = 0;
  static struct option opts[] = {
    {"cookies",       required_argument,  0, 'C'},
    {"category",      required_argument,  0, 'c'},
    {"help",          no_argument,        0, 'h'},
    {"keep-cookies",  no_argument,        0, 'k'},
    {"password",      required_argument,  0, 'p'},
    {"user",          required_argument,  0, 'u'},
    {"verbose",       no_argument,        0, 'v'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "C:c:hkp:u:v", opts, &option_index))) {
    if (opt < 0) {
      break;
    }

    switch (opt) {
      case 'h':
        usage();
        exit(0);
      case 'c':
        FREE(config->category);
        config->category = strndup(optarg, 16);
        break;
      case 'C':
        FREE(config->cookie_file);
        config->cookie_file = strdup(optarg);
        break;
      case 'k':
        config->cookie_persist = 1;
        break;
      case 'p':
        FREE(config->password);
        config->password = strndup(optarg, AUR_PASSWORD_MAX);
        config->cmdline_passwd = 1;
        break;
      case 'u':
        FREE(config->user);
        config->user = strndup(optarg, AUR_USER_MAX);
        config->cmdline_user = 1;
        break;
      case 'v':
        config->verbose++;
        break;

      case '?':
        return 1;
      default:
        return 1;
    }
  }

  return 0;
}

static int read_config_file(void) {
  int ret = 0;
  char *config_path, *ptr, *xdg_config_home;
  char line[BUFSIZ];
  FILE *fp;

  xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home) {
    if (asprintf(&config_path, "%s/burp/burp.conf", xdg_config_home) == -1) {
      fprintf(stderr, "error: failed to allocate memory\n");
      return 1;
    }
  } else {
    if (asprintf(&config_path, "%s/.config/burp/burp.conf", getenv("HOME")) == -1) {
      fprintf(stderr, "error: failed to allocate memory\n");
      return 1;
    }
  }

  fp = fopen(config_path, "r");
  if (!fp) {
    debug("failed to open %s: %s\n", config_path, strerror(errno));
    free(config_path);
    return ret;
  }

  while (fgets(line, BUFSIZ, fp)) {
    char *key;
    strtrim(line);

    if (line[0] == '#' || strlen(line) == 0) {
      continue;
    }

    if ((ptr = strchr(line, '#'))) {
      *ptr = '\0';
    }

    key = ptr = line;
    strsep(&ptr, "=");
    strtrim(key);
    strtrim(ptr);

    if (STREQ(key, "User")) {
      if (config->user == NULL) {
        config->user = strndup(ptr, AUR_USER_MAX);
        debug("using username: %s\n", config->user);
      }
    } else if (STREQ(key, "Password")) {
      if (config->password == NULL) {
        config->password = strndup(ptr, AUR_PASSWORD_MAX);
        debug("using password from config file.\n");
      }
    } else if (STREQ(key, "Cookies")) {
      if (config->cookie_file == NULL) {
        wordexp_t p;
        if (wordexp(ptr, &p, 0) == 0) {
          if (p.we_wordc == 1) {
            config->cookie_file = strdup(p.we_wordv[0]);
            debug("using cookie file: %s\n", config->cookie_file);
          } else {
            fprintf(stderr, "Ambiguous path to cookie file. Ignoring config option.\n");
          }
          wordfree(&p);
        } else {
          perror("wordexp");
          ret = errno;
          break;
        }
      }
    } else if (STREQ(key, "Persist")) {
      config->cookie_persist = 1;
    } else {
      fprintf(stderr, "Error parsing config file: bad option '%s'\n", key);
      ret = 1;
      break;
    }
  }

  fclose(fp);
  free(config_path);

  return ret;
}

int main(int argc, char **argv) {
  long cookie_expire;
  int ret = 1;

  config = config_new();

  if (curl_init() != 0) {
    fprintf(stderr, "Error: An error occurred while initializing curl\n");
    goto finish;
  }

  ret = parseargs(argc, argv);
  if (ret != 0) {
    return 1;
  }

  if (config->category) {
    config->catnum = category_is_valid(config->category);
    if (config->catnum < 0) {
      usage_categories();
      goto finish;
    }
  } else {
    config->catnum = 1;
  }

  if (optind == argc) {
    fprintf(stderr, "error: no packages specified (use -h for help)\n");
    goto finish;
  }

  /* We can't read the config file without having verbosity set, but the
   * command line options need to take precedence over the config.  file.
   * Therefore, if ((user && pass) || cookie file) is supplied on the command
   * line, we won't read the config file.
   */
  if (!(config->user || config->cookie_file)) {
    read_config_file();
  }

  if (config->cookie_persist && !config->cookie_file) {
    fprintf(stderr, "warning: ignoring --persist without path to cookie file\n");
  }

  if (cookie_setup() != 0) {
    goto finish;
  }

  cookie_expire = cookie_expire_time(config->cookie_file, AUR_URL_NO_PROTO, AUR_COOKIE_NAME);
  if (cookie_expire > 0) {
    if (time(NULL) < cookie_expire) {
      config->cookie_valid = 1;
    } else {
      fprintf(stderr, "Your cookie has expired. Gathering user and password...\n");
    }
  }

  if (!config->cookie_valid) {
    if (config->cmdline_user || !config->user) {
      config->user = read_stdin("Enter username", AUR_USER_MAX, 1);
      if (!config->user || !strlen(config->user)) {
        fprintf(stderr, "error: invalid username supplied\n");
        goto finish;
      }
    }

    if (!config->password || (config->cmdline_user && !config->cmdline_passwd)) {
      printf("[%s] ", config->user);
      config->password = read_stdin("Enter password", AUR_PASSWORD_MAX, 0);
    }
  }

  if (config->cookie_valid || aur_login() == 0) {
    ret = 0;
    while (optind < argc) {
      ret += aur_upload(argv[optind++]);
    }
  }

finish:
  if (config->cookie_file && !config->cookie_persist) {
    debug("Deleting file %s\n", config->cookie_file);
    unlink(config->cookie_file);
  }

  config_free(config);
  curl_cleanup();

  return ret;
}

/* vim: set et sw=2: */
