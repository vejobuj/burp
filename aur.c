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
#include "util.h"

#define AUR_LOGIN_FAIL_MSG      "Bad username or password."

struct aur_t {
  const char *proto;
  char *domainname;
  bool secure;

  char *username;
  char *password;
  char *aursid;

  char *cookies;
  bool persist_cookies;

  CURL *curl;
  struct curl_slist *extra_headers;
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

static inline void formfreep(struct curl_httppost **form) {
  curl_formfree(*form);
}
#define _cleanup_formfree_ _cleanup_(formfreep)

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

  aur->curl = curl_easy_init();
  if (aur->curl == NULL)
    return -ENOMEM;

  /* enable cookie handling */
  curl_easy_setopt(aur->curl, CURLOPT_COOKIEFILE, "");

  *ret = aur;

  return 0;
}

void aur_free(aur_t *aur) {
  if (aur == NULL)
    return;

  free(aur->username);
  free(aur->cookies);
  free(aur->domainname);
  free(aur->aursid);
  free(aur->password);

  curl_easy_cleanup(aur->curl);
  curl_slist_free_all(aur->extra_headers);
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

static struct curl_httppost *make_form(const struct form_element_t *elements) {
  struct curl_httppost *post = NULL, *last = NULL;

  for (const struct form_element_t *elem = elements; elem->key; ++elem) {
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
    { CURLFORM_COPYNAME, "remember_me", CURLFORM_COPYCONTENTS,
      aur->persist_cookies ? "on" : "" },
    { 0, NULL, 0, NULL },
  };

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

  return make_form(elements);
}

static int update_aursid_from_cookies(aur_t *aur, bool verify_expiration) {
  struct curl_slist *i, *cookielist = NULL;
  int r = -ENOKEY;

  curl_easy_getinfo(aur->curl, CURLINFO_COOKIELIST, &cookielist);

  for (i = cookielist; i; i = i->next) {
    _cleanup_free_ char *domain = NULL, *name = NULL, *aursid = NULL;
    long expire;

    if (sscanf(i->data, "%ms\t%*s\t%*s\t%*s\t%ld\t%ms\t%ms",
        &domain, &expire, &name, &aursid) != 4)
      continue;

    if (strncmp(domain, "#HttpOnly_", 10) == 0) {
      if (!streq(domain + strlen("#HttpOnly_"), aur->domainname))
        continue;
    } else if (!streq(domain, aur->domainname)) {
      continue;
    }

    if (verify_expiration && time(NULL) > expire)
      return -EKEYEXPIRED;

    if (!streq(name, "AURSID")) {
      continue;
    }

    aur->aursid = aursid;
    aursid = NULL;

    r = 0;
    break;
  }

  curl_slist_free_all(cookielist);

  return r;
}

static int touch(const char *filename) {
  return close(open(filename, O_WRONLY|O_CREAT|O_CLOEXEC|O_NOCTTY, 0644));
}

static int aur_login_cookies(aur_t *aur) {
  if (aur->cookies)
    curl_easy_setopt(aur->curl, CURLOPT_COOKIEFILE, aur->cookies);

  if (aur->persist_cookies) {
    touch(aur->cookies);
    curl_easy_setopt(aur->curl, CURLOPT_COOKIEJAR, aur->cookies);
  }

  /* Hack alert! Prime the cookielist for inspection. */
  curl_easy_setopt(aur->curl, CURLOPT_URL, "file:///dev/null");
  curl_easy_perform(aur->curl);

  return update_aursid_from_cookies(aur, true);
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
  struct curl_slist *headers = NULL;
  char *url = NULL;

  url = aur_make_url(aur, path);
  if (url == NULL)
    return NULL;

  curl_easy_setopt(aur->curl, CURLOPT_URL, url);
  free(url);

  if (aur->extra_headers == NULL) {
    aur->extra_headers = curl_slist_append(headers, "Expect:");
    if (aur->extra_headers == NULL)
      return NULL;
  }

  curl_easy_setopt(aur->curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(aur->curl, CURLOPT_HTTPHEADER, aur->extra_headers);
  /* curl_easy_setopt(aur->curl, CURLOPT_VERBOSE, 1L); */

  return aur->curl;
}

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

static int aur_communicate(aur_t *aur, struct memblock_t *response) {
  CURLcode r;

  curl_easy_setopt(aur->curl, CURLOPT_WRITEDATA, response);
  curl_easy_setopt(aur->curl, CURLOPT_WRITEFUNCTION, write_handler);

  r = curl_easy_perform(aur->curl);

  curl_easy_setopt(aur->curl, CURLOPT_WRITEDATA, NULL);
  curl_easy_setopt(aur->curl, CURLOPT_WRITEFUNCTION, NULL);

  return r != 0;
}

static int aur_login_password(aur_t *aur) {
  _cleanup_formfree_ struct curl_httppost *form = NULL;
  _cleanup_free_ char *buf = NULL;
  struct memblock_t response = { NULL, 0 };
  int r = 0;

  form = make_login_form(aur);
  if (form == NULL)
    r = -ENOMEM;

  aur->curl = make_post_request(aur, "/login", form);
  if (aur->curl == NULL)
    return -ENOMEM;

  r = aur_communicate(aur, &response);
  buf = response.data;
  if (r < 0) {
    fprintf(stderr, "error: failed to communicate: %s\n", strerror(-r));
    return r;
  }

  if (response.data) {
    if (strstr(response.data, AUR_LOGIN_FAIL_MSG) != NULL)
      return -EACCES;
  } else
    return -EFAULT;

  return update_aursid_from_cookies(aur, false);
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

  if (!fgets(buf, maxlen, stdin)) {
    fprintf(stderr, "failed to read from stdin\n");
    return NULL;
  }

  buf[strlen(buf) - 1] = '\0';

  putchar('\n');
  t.c_lflag |= ECHO;
  tcsetattr(0, TCSANOW, &t);

  return buf;
}

static int aur_login_interactive(aur_t *aur) {
  char *password;
  int r;

  password = ask_password(aur->username, "password", 1000);
  if (password == NULL)
    return -ENOMEM;

  r = aur_set_password(aur, password);
  if (r < 0)
    return r;

  return aur_login_password(aur);
}

int aur_login(aur_t *aur, bool force_password) {
  if (!aur->username)
    return -EBADR;

  if (!force_password && aur->cookies)
    return aur_login_cookies(aur);

  if (aur->password)
    return aur_login_password(aur);

  return aur_login_interactive(aur);
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

static int extract_upload_error(const char *html, char **error_out) {
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

int aur_upload(aur_t *aur, const char *tarball_path,
    const char *category, char **error) {
  _cleanup_formfree_ struct curl_httppost *form = NULL;
  _cleanup_free_ char *buf = NULL;
  long http_status;
  char *effective_url;
  struct stat st;
  struct memblock_t response = { NULL, 0 };
  int r;

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

  r = aur_communicate(aur, &response);
  buf = response.data;
  if (r < 0)
    return r;

  curl_easy_getinfo(aur->curl, CURLINFO_RESPONSE_CODE, &http_status);
  if (http_status >= 400)
    return -EIO;

  curl_easy_getinfo(aur->curl, CURLINFO_REDIRECT_URL, &effective_url);
  if (effective_url && (strstr(effective_url, "/packages/") ||
        strstr(effective_url, "/pkgbase/")))
    return 0;

  r = extract_upload_error(response.data, error);
  if (r < 0)
    return r;

  return -EKEYREJECTED;
}

/* vim: set et ts=2 sw=2: */
