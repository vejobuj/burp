/*
 *  burp.c
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

#include <errno.h>
#include <getopt.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "curl.h"
#include "llist.h"
#include "util.h"

#define NUM_CATEGORIES (sizeof(categories)/sizeof(categories[0]))

static struct llist_t *targets;

struct category {
  const char *name;
  int num;
};

static struct category categories[] = { 
  { "none", 1 },      { "daemons", 2 },   { "devel", 3 },
  { "editors", 4 },   { "emulators", 5 }, { "games", 6 },
  { "gnome", 7 },     { "i18n", 8 },      { "kde", 9 },
  { "lib", 10 },      { "modules", 11 },  { "multimedia", 12 },
  { "network", 13 },  { "office", 14 },   { "science", 15 },
  { "system", 16 },   { "x11", 17 },      { "xfce", 18 },
  { "kernels", 19 }
};

static int fn_cmp_cat (const void *c1, const void *c2) {
  struct category *cat1 = (struct category*)c1;
  struct category *cat2 = (struct category*)c2;

  return strcmp(cat1->name, cat2->name);
}

static int category_is_valid(const char *cat) {
  struct category key;

  key.name = cat;

  return ! (bsearch(&key, categories, NUM_CATEGORIES, sizeof(struct category), fn_cmp_cat));
}

static void usage() {
printf("burp %s\n\
Usage: burp [options] PACKAGE [PACKAGE2..]\n\
\n\
 Options:\n\
  -u, --user                AUR login username.\n\
  -p, --password            AUR login password.\n\
  -c CAT, --category=CAT    Assign the uploaded package with category CAT.\n\
                              This will default to the current category\n\
                              for pre-existing packages and 'None' for new\n\
                              packages. -c help will give a list of valid\n\
                              categories.\n\
  -C FILE, --cookies=FILE   Use FILE to store cookies rather than the default\n\
                              temporary file. Useful with the -k option.\n\
  -k, --keep-cookies        Cookies will be persistent and reused for logins.\n\
                              If you specify this option, you must also provide\n\
                              a path to a cookie file.\n\
  -v, --verbose             be more verbose. Pass twice for debug info.\n\n\
  burp also honors a config file. See burp(1) for more information.\n\n",
  VERSION);
}

static void usage_categories() {
  printf("Valid categories are:\n");
  int i;
  for (i = 0; i < NUM_CATEGORIES; i++)
    printf("\t%s\n", categories[i].name);
  putchar('\n');
}

static int parseargs(int argc, char **argv) {
  int opt;
  int option_index = 0;
  static struct option opts[] = {
    {"user",          required_argument,  0, 'u'},
    {"password",      required_argument,  0, 'p'},
    {"keep-cookies",  no_argument,        0, 'k'},
    {"category",      required_argument,  0, 'c'},
    {"cookies",       required_argument,  0, 'C'},
    {"verbose",       no_argument,        0, 'v'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "u:p:kc:C:v", opts, &option_index))) {
    if (opt < 0) {
      break;
    }

    switch (opt) {
      case 'c':
        if (config->category)
          FREE(config->category);
        config->category = strndup(optarg, 16);
        break;
      case 'C':
        if (config->cookies)
          FREE(config->cookies);
        config->cookies = strndup(optarg, PATH_MAX);
        break;
      case 'k':
        config->persist = TRUE;
        break;
      case 'p':
        if (config->password)
          FREE(config->password);
        config->password = strndup(optarg, AUR_PASSWORD_MAX);
        break;
      case 'u':
        if (config->user)
          FREE(config->user);
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

  /* Feed the remaining args into a linked list */
  while (optind < argc)
    targets = llist_add(targets, strdup(argv[optind++]));

  return 0;
}

void trap_handler(int signal) {
  if (config->verbose > 0)
    fprintf(stderr, "\nCaught user interrupt, exiting...\n");

  if (curl != NULL) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
  }

  llist_free(targets, free);
  if (config->cookies != NULL && ! config->persist) {
    if (config->verbose > 1)
      printf("::DEBUG:: Deleting file %s\n", config->cookies);

    delete_file(config->cookies);
  }

  config_free(config);

  exit(1);
}

int main(int argc, char **argv) {
  int ret;

  signal(SIGINT, trap_handler);

  config = config_new();
  targets = NULL;

  ret = parseargs(argc, argv);

  if (config->verbose > 1) {
    printf("::DEBUG:: Command line options:\n");
    printf("  config->user = %s\n", config->user);
    printf("  config->password = %s\n", config->password);
    printf("  config->cookies = %s\n", config->cookies);
    printf("  config->persist = %s\n", config->persist ? "true" : "false");
    printf("  config->category = %s\n", config->category);
    printf("  config->verbose = %d\n", config->verbose);
  }

  /* Ensure we have a proper config environment */
  if (config->category == NULL)
    config->category = "None";
  else
    if (category_is_valid(config->category) > 0) {
      usage_categories();
      goto cleanup;
    }

  if (targets == NULL) {
    usage();
    goto cleanup;
  }

  read_config_file();

  /* Quick sanity check */
  if (config->persist && config->cookies == NULL) {
    fprintf(stderr, "Conflicting options: do not specify -k without -C\n");
    usage();
    goto cleanup;
  }

  int cookie_valid = FALSE;
  /* Determine how we'll login -- either by cookie or credentials */
  if (config->cookies != NULL) { /* User specified cookie file */
    if (! file_exists(config->cookies)) {
      if (touch(config->cookies) != 0) {
        fprintf(stderr, "Error creating cookie file: ");
        perror(config->cookies);
        goto cleanup;
      }
    } else { /* assume its a real cookie file and evaluate it */
      if (cookie_expire_time(config->cookies, AUR_URL_NO_PROTO , AUR_COOKIE_NAME) > 0)
        cookie_valid = TRUE;
    }
  } else { /* create PID based file in /tmp */
    if ((config->cookies = get_tmpfile(COOKIEFILE_FORMAT)) == NULL) {
      fprintf(stderr, "error creating cookie file.\n");
      goto cleanup;
    }
  }

  if (! cookie_valid) {
    if (config->verbose > 1)
      fprintf(stderr, "::DEBUG:: cookie auth will fail. Falling back to user/pass\n");

    if (config->user == NULL)
      config->user = get_username(AUR_USER_MAX);
    if (config->password == NULL)
      config->password = get_password(AUR_PASSWORD_MAX);
  }

  curl_global_init(CURL_GLOBAL_NOTHING);
  curl_local_init();

  if (cookie_valid || aur_login() == 0) {
    struct llist_t *l;
    for (l = targets; l; l = l->next)
      aur_upload((const char*)l->data);
  }

  if (curl)
    curl_easy_cleanup(curl);

  curl_global_cleanup();

cleanup:
  llist_free(targets, free);
  if (config->cookies != NULL && ! config->persist) {
    if (config->verbose > 1)
      printf("::DEBUG:: Deleting file %s\n", config->cookies);

    delete_file(config->cookies);
  }
  config_free(config);

  return 0;
}
