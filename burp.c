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

#include <ctype.h>
#include <getopt.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "llist.h"

#define USER_MAX      16
#define PASSWORD_MAX  128

#define AUR_LOGIN_FIELD     "user"
#define AUR_PASSWD_FIELD    "passwd"
#define AUR_LOGIN_FAIL_MSG  "Bad username or password."
#define AUR_LOGIN_URL       "http://aur.archlinux.org/"
#define AUR_SUBMIT_URL      "http://aur.archlinux.org/pkgsubmit.php"

#define COOKIEFILE_DEFAULT  "/tmp/burp-%d.cookies"

#define FREE(x) do { free(x); x = NULL; } while (0)

static const char *categories[] = {
  "daemons", "devel", "editors", "emulators", "games", "gnome", "i18n", "kde",
  "lib", "modules", "multimedia", "network", "office", "science", "system",
  "x11", "xfce", "kernels", NULL};

struct config_t {
  char *user;
  char *password;
  char *cookies;
  char *category;
  int verbose;
};

struct write_result {
  char *memory;
  size_t size;
};

static struct config_t *config;
static struct llist_t *targets;
static CURL *curl;

static void *myrealloc(void *ptr, size_t size) {
  if (ptr)
    return realloc(ptr, size);
  else
    return calloc(1, size);
}

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {

  struct write_result *mem = (struct write_result*)stream;
  size_t realsize = nmemb * size;

  mem->memory = myrealloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory) {
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
  }
  return realsize;
}

static char *strtrim(char *str) {
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

static struct config_t *config_new(struct config_t *config) {
  config = malloc(sizeof *config);
  if (config == NULL) {
    fprintf(stderr, "Error allocating %zd bytes for config.\n", sizeof *config);
    return NULL;
  }

  config->user = config->password = config->cookies = config->category = NULL;
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
  if (config->category && strcmp(config->category, "None") != 0)
    free(config->category);

  free(config);
}

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
                              packages. -C help will give a list of valid categories\n\
  -v, --verbose             be more verbose. Pass twice for debug messages\n\n",
  VERSION);
}

static void usage_categories() {
  int i;

  printf("Valid categories are:\n");
  for (i = 0; (categories[i]) != NULL; i++)
    printf("\t%s\n", categories[i]);
  putchar('\n');

}

static int category_is_valid(const char *cat) {
  int i;

  for (i = 0; (categories[i]) != NULL; i++)
    if (strcmp(categories[i], cat) == 0)
      return 0;

  return 1;
}


static int parseargs(int argc, char **argv) {
  int opt;
  int option_index = 0;
  static struct option opts[] = {
    /* Operations */
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
        config->password = strndup(optarg, PASSWORD_MAX);
        break;
      case 'u':
        if (config->user)
          FREE(config->user);
        config->user = strndup(optarg, USER_MAX);
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

static char *get_username(void) {
  char *buf;

  printf("Enter username: ");

  buf = calloc(1, USER_MAX + 1);

  /* fgets() will leave a newline char on the end */
  fgets(buf, USER_MAX, stdin);
  *(buf + strlen(buf) - 1) = '\0';

  return buf;
}

static char *get_password(void) {
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

static int set_cookie_filepath(char **filename) {
  if (*filename != NULL)
    FREE(*filename);

  *filename = calloc(1, PATH_MAX + 1);
  snprintf(*filename, PATH_MAX, COOKIEFILE_DEFAULT, getpid());

  return *filename == NULL;
}

static void delete_file(const char *filename) {
  struct stat st;

  if (stat(filename, &st) == 0)
    unlink(filename);
}

static void curl_local_init() {
  curl = curl_easy_init();

  if (config->verbose > 1)
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, config->cookies);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
}

static long aur_login(void) {
  long ret, code;
  CURLcode status;
  struct curl_httppost *post, *last;
  struct curl_slist *headers;
  static struct write_result response;

  ret = 0;
  post = last = NULL;
  headers = NULL;
  response.memory = NULL;
  response.size = 0;

  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, AUR_LOGIN_FIELD,
    CURLFORM_COPYCONTENTS, config->user, CURLFORM_END);
  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, AUR_PASSWD_FIELD,
    CURLFORM_COPYCONTENTS, config->password, CURLFORM_END);

  headers = curl_slist_append(headers, "Expect:");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_URL, AUR_LOGIN_URL);

  status = curl_easy_perform(curl);
  if(status != 0) {
    fprintf(stderr, "curl error: unable to send data to %s\n", AUR_LOGIN_URL);
    fprintf(stderr, "%s\n", curl_easy_strerror(status));
    ret = status;
    goto cleanup;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if(code != 200) {
    fprintf(stderr, "curl error: server responded with code %ld\n", code);
    ret = code;
    goto cleanup;
  }

  if (strstr(response.memory, AUR_LOGIN_FAIL_MSG) != NULL) {
    fprintf(stderr, "Error: %s\n", AUR_LOGIN_FAIL_MSG);
    ret = 1L; /* Reuse an uncommon curl error */
  }

cleanup:
  free(response.memory);
  curl_slist_free_all(headers);
  curl_formfree(post);

  return ret;
}

