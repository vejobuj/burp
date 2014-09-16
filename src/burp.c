#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <wordexp.h>

#include "aur.h"
#include "log.h"
#include "util.h"

#ifdef GIT_VERSION
#undef PACKAGE_VERSION
#define PACKAGE_VERSION GIT_VERSION
#endif

static inline void aur_freep(aur_t **aur) { aur_free(*aur); }
#define _cleanup_aur_ _cleanup_(aur_freep)

struct category_t {
  const char *name;
  const char *id;
};

enum {
  OPT_DOMAIN = '~' + 1,
};

/* This list must be sorted */
/* TODO: move this list into aur.h, add aur_parse_category, etc */
static const struct category_t categories[] = {
  { "daemons",      "2" },
  { "devel",        "3" },
  { "editors",      "4" },
  { "emulators",    "5" },
  { "fonts",       "20" },
  { "games",        "6" },
  { "gnome",        "7" },
  { "i18n",         "8" },
  { "kde",          "9" },
  { "kernels",     "19" },
  { "lib",         "10" },
  { "modules",     "11" },
  { "multimedia",  "12" },
  { "network",     "13" },
  { "none",         "1" },
  { "office",      "14" },
  { "science",     "15" },
  { "system",      "16" },
  { "x11",         "17" },
  { "xfce",        "18" },
};

static const char *arg_category = "1";
static const char *arg_domain = "aur.archlinux.org";
static char *arg_username;
static char *arg_password;
static char *arg_cookiefile;
static int arg_loglevel = LOG_WARN;
static bool arg_expire;

static int category_compare(const void *a, const void *b) {
  const struct category_t *left = a;
  const struct category_t *right = b;
  return strcasecmp(left->name, right->name);
}

static const char *category_validate(const char *cat) {
  struct category_t key = { cat, NULL };
  struct category_t *res;

  res = bsearch(&key, categories, ARRAYSIZE(categories),
      sizeof(struct category_t), category_compare);

  return res ? res->id : NULL;
}

static char *find_config_file(void) {
  char *var, *out;

  var = getenv("XDG_CONFIG_HOME");
  if (var) {
    if (asprintf(&out, "%s/burp/burp.conf", var) < 0) {
      log_error("failed to allocate memory");
      return NULL;
    }
    return out;
  }

  var = getenv("HOME");
  if (var) {
    if (asprintf(&out, "%s/.config/burp/burp.conf", var) < 0){
      log_error("failed to allocate memory");
      return NULL;
    }
    return out;
  }

  return NULL;
}

static char *shell_expand(const char *in) {
  wordexp_t wexp;
  char *out = NULL;

  if (wordexp(in, &wexp, WRDE_NOCMD) < 0)
    return NULL;

  out = strdup(wexp.we_wordv[0]);
  wordfree(&wexp);
  if (out == NULL)
    return NULL;

  return out;
}

static size_t strtrim(char *str) {
  char *left = str, *right;

  if (!str || *str == '\0')
    return 0;

  while (isspace((unsigned char)*left))
    left++;

  if (left != str) {
    memmove(str, left, (strlen(left) + 1));
    left = str;
  }

  if (*str == '\0')
    return 0;

  right = (char*)rawmemchr(str, '\0') - 1;
  while (isspace((unsigned char)*right))
    right--;

  *++right = '\0';

  return right - left;
}

static int read_config_file(void) {
  _cleanup_fclose_ FILE *fp = NULL;
  char *config_path = NULL;
  char line[BUFSIZ];
  int lineno = 0;

  config_path = find_config_file();
  if (config_path == NULL) {
    log_warn("unable to determine location of config file. "
       "Skipping.\n");
    return 0;
  }

  fp = fopen(config_path, "r");
  if (fp == NULL) {
    if (errno == ENOENT)
      /* ignore error when file isn't found */
      return 0;

    log_error("failed to open %s: %s", config_path, strerror(errno));
    return -errno;
  }

  while (fgets(line, sizeof(line), fp) != NULL) {
    char *key, *value;
    size_t len;

    ++lineno;

    len = strtrim(line);
    if (len == 0 || line[0] == '#')
      continue;

    key = value = line;
    strsep(&value, "=");
    strtrim(key);
    strtrim(value);

    if (streq(key, "User")) {
      char *v = strdup(value);
      if (v == NULL)
        log_error("failed to allocate memory\n");
      else
        arg_username = v;
    } else if (streq(key, "Password")) {
      char *v = strdup(value);
      if (v == NULL)
        log_error("failed to allocate memory\n");
      else
        arg_password = v;
    } else if (streq(key, "Cookies")) {
      char *v = shell_expand(value);
      if (v == NULL)
        log_error("failed to allocate memory\n");
      else
        arg_cookiefile = v;
    } else
      log_warn("unknown config entry '%s' on line %d", key, lineno);
  }

  return 0;
}

