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
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "conf.h"
#include "curl.h"
#include "util.h"

static CURL *curl;

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
  struct write_result *mem = (struct write_result*)stream;
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
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

  return 0;
}

int cookie_setup(void) {

  /* enable cookie management for this session */
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

  if (!config->cookie_file) {
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

long aur_login(void) {
  long httpcode, ret = 0;
  CURLcode status;
  struct curl_httppost *post, *last;
  struct curl_slist *headers;
  struct write_result response = { NULL, 0 };

  post = last = NULL;
  curl_formadd(&post, &last, CURLFORM_COPYNAME, AUR_LOGIN_FIELD,
      CURLFORM_COPYCONTENTS, config->user, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, AUR_PASSWD_FIELD,
      CURLFORM_COPYCONTENTS, config->password, CURLFORM_END);

  if (config->cookie_persist) {
    curl_formadd(&post, &last, CURLFORM_COPYNAME, AUR_REMEMBERME_FIELD,
        CURLFORM_COPYCONTENTS, "on", CURLFORM_END);
  }

  headers = NULL;
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

  if (strstr(response.memory, AUR_LOGIN_FAIL_MSG) != NULL) {
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

char *strip_html_tags(const char *unsanitized, size_t len) {
  int in_tag = 0;
  size_t i;
  char *ptr;
  char *sanitized;

  MALLOC(sanitized, len, return NULL);
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

long aur_upload(const char *taurball) {
  char *errormsg, *fullpath, *effective_url;
  char category[3], errbuffer[CURL_ERROR_SIZE] = {0};
  const char *error_start, *error_end, *redir_page = NULL;
  const char * const packages_php = "packages.php";
  long httpcode, ret = 1;
  CURLcode status;
  struct curl_httppost *post, *last;
  struct curl_slist *headers;
  struct write_result response = { NULL, 0 };
  struct stat st;

  fullpath = realpath(taurball, NULL);
  if (fullpath == NULL) {
    fprintf(stderr, "Error uploading file '%s': ", taurball);
    perror("");
    return ret;
  }

  /* make sure the resolved path is a regular file */
  if (stat(fullpath, &st) != 0) {
    perror("stat");
    return ret;
  }

  if (!S_ISREG(st.st_mode)) {
    fprintf(stderr, "skipping target `%s\': not a file\n", taurball);
    return ret;
  }

  snprintf(category, 3, "%d", config->catnum);

  post = last = NULL;
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "pkgsubmit",
      CURLFORM_COPYCONTENTS, "1", CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "category",
      CURLFORM_COPYCONTENTS, category, CURLFORM_END);
  curl_formadd(&post, &last, CURLFORM_COPYNAME, "pfile", 
      CURLFORM_FILE, fullpath, CURLFORM_END);

  headers = NULL;
  headers = curl_slist_append(headers, "Expect:");

  curl_easy_setopt(curl, CURLOPT_URL, AUR_SUBMIT_URL);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuffer);

  if (config->verbose) {
    printf("Uploading taurball: %s\n", fullpath);
  }

  status = curl_easy_perform(curl);

  curl_slist_free_all(headers);
  curl_formfree(post);
  free(fullpath);

  if (status != CURLE_OK) {
    fprintf(stderr, "error: unable to send data to %s: %s\n", AUR_SUBMIT_URL, errbuffer);
    return ret;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if (httpcode != 200) {
    fprintf(stderr, "error: server responded with HTTP %ld\n", httpcode);
    goto cleanup;
  }

  debug("%s\n", response.memory);

  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
  if (effective_url) {
    redir_page = strrchr(effective_url, '/') + 1;
    if (strncmp(redir_page, packages_php, strlen(packages_php)) == 0) {
      printf("%s has been uploaded successfully.\n", taurball);
      ret = 0;
      goto cleanup;
    }
  }

  /* failboat */
  error_start = strstr(response.memory, STARTTAG);
  if (error_start) {
    error_start += strlen(STARTTAG);
    error_end = strstr(error_start, ENDTAG);
    if (error_end) {
      errormsg = strip_html_tags(error_start, error_end - error_start);
      if (errormsg) {
        fprintf(stderr, "error: %s\n", errormsg);
        FREE(errormsg);
      }
      goto cleanup;
    }
  }

  fprintf(stderr, "error: unexpected failure uploading `%s'\n", taurball);

cleanup:
  free(response.memory);

  return ret;
}

