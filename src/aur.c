#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <curl/curl.h>

#include "aur.h"
#include "log.h"
#include "util.h"

struct aur_t {
  const char *proto;
  char *domainname;
  bool secure;

  char *username;
  char *password;
  char *aursid;

  char *cookies;
  bool persist_cookies;

  bool debug;

  CURL *curl;
};

struct form_element_t {
  CURLformoption keyoption;
  const char *key;
  CURLformoption valueoption;
  const char *value;
};

struct memblock_t {
  char *data;
  size_t len;
};

static inline void memblock_free(struct memblock_t *memblock) {
  free(memblock->data);
}
#define _cleanup_memblock_ _cleanup_(memblock_free)

static inline void formfreep(struct curl_httppost **form) {
  curl_formfree(*form);
}
#define _cleanup_form_ _cleanup_(formfreep)

static inline void slistfreep(struct curl_slist **slist) {
  curl_slist_free_all(*slist);
}
#define _cleanup_slist_ _cleanup_(slistfreep)

static size_t write_handler(void *ptr, size_t nmemb, size_t size, void *userdata) {
  struct memblock_t *response = userdata;
  size_t bytecount = size * nmemb;
  char *alloc;

  alloc = realloc(response->data, response->len + bytecount + 1);
  if (alloc == NULL)
    return 0;

  response->data = memcpy(alloc + response->len, ptr, bytecount);
  response->len += bytecount;
  response->data[response->len] = '\0';

  return bytecount;
}

static int touch(const char *filename) {
  return close(open(filename, O_WRONLY|O_CREAT|O_CLOEXEC|O_NOCTTY, 0644));
}

static int curl_reset(aur_t *aur) {
  if (aur->curl == NULL)
    aur->curl = curl_easy_init();
  else
    curl_easy_reset(aur->curl);

  if (aur->curl == NULL)
    return -ENOMEM;

  if (aur->cookies) {
    curl_easy_setopt(aur->curl, CURLOPT_COOKIEFILE, aur->cookies);
    if (aur->persist_cookies) {
      touch(aur->cookies);
      curl_easy_setopt(aur->curl, CURLOPT_COOKIEJAR, aur->cookies);
    }
  } else
    curl_easy_setopt(aur->curl, CURLOPT_COOKIEFILE, "");

  curl_easy_setopt(aur->curl, CURLOPT_WRITEFUNCTION, write_handler);

  return 0;
}

int aur_new(aur_t **ret, const char *domainname, bool secure) {
  aur_t *aur;

  aur = calloc(1, sizeof(*aur));
  if (aur == NULL)
    return -ENOMEM;

  aur->secure = secure;
  aur->proto = secure ? "https" : "http";
  aur->domainname = strdup(domainname);
  if (aur->domainname == NULL)
    return -ENOMEM;

  curl_global_init(CURL_GLOBAL_ALL);

  log_debug("created new AUR client for %s://%s", aur->proto,
      aur->domainname);

  *ret = aur;

  return 0;
}

void aur_free(aur_t *aur) {
  if (aur == NULL)
    return;

  log_debug("destroying AUR client for %s://%s", aur->proto,
      aur->domainname);

  free(aur->username);
  free(aur->cookies);
  free(aur->domainname);
  free(aur->aursid);
  free(aur->password);

  curl_easy_cleanup(aur->curl);
  curl_global_cleanup();
}

static int copy_string(char **field, const char *value) {
  char *newvalue = NULL;

  if (value != NULL) {
    newvalue = strdup(value);
    if (newvalue == NULL)
      return -ENOMEM;
  }

  free(*field);
  *field = newvalue;

  return 0;
}

int aur_set_username(aur_t *aur, const char *username) {
  return copy_string(&aur->username, username);
}

int aur_set_cookies(aur_t *aur, const char *cookies) {
  return copy_string(&aur->cookies, cookies);
}

int aur_set_persist_cookies(aur_t *aur, bool enabled) {
  aur->persist_cookies = enabled;
  return 0;
}

int aur_set_password(aur_t *aur, const char *password) {
  return copy_string(&aur->password, password);
}

int aur_set_debug(aur_t *aur, bool enable) {
  aur->debug = enable;
  return 0;
}

static bool is_package_url(const char *url) {
  return strstr(url, "/packages/") || strstr(url, "/pkgbase/");
}

static char *strip_html_tags(const char *in, size_t len) {
  int tag_depth = 0;
  size_t i;
  char *p, *out;

  out = malloc(len + 1);
  if (out == NULL)
    return NULL;

  p = out;
  for (i = 0; i < len; i++) {
    switch (in[i]) {
    case '<':
      ++tag_depth;
      break;
    case '>':
      --tag_depth;
      break;
    default:
      if (!tag_depth)
        *p++ = in[i];
      break;
    }
  }

  *p = '\0';
  return out;
}

static int extract_html(const char *html, const char *start_tag,
    const char *end_tag, char **text_out) {
  char *p, *q;

  /* find the start */
  p = strstr(html, start_tag);
  if (p == NULL)
    return -ENOENT;

  /* fast forward past the tag */
  p += strlen(start_tag);

  q = strstr(p, end_tag);
  if (q == NULL)
    return -EINVAL;

  *text_out = strip_html_tags(p, q - p);
  if (*text_out == NULL)
    return -ENOMEM;

  return 0;
}