static void __attribute__((noreturn)) print_version(void) {
  fputs(PACKAGE_NAME " v" PACKAGE_VERSION "\n", stdout);
  exit(EXIT_SUCCESS);
}

static void usage_categories(void) {
  fprintf(stderr, "Valid categories:\n");
  for (size_t i = 0; i < ARRAYSIZE(categories); ++i)
    fprintf(stderr, "\t%s\n", categories[i].name);
}

static void __attribute__((noreturn)) print_usage(void) {
  fprintf(stderr, "burp %s\n"
  "Usage: burp [options] targets...\n\n"
  " Options:\n"
  "  -u, --user                AUR login username.\n"
  "  -p, --password            AUR login password.\n"
  "  -c CAT, --category=CAT    Assign the uploaded package with category CAT.\n"
  "                              This will default to the current category\n"
  "                              for pre-existing packages and 'None' for new\n"
  "                              packages. -c help will give a list of valid\n"
  "                              categories.\n", PACKAGE_VERSION);
  fprintf(stderr,
  "  -e, --expire              Instead of uploading, expire the current session\n"
  /* leaving --domain undocumented for now */
  /* "      --domain=DOMAIN       Domain of the AUR (default: aur.archlinux.org)\n" */
  "  -C FILE, --cookies=FILE   Read and write login cookies from FILE. \n"
  "                              The file must be a valid Netscape cookie file.\n"
  "  -v, --verbose             be more verbose. Pass twice for debug info.\n\n"

  "  -h, --help                display this help and exit\n"
  "  -V, --version             display the version and exit\n\n"
  "  burp also honors a config file. See burp(1) for more information.\n\n");
  exit(EXIT_SUCCESS);
}

static int parseargs(int *argc, char ***argv) {
  static struct option option_table[] = {
    { "cookies",       required_argument,  0, 'C' },
    { "category",      required_argument,  0, 'c' },
    { "expire",        no_argument,        0, 'e' },
    { "help",          no_argument,        0, 'h' },
    { "password",      required_argument,  0, 'p' },
    { "user",          required_argument,  0, 'u' },
    { "version",       no_argument,        0, 'V' },
    { "verbose",       no_argument,        0, 'v' },
    { "domain",        required_argument,  0, OPT_DOMAIN },
    { NULL, 0, NULL, 0 },
  };

  for (;;) {
    int opt = getopt_long(*argc, *argv, "C:c:ehp:u:Vv", option_table, NULL);
    if (opt < 0)
      break;

    switch (opt) {
    case 'C':
      arg_cookiefile = optarg;
      break;
    case 'c':
      arg_category = category_validate(optarg);
      if (arg_category == NULL) {
        log_error("invalid category %s", optarg);
        usage_categories();
        return -EINVAL;
      }
      break;
    case 'e':
      arg_expire = true;
      break;
    case 'h':
      print_usage();
    case 'p':
      arg_password = optarg;
      break;
    case 'u':
      arg_username = optarg;
      break;
    case 'V':
      print_version();
    case 'v':
      ++arg_loglevel;
      break;
    case OPT_DOMAIN:
      arg_domain = optarg;
      break;
    default:
      return -EINVAL;
    }
  }

  *argv += optind;
  *argc -= optind;

  if (!arg_expire && *argc == 0) {
    log_error("error: no files specified (use -h for help)");
    return -EINVAL;
  }

  log_set_level(arg_loglevel);

  return 0;
}

