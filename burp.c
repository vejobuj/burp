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

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
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
#define TARGETMAX 32

/* forward decls */
static int category_is_valid(const char*);
static long cookie_expire_time(const char*, const char*, const char*);
static int fn_cmp_cat (const void*, const void*);
static int parseargs(int, char**);
static int read_config_file(void);
static void usage(void);
static void usage_categories(void);


/* structures */
typedef struct __category_t {
  const char *name;
  int num;
} category_t;

static category_t categories[] = {
  { "daemons",      2 }, { "devel",        3 }, { "editors",      4 },
  { "emulators",    5 }, { "games",        6 }, { "gnome",        7 },
  { "i18n",         8 }, { "kde",          9 }, { "kernels",     19 },
  { "lib",         10 }, { "modules",     11 }, { "multimedia",  12 },
  { "network",     13 }, { "office",      14 }, { "science",     15 },
  { "system",      16 }, { "x11",         17 }, { "xfce",        18 }
};

int category_is_valid(const char *cat) {
  category_t key, *res;

  key.name = cat;

  res = bsearch(&key, categories, NUM_CATEGORIES, sizeof(category_t), fn_cmp_cat);

  return res ? res->num : -1;
}

long cookie_expire_time(const char *cookie_file,
                        const char *domain,
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

int fn_cmp_cat (const void *c1, const void *c2) {
  category_t *cat1 = (category_t*)c1;
  category_t *cat2 = (category_t*)c2;

  return strcmp(cat1->name, cat2->name);
}

int parseargs(int argc, char **argv) {
  int opt;
  int option_index = 0;
  static struct option opts[] = {
    {"help",          no_argument,        0, 'h'},
    {"user",          required_argument,  0, 'u'},
    {"password",      required_argument,  0, 'p'},
    {"keep-cookies",  no_argument,        0, 'k'},
    {"category",      required_argument,  0, 'c'},
    {"cookies",       required_argument,  0, 'C'},
    {"verbose",       no_argument,        0, 'v'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "hu:p:kc:C:v", opts, &option_index))) {
    if (opt < 0) {
      break;
    }

    switch (opt) {
      case 'h':
        usage();
        exit(0);
      case 'c':
        if (config->category) {
          FREE(config->category);
        }
        config->category = strndup(optarg, 16);
        break;
      case 'C':
        if (config->cookies) {
          FREE(config->cookies);
        }
        config->cookies = strndup(optarg, PATH_MAX);
        break;
      case 'k':
        config->persist = true;
        break;
      case 'p':
        if (config->password) {
          FREE(config->password);
        }
        config->password = strndup(optarg, AUR_PASSWORD_MAX);
        break;
      case 'u':
        if (config->user) {
          FREE(config->user);
        }
        config->user = strndup(optarg, AUR_USER_MAX);
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

int read_config_file() {
  int ret = 0;
  char *ptr, *xdg_config_home;
  char config_path[PATH_MAX + 1], line[BUFSIZ];
  FILE *fp;

  xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home) {
    snprintf(config_path, PATH_MAX, "%s/burp/burp.conf", xdg_config_home);
  } else {
    snprintf(config_path, PATH_MAX, "%s/.config/burp/burp.conf",
      getenv("HOME"));
  }

  fp = fopen(config_path, "r");
  if (!fp) {
    debug("failed to open %s: %s\n", config_path, strerror(errno));
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
      if (config->cookies == NULL) {
        wordexp_t p;
        if (wordexp(ptr, &p, 0) == 0) {
          if (p.we_wordc == 1) {
            config->cookies = strdup(p.we_wordv[0]);
            debug("using cookie file: %s\n", config->cookies);
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
      config->persist = true;
    } else {
      fprintf(stderr, "Error parsing config file: bad option '%s'\n", key);
      ret = 1;
      break;
    }
  }

  fclose(fp);

  return ret;
}

void usage() {
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

void usage_categories() {
  unsigned i;

  printf("Valid categories are:\n");
  for (i = 0; i < NUM_CATEGORIES; i++) {
    printf("\t%s\n", categories[i].name);
  }
  putchar('\n');
}

int main(int argc, char **argv) {
  int ret = 0, cookie_valid = false;

  config = config_new();

  ret = parseargs(argc, argv);
  if (ret != 0) {
    goto finish;
  }

  /* Ensure we have a proper config environment */
  if (config->category == NULL) {
    config->category = "None";
  } else {
    config->catnum = category_is_valid(config->category);
  }

  if (config->catnum < 0) {
    usage_categories();
    goto finish;
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
  if (!(config->user || config->cookies)) {
    read_config_file();
  }

  /* Quick sanity check */
  if (config->persist && !config->cookies) {
    fprintf(stderr, "error: do not specify persistent "
                    "cookies without providing a path to a cookie file.\n");
    goto finish;
  }

  /* Determine how we'll login -- either by cookie or credentials */
  if (config->cookies != NULL) { /* User specified cookie file */
    if (!access(config->cookies, R_OK) == 0) {
      if (touch(config->cookies) != 0) {
        fprintf(stderr, "Error creating cookie file: ");
        perror(config->cookies);
        goto finish;
      }
    } else { /* assume its a real cookie file and evaluate it */
      long expire = cookie_expire_time(config->cookies, AUR_URL_NO_PROTO , AUR_COOKIE_NAME);
      if (expire > 0) {
        if (time(NULL) < expire) {
          cookie_valid = true;
        } else {
          fprintf(stderr, "Your cookie has expired. Gathering user and password...\n");
        }
      }
    }
  } else { /* create PID based file in /tmp */
    if ((config->cookies = get_tmpfile(COOKIEFILE_FORMAT)) == NULL) {
      fprintf(stderr, "error creating cookie file.\n");
      goto finish;
    }
  }

  if (!cookie_valid) {
    debug("cookie auth will fail. Falling back to user/pass\n");

    if (config->user == NULL) {
      config->user = read_stdin("Enter username", AUR_USER_MAX, 1);
    }

    if (config->password == NULL) {
      printf("[%s] ", config->user);
      config->password = read_stdin("Enter password", AUR_PASSWORD_MAX, 0);
    }
  }

  if (curl_init() != 0) {
    fprintf(stderr, "Error: An error occurred while initializing curl\n");
    goto finish;
  }

  if (cookie_valid || aur_login() == 0) {
    while (optind < argc) {
      aur_upload(argv[optind++]);
    }
  }

  debug("Cleaning up curl handle\n");

  curl_cleanup();

finish:
  if (config->cookies != NULL && !config->persist) {
    debug("Deleting file %s\n", config->cookies);
    unlink(config->cookies);
  }

  config_free(config);

  return ret;
}

