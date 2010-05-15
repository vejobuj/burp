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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "conf.h"
#include "curl.h"
#include "llist.h"
#include "util.h"

static const char *categories[] = { "elephantitus",
  "none", "daemons", "devel", "editors", "emulators", "games", "gnome", "i18n",
  "kde", "lib", "modules", "multimedia", "network", "office", "science",
  "system", "x11", "xfce", "kernels", NULL};

static struct llist_t *targets;

static void usage() {
printf("burp %s\n\
Usage: burp [options] PACKAGE [PACKAGE2..]\n\
\n\
 Options:\n\
  -u, --user                AUR login username\n\
  -p, --password            AUR login password\n\
  -c CAT, --category=CAT    category to assign the uploaded package.\n\
                              This will default to the current category\n\
                              for pre-existing packages and 'None' for new\n\
                              packages. -c help will give a list of valid categories\n\
  -v, --verbose             be more verbose. Pass twice for debug messages\n\n",
  VERSION);
}

static void usage_categories() {
  printf("Valid categories are:\n");
  int i;
  for (i = 1; (categories[i]) != NULL; i++)
    printf("\t%s\n", categories[i]);
  putchar('\n');
}

static int category_is_valid(const char *cat) {
  int i;
  for (i = 1; (categories[i]) != NULL; i++)
    if (strcasecmp(categories[i], cat) == 0) {
      config->catnum = i;
      return 0;
    }

  return 1;
}

static int parseargs(int argc, char **argv) {
  int opt;
  int option_index = 0;
  static struct option opts[] = {
    {"user",      required_argument,  0, 'u'},
    {"password",  required_argument,  0, 'p'},
    {"category",  required_argument,  0, 'c'},
    {"verbose",   no_argument,        0, 'v'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "u:p:c:v", opts, &option_index))) {
    if (opt < 0) {
      break;
    }

    switch (opt) {
      case 'c':
        if (config->category)
          FREE(config->category);
        config->category = strndup(optarg, 16);
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
        usage();
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

int main(int argc, char **argv) {
  int ret;

  config = config_new();
  targets = NULL;

  ret = parseargs(argc, argv);

  if (config->verbose > 1) {
    printf("::DEBUG:: Command line options:\n");
    printf("  config->user = %s\n", config->user);
    printf("  config->password = %s\n", config->password);
    printf("  config->cookies = %s\n", config->cookies);
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

  if (config->user == NULL || config->password == NULL)
    if (read_config_file() != 0)
      goto cleanup;

  if (config->user == NULL)
     get_username(&(config->user), AUR_USER_MAX);

  if (config->password == NULL)
     get_password(&(config->password), AUR_PASSWORD_MAX);

  if ((get_tmpfile(&(config->cookies), COOKIEFILE_FORMAT)) != 0) {
    fprintf(stderr, "error creating cookie file");
    goto cleanup;
  } else
    if (config->verbose > 1)
      printf("::DEBUG:: Using cookie file: %s\n", config->cookies);

  curl_global_init(CURL_GLOBAL_NOTHING);
  curl_local_init();

  if (aur_login() == 0) {
    struct llist_t *l;
    for (l = targets; l; l = l->next)
      aur_upload((const char*)l->data);
  }

  if (curl != NULL)
    curl_easy_cleanup(curl);

  curl_global_cleanup();

cleanup:
  llist_free(targets, free);
  if (config->verbose > 1) {
    printf("::DEBUG:: Deleting file %s\n", config->cookies);
    delete_file(config->cookies);
  }
  config_free(config);

  return 0;
}
