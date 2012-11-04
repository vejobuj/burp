/* Copyright (c) 2010-2011 Dave Reisner
 *
 * curl.c
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "conf.h"
#include "curl.h"
#include "util.h"

static CURL *curl;

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
  struct write_result *mem = stream;
  size_t realsize = nmemb * size;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory) {
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
  }

  return realsize;
}

int curl_init() {
  if (curl_global_init(CURL_GLOBAL_SSL) != 0) {
    return 1;
  }

  curl = curl_easy_init();
  if (!curl) {
    return 1;
  }

  debug("initializing curl\n");

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  return 0;
}

int cookie_setup(void) {
  /* enable cookie management for this session */
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

  if (!config->cookie_file) {
    if (config->cookie_persist) {
      fprintf(stderr, "warning: ignoring --persist without path to cookie file\n");
    }
    return 0;
  }

  if (!access(config->cookie_file, F_OK) == 0) {
    if (touch(config->cookie_file) != 0) {
      fprintf(stderr, "error: failed to create cookie file: ");
      perror(config->cookie_file);
      return 1;
    }
  }

  if (config->cookie_persist) {
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, config->cookie_file);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, config->cookie_file);
  }

  return 0;
}

void curl_cleanup() {
  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

char *get_csrf_token() {
  struct curl_slist *i, *cookielist = NULL;
  char cname[256], token[256];

  curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookielist);
  for (i = cookielist; i; i = i->next) {
    int r = sscanf(i->data, "%*s\t%*s\t%*s\t%*s\t%*s\t%s\t%s", cname, token);
    if (r != 2) {
      continue;
    }
    if (strcmp(cname, "AURSID") != 0) {
      continue;
    }
    debug("AURSID cookie found with value: %s\n", token);
    break;
  }

  curl_slist_free_all(cookielist);

  return strdup(token);
}

long aur_login(void) {
  long httpcode, ret = 0;
  CURLcode status;
  struct curl_httppost *post, *last;
  struct curl_slist *headers = NULL;
  struct write_result response = { NULL, 0 };
  const char *persist = config->cookie_persist ? "on" : "";

  post = last = NULL;
  curl_formadd(&post, &last, CURLFORM_COPYNAME, AUR_LOGIN_FIELD,
      CURLFORM_COPYCONTENTS, config->user, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, AUR_PASSWD_FIELD,
      CURLFORM_COPYCONTENTS, config->password, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, AUR_REMEMBERME_FIELD,
      CURLFORM_COPYCONTENTS, persist, CURLFORM_END);

  if (config->verbose) {
    printf("submitting form:\n");
    printf("  config->user=%s\n", config->user);
    printf("  config->password=%s\n", config->password ? "--redacted--" : "");
    printf("  config->rememberme=%s\n", persist);
  }

  headers = curl_slist_append(headers, "Expect:");

  curl_easy_setopt(curl, CURLOPT_URL, AUR_LOGIN_URL);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  if (config->verbose > 0) {
    printf("Logging in to AUR as user %s\n", config->user);
  }

  status = curl_easy_perform(curl);
  if (status != CURLE_OK) {
    fprintf(stderr, "error: unable to send data to %s: %s\n", AUR_SUBMIT_URL,
        curl_easy_strerror(status));
    ret = status;
    goto cleanup;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    fprintf(stderr, "error: server responded with HTTP %ld\n", httpcode);
    ret = httpcode;
    goto cleanup;
  }

  debug("%s\n", response.memory);

  if (memmem(response.memory, response.size, AUR_LOGIN_FAIL_MSG,
        strlen(AUR_LOGIN_FAIL_MSG))) {
    fprintf(stderr, "Error: %s\n", AUR_LOGIN_FAIL_MSG);
    ret = 1L; /* Reuse an uncommon curl error */
  }

cleanup:
  free(response.memory);
  curl_slist_free_all(headers);
  curl_formfree(post);

  /* We're done using the password. Overwrite its memory */
  config->password = memset(config->password, 42, strlen(config->password));

  return ret;
}

