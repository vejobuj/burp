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

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <getopt.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>

#include "llist.h"

#define USER_MAX      16
#define PASSWORD_MAX  128

#define AUR_LOGIN_FIELD     "user"
#define AUR_PASSWD_FIELD    "passwd"
#define AUR_LOGIN_FAIL_MSG  "Bad username or password."
#define AUR_LOGIN_URL       "http://aur.archlinux.org/"
#define AUR_SUBMIT_URL      "http://aur.archlinux.org/pkgsubmit.php"

#define FREE(x) do { free(x); x = NULL; } while (0)

struct config_t {
  char *user;
  char *password;
  char *cookies;
  int verbose;
};

static struct config_t *config;
static struct llist_t *targets;
static CURL *curl;

static struct config_t *config_new(struct config_t *config) {
  config = malloc(sizeof *config);
  if (config == NULL) {
    fprintf(stderr, "Error allocating %zd bytes for config.\n", sizeof *config);
    return NULL;
  }

  config->user = config->password = config->cookies = NULL;
  config->verbose = 0;

  return config;
}

static void config_free(struct config_t *config) {
  if (config->user)
    free(config->user);
  if (config->password)
    free(config->password);
  if (config->cookies)
    free(config->cookies);

  free(config);
}

static void usage() {
printf("burp v%s\n\
Usage: burp [options] PACKAGE [PACKAGE2..]\n\
\n\
 Options:\n\
  -u, --user                AUR login username\n\
  -p, --password            AUR login password\n\
  -c, FILE --cookies=FILE   save cookies to FILE\n\
  -v, --verbose             be more verbose. Pass twice for debug messages\n\n",
  VERSION);
}


static int parseargs(int argc, char **argv) {
  int opt;
  int option_index = 0;
  static struct option opts[] = {
    /* Operations */
    {"user",      required_argument,  0, 'u'},
    {"password",  required_argument,  0, 'p'},
    {"cookies",   required_argument,  0, 'c'},
    {"verbose",   no_argument,        0, 'v'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "u:p:c:v", opts, &option_index))) {
    if (opt < 0) {
      break;
    }

    switch (opt) {
      case 'u':
        if (config->user)
          FREE(config->user);
        config->user = strndup(optarg, USER_MAX);
        break;
      case 'p':
        if (config->password)
          FREE(config->password);
        config->password = strndup(optarg, PASSWORD_MAX);
        break;
      case 'c':
        if (config->cookies)
          FREE(config->cookies);
        config->cookies = strndup(optarg, PATH_MAX);
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

char *get_username(void) {
  char *buf;

  printf("Enter username: ");

  buf = calloc(1, USER_MAX + 1);

  /* fgets() will leave a newline char on the end */
  fgets(buf, USER_MAX, stdin);
  *(buf + strlen(buf) - 1) = '\0';

  return buf;
}

char *get_password(void) {
  struct termios t;
  char *buf;

  buf = calloc(1, PASSWORD_MAX + 1);
  printf("Enter password: ");

  /* turn off the echo flag */
  tcgetattr(fileno(stdin), &t);
  t.c_lflag &= ~ECHO;
  tcsetattr(fileno(stdin), TCSANOW, &t);

  /* fgets() will leave a newline char on the end */
  fgets(buf, PASSWORD_MAX, stdin);
  *(buf + strlen(buf) - 1) = '\0';

  putchar('\n');
  t.c_lflag |= ECHO;
  tcsetattr(fileno(stdin), TCSANOW, &t);

  return buf;
}

int create_cookie_file(const char *filename) {
  return 0;
}

int aur_login(void) {
  struct curl_httppost *post, *last;
  struct curl_slist *headers;

  curl = curl_easy_init();
  if (config->verbose > 1)
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

  post = last = NULL;
  headers = NULL;

  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, AUR_LOGIN_FIELD,
    CURLFORM_COPYCONTENTS, config->user, CURLFORM_END);
  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, AUR_PASSWD_FIELD,
    CURLFORM_COPYCONTENTS, config->password, CURLFORM_END);

  headers = curl_slist_append(headers, "Expect:");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "/home/haruko/cookies.txt");
  /* curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _______); */
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_URL, AUR_LOGIN_URL);
  curl_easy_perform(curl);

  curl_easy_cleanup(curl);

  return 0;
}

void read_config_file() {
  struct stat st;
  char *line;
  char config_path[PATH_MAX + 1];

  snprintf(&config_path[0], PATH_MAX, "%s/%s", 
    getenv("XDG_CONFIG_HOME"), "burp.conf");

  if (stat(config_path, &st) < 0) {
    if (config->verbose > 1)
      printf("::DEBUG:: No config file found\n");
    return;
  }

  if (config->verbose > 1)
    printf("::DEBUG:: Found config file\n");

  FILE *conf_fd = fopen(config_path, "r");
  getline(&line, NULL, conf_fd);

  printf("%s\n", line);

}

int main(int argc, char **argv) {

  int ret;
  struct llist_t *l;

  /* object creation */
  curl_global_init(CURL_GLOBAL_NOTHING);
  config = config_new(config);

  /* parse args */
  targets = NULL;
  ret = parseargs(argc, argv);

  if (config->verbose > 1) {
    printf("config->user = %s\n", config->user);
    printf("config->password = %s\n", config->password);
    printf("config->cookies = %s\n", config->cookies);
    printf("config->verbose = %d\n", config->verbose);
  }

  if (targets == NULL) {
    usage();
    goto cleanup;
  }


  /* Ensure we have a proper config environment */
  if (config->user == NULL || config->password == NULL)
    read_config_file();

  if (config->user == NULL)
    config->user = get_username();

  if (config->password == NULL)
    config->password = get_password();

  if (create_cookie_file(config->cookies) > 0) {
    perror("error creating cookie file");
    goto cleanup;
  }

  if (aur_login() > 0) {
    fprintf(stderr, "login error\n");
    goto cleanup;
  }

  for (l = targets; l; l = l->next) {
    printf("target: %s\n", (const char*)l->data);
    /* upload */
  }

  /* object destruction */
cleanup:
  curl_global_cleanup();
  llist_free(targets, free);
  config_free(config);

  return ret;
}
