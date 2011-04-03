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
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>

#include "conf.h"
#include "curl.h"
#include "llist.h"
#include "util.h"

#define NUM_CATEGORIES (sizeof(categories)/sizeof(categories[0]))
#define COOKIE_SIZE 1024

static struct llist_t *targets;

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

static long cookie_expire_time(const char *cookie_file,
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
      if (config->verbose > 1) {
        printf("::DEBUG:: Cookie found (expires %ld)\n", expire);
      }
      break;
    }
  }

  fclose(fp);

  return(expire);
}

static int fn_cmp_cat (const void *c1, const void *c2) {
  category_t *cat1 = (category_t*)c1;
  category_t *cat2 = (category_t*)c2;

  return(strcmp(cat1->name, cat2->name));
}

static int category_is_valid(const char *cat) {
  category_t key, *res;

  key.name = cat;

  res = bsearch(&key, categories, NUM_CATEGORIES, sizeof(category_t), fn_cmp_cat);

  return(res ? res->num : -1);
}

static int read_config_file(void) {
  int ret = 0;
  char *ptr, *xdg_config_home;
  char config_path[PATH_MAX + 1], line[BUFSIZ];
  FILE *conf_fd;

  xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home) {
    snprintf(&config_path[0], PATH_MAX, "%s/burp/burp.conf", xdg_config_home);
  } else {
    snprintf(&config_path[0], PATH_MAX, "%s/.config/burp/burp.conf",
      getenv("HOME"));
  }

  if (! access(config_path, R_OK) == 0) {
    if (config->verbose > 1) {
      printf("::DEBUG:: No config file found or not readable\n");
    }
    return(ret);
  }

  conf_fd = fopen(config_path, "r");

  if (config->verbose > 1) {
    printf("::DEBUG:: Found config file\n");
  }

  while (fgets(line, BUFSIZ, conf_fd)) {
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
        if (config->verbose > 1) {
          printf("::DEBUG:: Using username: %s\n", config->user);
        }
      }
    } else if (STREQ(key, "Password")) {
      if (config->password == NULL) {
        config->password = strndup(ptr, AUR_PASSWORD_MAX);
        if (config->verbose > 1) {
          printf("::DEBUG:: Using password from config file.\n");
        }
      }
    } else if (STREQ(key, "Cookies")) {
      if (config->cookies == NULL) {
        wordexp_t p;
        if (wordexp(ptr, &p, 0) == 0) {
          if (p.we_wordc == 1) {
            config->cookies = strdup(p.we_wordv[0]);
            if (config->verbose >= 2) {
              printf("::DEBUG:: Using cookie file: %s\n", config->cookies);
            }
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
      config->persist = TRUE;
    } else {
      fprintf(stderr, "Error parsing config file: bad option '%s'\n", key);
      ret = 1;
      break;
    }
  }

  fclose(conf_fd);

  return(ret);
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
        config->persist = TRUE;
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
        return(1);
      default:
        return(1);
    }
  }

  /* Feed the remaining args into a linked list */
  while (optind < argc) {
    targets = llist_add(targets, strdup(argv[optind++]));
  }

  return(0);
}

static void cleanup(int ret) {
  llist_free(targets, free);

  if (config->cookies != NULL && ! config->persist) {
    if (config->verbose > 1) {
      printf("::DEBUG:: Deleting file %s\n", config->cookies);
    }

    unlink(config->cookies);
  }

  config_free(config);

  exit(ret);
}

static ssize_t xwrite(int fd, const void *buf, size_t count) {
  ssize_t ret;
  while ((ret = write(fd, buf, count)) == -1 && errno == EINTR);
  return(ret);
}

static void trap_handler(int signum) {
  int err = fileno(stderr);

  if (signum == SIGSEGV) {
    const char *msg = "An internal error occurred. Please submit a full bug "
                      "report with a backtrace if possible.\n";
    xwrite(err, msg, strlen(msg));
    exit(signum);
  } else if (signum == SIGINT) {
    struct termios t;
    const char *msg = "\nCaught user interrupt\n";

    tcgetattr(fileno(stdin), &t);
    t.c_lflag |= ECHO;
    tcsetattr(fileno(stdin), TCSANOW, &t);

    xwrite(err, msg, strlen(msg));
  }

  cleanup(signum);
}