static char *strip_html_tags(const char *unsanitized, size_t len) {
  int in_tag = 0;
  size_t i;
  char *ptr;
  char *sanitized;

  MALLOC(sanitized, len + 1, return NULL);
  ptr = sanitized;

  for (i = 0; i < len; i++) {
    switch (unsanitized[i]) {
      case '<':
        in_tag = 1;
        break;
      case '>':
        in_tag = 0;
        break;
      default:
        if (!in_tag) {
          *ptr++ = unsanitized[i];
        }
        break;
    }
  }

  *ptr++ = '\0';

  return sanitized;
}

void prime_cookielist() {
  curl_easy_setopt(curl, CURLOPT_URL, "file:///dev/null");
  curl_easy_perform(curl);
}

long aur_upload(const char *taurball, const char *csrf_token) {
  char *errormsg, *effective_url;
  char category[3], errbuffer[CURL_ERROR_SIZE] = {0};
  const char *display_name, *error_start, *error_end;
  long httpcode, ret = 1;
  CURLcode status;
  struct curl_httppost *post = NULL, *last = NULL;
  struct curl_slist *headers = NULL;
  struct write_result response = { NULL, 0 };
  struct stat st;

  /* make sure the resolved path is a regular file */
  if (stat(taurball, &st) != 0) {
    fprintf(stderr, "error: failed to stat `%s': %s\n", taurball, strerror(errno));
    return ret;
  }

  if (!S_ISREG(st.st_mode)) {
    fprintf(stderr, "error: `%s\' is not a file\n", taurball);
    return ret;
  }

  display_name = strrchr(taurball, '/');
  if (display_name) {
    display_name++;
  } else {
    display_name = taurball;
  }

  curl_formadd(&post, &last, CURLFORM_COPYNAME, "pkgsubmit",
      CURLFORM_COPYCONTENTS, "1", CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "pfile", 
      CURLFORM_FILE, taurball, CURLFORM_END);
  snprintf(category, 3, "%d", config->catnum);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "category",
      CURLFORM_COPYCONTENTS, category, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "token",
      CURLFORM_COPYCONTENTS, csrf_token);

  if (config->verbose) {
    printf("submitting form:\n");
    printf("  pkgsubmit=1\n");
    printf("  prfile=%s\n", taurball);
    printf("  category=%s\n", category);
    printf("  token=%s\n", csrf_token);
  }

  headers = curl_slist_append(headers, "Expect:");

  curl_easy_setopt(curl, CURLOPT_URL, AUR_SUBMIT_URL);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuffer);

  if (config->verbose) {
    printf("Uploading taurball: %s\n", display_name);
  }

  status = curl_easy_perform(curl);
  if (status != CURLE_OK) {
    fprintf(stderr, "error: unable to send data to %s: %s\n", AUR_SUBMIT_URL, errbuffer);
    goto cleanup;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    fprintf(stderr, "error: server responded with HTTP %ld\n", httpcode);
    goto cleanup;
  }

  debug("%s\n", response.memory);

  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
  if (effective_url) {
    /* TODO: this check could probably be better. it only ensures that we've
     * been redirected to _some_ packages page. */
    if (strstr(effective_url, "/packages/")) {
      printf("%s has been uploaded successfully.\n", display_name);
      ret = 0;
      goto cleanup;
    }
  }

  /* failboat */
  error_start = memmem(response.memory, response.size, ERROR_STARTTAG, strlen(ERROR_STARTTAG));
  if (error_start) {
    error_start += strlen(ERROR_STARTTAG);
    error_end = memmem(error_start, response.size - (error_start - response.memory),
        ERROR_ENDTAG, strlen(ERROR_ENDTAG));
    if (error_end) {
      errormsg = strip_html_tags(error_start, error_end - error_start);
      if (errormsg) {
        fprintf(stderr, "[AUR] %s\n", errormsg);
        FREE(errormsg);
      }
      goto cleanup;
    }
  }

  fprintf(stderr, "error: unexpected failure uploading `%s'\n", taurball);

cleanup:
  curl_slist_free_all(headers);
  curl_formfree(post);

  free(response.memory);

  return ret;
}

/* vim: set et sw=2: */