static long aur_upload(const char *taurball) {
  char *fullpath;

  fullpath = realpath(taurball, NULL);
  if (fullpath == NULL) {
    fprintf(stderr, "Error uploading file '%s': ", taurball);
    perror("");
    return 1L;
  }

  long ret, code;
  CURLcode status;
  struct curl_httppost *post, *last;
  struct curl_slist *headers;
  static struct write_result response;

  ret = 0;
  post = last = NULL;
  headers = NULL;
  response.memory = NULL;
  response.size = 0;

  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, "pkgsubmit",
    CURLFORM_COPYCONTENTS, "1", CURLFORM_END);
  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, "category",
    CURLFORM_COPYCONTENTS, config->category, CURLFORM_END);
  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, "pfile",
    CURLFORM_FILE, fullpath, CURLFORM_END);

  headers = curl_slist_append(headers, "Expect:");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_URL, AUR_SUBMIT_URL);

  status = curl_easy_perform(curl);
  if (status != 0) {
    fprintf(stderr, "curl error: unable to send data to %s\n", AUR_SUBMIT_URL);
    fprintf(stderr, "%s\n", curl_easy_strerror(status));
    ret = status;
    goto cleanup;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if(code != 200) {
    fprintf(stderr, "curl error: server responded with code %ld\n", code);
    ret = code;
    goto cleanup;
  }

  if (strstr(response.memory, "not allowed to overwrite") != NULL) {
    fprintf(stderr, "Error: You don't have permission to overwrite this file.\n");
    ret = 1;
  } else if (strstr(response.memory, "Unknown file format") != NULL) {
    fprintf(stderr, "Error: Incorrect file format. Upload must conform to AUR "
                    "packaging guidelines.\n");
    ret = 1;
  } else {
    char *basename;
    if ((basename = strrchr(taurball, '/')) != NULL)
      printf("%s ", basename);
    else
      printf("%s ", taurball);
    printf("has been uploaded successfully.\n");
  }

cleanup:
  free(fullpath);
  free(response.memory);
  curl_slist_free_all(headers);
  curl_formfree(post);

  return ret;
}

static int read_config_file() {
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

int main(int argc, char **argv) {

  int ret;
  struct llist_t *l;

  config = config_new(config);

  /* parse args */
  targets = NULL;
  ret = parseargs(argc, argv);

  if (config->verbose > 1) {
    printf("config->user = %s\n", config->user);
    printf("config->password = %s\n", config->password);
    printf("config->cookies = %s\n", config->cookies);
    printf("config->category = %s\n", config->category);
    printf("config->verbose = %d\n", config->verbose);
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
    config->user = get_username();

  if (config->password == NULL)
    config->password = get_password();

  if ((set_cookie_filepath(&(config->cookies))) != 0) {
    fprintf(stderr, "error creating cookie file");
    goto cleanup;
  }

  curl_global_init(CURL_GLOBAL_NOTHING);
  curl_local_init();

  if (aur_login() == 0)
    for (l = targets; l; l = l->next)
      aur_upload((const char*)l->data);

  if (curl != NULL)
    curl_easy_cleanup(curl);
  curl_global_cleanup();

cleanup:
  llist_free(targets, free);
  delete_file(config->cookies);
  config_free(config);

  return 0;
}