static int extract_html_error(const char *html, char **error_out) {
  struct tagpair_t {
    const char *start;
    const char *end;
  } error_tags[] = {
    { "<p class=\"pkgoutput\">", "</p>" },   /* AUR before 3.0.0 */
    { "<ul class=\"errorlist\">", "</ul>" }, /* AUR >=3.0.0 */
    { NULL, NULL },
  };

  for (struct tagpair_t *tag = error_tags; tag->start; ++tag) {
    if (extract_html(html, tag->start, tag->end, error_out) == 0)
      return 0;
  }

  return -ENOENT;
}

static struct curl_httppost *make_form(const struct form_element_t *elements) {
  struct curl_httppost *post = NULL, *last = NULL;

  for (const struct form_element_t *elem = elements; elem->key; ++elem) {
    log_debug("  appending form field: %s=%s", elem->key, elem->value);
    if (curl_formadd(&post, &last, elem->keyoption, elem->key,
          elem->valueoption, elem->value, CURLFORM_END) != CURL_FORMADD_OK)
      return NULL;
  }

  return post;
}

static struct curl_httppost *make_login_form(aur_t *aur) {
  const struct form_element_t elements[] = {
    { CURLFORM_COPYNAME, "user", CURLFORM_COPYCONTENTS, aur->username },
    { CURLFORM_COPYNAME, "passwd", CURLFORM_COPYCONTENTS, aur->password },
    { CURLFORM_COPYNAME, "remember_me", CURLFORM_COPYCONTENTS, "on" },
    { 0, NULL, 0, NULL },
  };

  log_debug("building login form");

  return make_form(elements);
}

static struct curl_httppost *make_upload_form(aur_t *aur, const char *filepath,
    const char *category) {
  const struct form_element_t elements[] = {
    { CURLFORM_COPYNAME, "category", CURLFORM_COPYCONTENTS, category },
    { CURLFORM_COPYNAME, "token", CURLFORM_COPYCONTENTS, aur->aursid },
    { CURLFORM_COPYNAME, "pkgsubmit", CURLFORM_COPYCONTENTS, "1" },
    { CURLFORM_COPYNAME, "pfile", CURLFORM_FILE, filepath },
    { 0, NULL, 0, NULL },
  };

  log_debug("building upload form");

  return make_form(elements);
}

static bool domain_equals(const char *a, const char *b) {
  size_t a_len, b_len;

  /* ignore port numbers */
  a_len = strcspn(a, ":");
  b_len = strcspn(b, ":");

  return a_len == b_len && strncasecmp(a, b, a_len) == 0;
}

static int update_aursid_from_cookies(aur_t *aur) {
  _cleanup_slist_ struct curl_slist *cookielist = NULL;
  time_t now = time(NULL);

  curl_easy_getinfo(aur->curl, CURLINFO_COOKIELIST, &cookielist);

  for (struct curl_slist *i = cookielist; i; i = i->next) {
    _cleanup_free_ char *domain = NULL, *name = NULL, *aursid = NULL;
    long expire;

    log_debug("cookie=%s", i->data);

    if (sscanf(i->data, "%ms\t%*s\t%*s\t%*s\t%ld\t%ms\t%ms",
        &domain, &expire, &name, &aursid) != 4)
      continue;

    if (strncmp(domain, "#HttpOnly_", 10) == 0) {
      if (!domain_equals(domain + strlen("#HttpOnly_"), aur->domainname))
        continue;
    } else if (!domain_equals(domain, aur->domainname))
      continue;

    if (!streq(name, "AURSID"))
      continue;

    if (now >= expire)
      return -EKEYEXPIRED;

    log_debug("found valid cookie to use");

    aur->aursid = aursid;
    aursid = NULL;
    return 0;
  }

  /* if no cookie was found, expire any existing credentials */
  free(aur->aursid);
  aur->aursid = NULL;

  return -ENOKEY;
}

static void preload_cookiefile(aur_t *aur) {
  /* Hack alert! Prime the cookielist for inspection. */
  curl_easy_setopt(aur->curl, CURLOPT_URL, "file:///dev/null");
  curl_easy_perform(aur->curl);
}

static int aur_login_cookies(aur_t *aur) {
  int r;

  log_info("attempting login by cookie as user %s", aur->username);

  r = curl_reset(aur);
  if (r < 0)
    return r;

  preload_cookiefile(aur);

  return update_aursid_from_cookies(aur);
}

static char *aur_make_url(aur_t *aur, const char *uri) {
  char *url;
  int r;

  r = asprintf(&url, "%s://%s%s", aur->proto, aur->domainname, uri);
  if (r < 0)
    return NULL;

  return url;
}