static int log_login_error(int err, const char *html_error) {
  if (html_error) {
    log_error("%s", html_error);
    return -EXIT_FAILURE;
  }

  switch (-err) {
  case EBADR:
    log_error("insufficient credentials provided to login.");
    break;
  case EKEYEXPIRED:
    log_error("required login cookie has expired.");
    break;
  case EKEYREJECTED:
    log_error("login cookie not accepted.");
    break;
  default:
    log_error("failed to login to AUR: %s", strerror(-err));
    break;
  }

  return -EXIT_FAILURE;
}

static void echo_on(void) {
  struct termios t;
  tcgetattr(0, &t);
  t.c_lflag |= ECHO;
  tcsetattr(0, TCSANOW, &t);
}

static void echo_off(void) {
  struct termios t;
  tcgetattr(0, &t);
  t.c_lflag &= ~ECHO;
  tcsetattr(0, TCSANOW, &t);
}

static char *read_stdin(char *buf, int len, bool echo) {
  char *r;

  if (!echo)
    echo_off();

  r = fgets(buf, len, stdin);

  if (!echo) {
    putc('\n', stdout);
    echo_on();
  }

  if (r == NULL)
    return NULL;

  buf[strlen(buf) - 1] = '\0';

  return r;
}

static char *ask_username(void) {
  char *username, *r;

  username = malloc(128 + 1);
  if (username == NULL)
    return NULL;

  printf("Enter username: ");

  r = read_stdin(username, 128, true);
  if (r == NULL) {
    free(username);
    return NULL;
  }

  return username;
}

static char *ask_password(void) {
  char *passwd, *r;

  passwd = malloc(128 + 1);
  if (passwd == NULL)
    return NULL;

  printf("[%s] Enter password: ", arg_username);

  r = read_stdin(passwd, 128, false);
  if (r == NULL) {
    free(passwd);
    return NULL;
  }

  return passwd;
}

static int login(aur_t *aur) {
  int r;
  _cleanup_free_ char *username = NULL, *password = NULL, *error = NULL;

  if (arg_username == NULL) {

    username = ask_username();
    if (username == NULL)
      return log_login_error(ENOMEM, NULL);

    r = aur_set_username(aur, username);
    if (r < 0)
      return log_login_error(r, NULL);

    arg_username = username;
  }

  r = aur_login(aur, &error);
  if (r < 0) {
    switch (r) {
    case -EKEYEXPIRED:
      /* cookie expired */
      log_warn("Your cookie has expired -- using password login");
    /* fallthrough */
    case -ENOKEY:
      password = ask_password();
      if (password == NULL)
        return -ENOMEM;

      r = aur_set_password(aur, password);
      if (r < 0)
        return log_login_error(r, NULL);

      r = aur_login(aur, &error);
      break;
    }

    if (r < 0)
      return log_login_error(r, error);
  }

  return 0;
}

static int upload(aur_t *aur, char **packages, int package_count) {
  int r = 0;

  for (int i = 0; i < package_count; ++i) {
    _cleanup_free_ char *error = NULL;
    int k = aur_upload(aur, packages[i], arg_category, &error);
    if (k == 0)
      printf("success: uploaded %s\n", packages[i]);
    else {
      log_error("failed to upload %s: %s", packages[i],
          error ? error : strerror(-k));
      if (r == 0)
        r = k;
    }
  }

  return r;
}

static int create_aur_client(aur_t **aur) {
  int r;

  r = aur_new(aur, arg_domain, true);
  if (r < 0) {
    log_error("failed to create AUR client: %s", strerror(-r));
    return r;
  }

  if (arg_username)
    aur_set_username(*aur, arg_username);
  if (arg_password)
    aur_set_password(*aur, arg_password);
  if (arg_cookiefile)
    aur_set_cookiefile(*aur, arg_cookiefile);
  if (arg_loglevel >= LOG_DEBUG)
    aur_set_debug(*aur, true);

  return 0;
}

int main(int argc, char *argv[]) {
  _cleanup_aur_ aur_t *aur = NULL;

  if (read_config_file() < 0)
    return EXIT_FAILURE;

  if (parseargs(&argc, &argv) < 0)
    return EXIT_FAILURE;

  if (create_aur_client(&aur) < 0)
    return EXIT_FAILURE;

  if (arg_expire)
    return !!aur_logout(aur);

  if (login(aur) < 0)
    return EXIT_FAILURE;

  if (upload(aur, argv, argc) < 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

/* vim: set et ts=2 sw=2: */