int main(int argc, char **argv) {
  int ret = 0, cookie_valid = FALSE;
  struct sigaction new_action, old_action;

  new_action.sa_handler = trap_handler;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;

  sigaction(SIGINT, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN) {
    sigaction(SIGINT, &new_action, NULL);
  }
  if (old_action.sa_handler != SIG_IGN) {
    sigaction(SIGTERM, &new_action, NULL);
  }
  if (old_action.sa_handler != SIG_IGN) {
    sigaction(SIGSEGV, &new_action, NULL);
  }

  config = config_new();
  targets = NULL;

  ret = parseargs(argc, argv);
  if (ret != 0) {
    cleanup(ret);
  }

  /* Ensure we have a proper config environment */
  if (config->category == NULL) {
    config->category = "None";
  } else {
    config->catnum = category_is_valid(config->category);
  }

  if (config->catnum < 0) {
    usage_categories();
    cleanup(ret);
  }

  if (targets == NULL) {
    usage();
    cleanup(ret);
  }

  /* We can't read the config file without having verbosity set, but the
   * command line options need to take precedence over the config.  file.
   * Therefore, if ((user && pass) || cookie file) is supplied on the command
   * line, we won't read the config file.
   */
  if (! (config->user || config->cookies)) {
    read_config_file();
  }

  if (config->verbose > 1) {
    printf("::DEBUG:: Runtime options:\n");
    printf("  config->user = %s\n", config->user);
    printf("  config->password = %s\n", config->password);
    printf("  config->cookies = %s\n", config->cookies);
    printf("  config->persist = %s\n", config->persist ? "true" : "false");
    printf("  config->category = %s\n", config->category);
    printf("  config->verbose = %d\n", config->verbose);
  }

  /* Quick sanity check */
  if (config->persist && ! config->cookies) {
    fprintf(stderr, "%s: Error parsing options: do not specify persistent "
                    "cookies without providing a path to the cookie file.\n",
                    argv[0]);
    cleanup(ret);
  }

  /* Determine how we'll login -- either by cookie or credentials */
  if (config->cookies != NULL) { /* User specified cookie file */
    if (! access(config->cookies, R_OK) == 0) {
      if (touch(config->cookies) != 0) {
        fprintf(stderr, "Error creating cookie file: ");
        perror(config->cookies);
        cleanup(ret);
      }
    } else { /* assume its a real cookie file and evaluate it */
      long expire = cookie_expire_time(config->cookies, AUR_URL_NO_PROTO , AUR_COOKIE_NAME);
      if (expire > 0) {
        if (time(NULL) < expire) {
          cookie_valid = TRUE;
        }
        else
          fprintf(stderr, "Your cookie has expired. Gathering user and password...\n");
      }
    }
  } else { /* create PID based file in /tmp */
    if ((config->cookies = get_tmpfile(COOKIEFILE_FORMAT)) == NULL) {
      fprintf(stderr, "error creating cookie file.\n");
      cleanup(ret);
    }
  }

  if (! cookie_valid) {
    if (config->verbose > 1) {
      fprintf(stderr, "::DEBUG:: cookie auth will fail. Falling back to user/pass\n");
    }

    if (config->user == NULL) {
      config->user = get_username(AUR_USER_MAX);
    }

    if (config->password == NULL) {
      printf("[%s] ", config->user);
      config->password = get_password(AUR_PASSWORD_MAX);
    }
  }

  if (curl_global_init(CURL_GLOBAL_SSL) != 0 || curl_local_init() != 0) {
    fprintf(stderr, "Error: An error occurred while initializing curl\n");
    cleanup(ret);
  }

  if (cookie_valid || aur_login() == 0) {
    struct llist_t *l;
    for (l = targets; l; l = l->next)
      aur_upload((const char*)l->data);
  }

  if (config->verbose > 1) {
    printf("::DEBUG:: Cleaning up curl handle\n");
  }

  if (curl) {
    curl_easy_cleanup(curl);
  }

  curl_global_cleanup();

  cleanup(ret);
  /* never reached */
  return(0);
}