static CURL *make_post_request(aur_t *aur, const char *path,
    struct curl_httppost *post) {
  char *url = NULL;

  url = aur_make_url(aur, path);
  if (url == NULL)
    return NULL;

  log_info("creating POST request to %s", url);
  curl_easy_setopt(aur->curl, CURLOPT_URL, url);
  free(url);

  curl_easy_setopt(aur->curl, CURLOPT_HTTPPOST, post);

  if (aur->debug)
    curl_easy_setopt(aur->curl, CURLOPT_VERBOSE, 1L);

  return aur->curl;
}

static long communicate(aur_t *aur, struct memblock_t *response) {
  long response_code;

  log_info("fetching response from remote");
  curl_easy_setopt(aur->curl, CURLOPT_WRITEDATA, response);

  if (curl_easy_perform(aur->curl) != CURLE_OK)
    return -1;

  curl_easy_getinfo(aur->curl, CURLINFO_RESPONSE_CODE, &response_code);
  log_info("server responded with status %ld", response_code);

  return response_code;
}

static int aur_login_password(aur_t *aur, char **error) {
  _cleanup_form_ struct curl_httppost *form = NULL;
  _cleanup_memblock_ struct memblock_t response = { NULL, 0 };
  char *effective_url = NULL;
  long http_status;
  int r;

  log_info("attempting login by password as user %s", aur->username);

  r = curl_reset(aur);
  if (r < 0)
    return r;

  form = make_login_form(aur);
  if (form == NULL)
    return -ENOMEM;

  aur->curl = make_post_request(aur, "/login", form);
  if (aur->curl == NULL)
    return -ENOMEM;

  http_status = communicate(aur, &response);
  if (http_status < 0 || http_status >= 400)
    return -EIO;

  curl_easy_getinfo(aur->curl, CURLINFO_REDIRECT_URL, &effective_url);
  if (effective_url == NULL) {
    r = extract_html_error(response.data, error);
    if (r < 0)
      return r;

    if (error)
      return -EIO;
  }

  return update_aursid_from_cookies(aur);
}

static char *ask_password(const char *user, const char *prompt, size_t maxlen) {
  struct termios t;
  char *buf;

  buf = malloc(maxlen + 1);
  if (buf == NULL)
    return NULL;

  printf("[%s] %s: ", user, prompt);

  tcgetattr(0, &t);
  t.c_lflag &= ~ECHO;
  tcsetattr(0, TCSANOW, &t);

  if (!fgets(buf, maxlen, stdin))
    return NULL;

  buf[strlen(buf) - 1] = '\0';

  putchar('\n');
  t.c_lflag |= ECHO;
  tcsetattr(0, TCSANOW, &t);

  return buf;
}

static int aur_login_interactive(aur_t *aur, char **error) {
  char *password;
  int r;

  password = ask_password(aur->username, "password", 1000);
  if (password == NULL)
    return -ENOMEM;

  r = aur_set_password(aur, password);
  if (r < 0)
    return r;

  return aur_login_password(aur, error);
}

int aur_login(aur_t *aur, bool force_password, char **error) {
  if (!aur->username)
    return -EBADR;

  if (!force_password && aur->cookies)
    return aur_login_cookies(aur);

  if (aur->password)
    return aur_login_password(aur, error);

  return aur_login_interactive(aur, error);
}

int aur_upload(aur_t *aur, const char *tarball_path,
    const char *category, char **error) {
  _cleanup_form_ struct curl_httppost *form = NULL;
  _cleanup_memblock_ struct memblock_t response = { NULL, 0 };
  long http_status;
  char *effective_url = NULL;
  struct stat st;
  int r;

  if (aur->aursid == NULL)
    return -ENOKEY;

  log_info("uploading %s with category %s", tarball_path, category);

  if (stat(tarball_path, &st) < 0)
    return -errno;

  if (!S_ISREG(st.st_mode))
    return -EINVAL;

  form = make_upload_form(aur, tarball_path, category);
  if (form == NULL)
    return -ENOMEM;

  aur->curl = make_post_request(aur, "/submit", form);
  if (aur->curl == NULL)
    return -ENOMEM;

  http_status = communicate(aur, &response);
  if (http_status < 0 || http_status >= 400)
    return -EIO;

  curl_easy_getinfo(aur->curl, CURLINFO_REDIRECT_URL, &effective_url);
  if (effective_url && is_package_url(effective_url))
    return 0;

  r = extract_html_error(response.data, error);
  if (r < 0)
    return r;

  return -EKEYREJECTED;
}

int aur_logout(aur_t *aur) {
  _cleanup_memblock_ struct memblock_t response = { NULL, 0 };
  long http_status;
  int r;

  log_info("logging out");

  r = curl_reset(aur);
  if (r < 0)
    return r;

  if (aur->aursid == NULL) {
    if (aur->cookies)
      preload_cookiefile(aur);
    else
      return 0;
  }

  aur->curl = make_post_request(aur, "/logout", NULL);
  if (aur->curl == NULL)
    return -ENOMEM;

  http_status = communicate(aur, &response);
  if (http_status >= 400)
    return -EIO;

  r = update_aursid_from_cookies(aur);
  if (r != -ENOKEY && r != -EKEYEXPIRED)
    return -EIO;

  return 0;
}

/* vim: set et ts=2 sw=2: */
