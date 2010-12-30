/* Copyright (c) 2010 Dave Reisner
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "curl.h"

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
  struct write_result *mem = (struct write_result*)stream;
  size_t realsize = nmemb * size;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory) {
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
  }

  return(realsize);
}

int curl_local_init() {
  curl = curl_easy_init();

  if (! curl) {
    return(1);
  }

  if (config->verbose > 1) {
    printf("::DEBUG:: Initializing curl\n");
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
  }

  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, config->cookies);
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, config->cookies);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

  return(0);
}

long aur_login(void) {
  long code, ret = 0;
  CURLcode status;
  struct curl_httppost *post, *last;
  struct curl_slist *headers;
  static struct write_result response;

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

  if (config->persist) {
    curl_formadd(&post, &last,
      CURLFORM_COPYNAME, AUR_REMEMBERME_FIELD,
      CURLFORM_COPYCONTENTS, "on", CURLFORM_END);
  }

  headers = curl_slist_append(headers, "Expect:");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_URL, AUR_LOGIN_URL);

  if (config->verbose > 0) {
    printf("Logging in to AUR as user %s\n", config->user);
  }

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

  if (config->verbose > 1) {
    printf("%s\n", response.memory);
  }

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

  return(ret);
}

long aur_upload(const char *taurball) {
  char *ptr, *fullpath;
  char missing_var[10], category[3];
  long httpcode, ret = 0;
  CURLcode status;
  struct curl_httppost *post, *last;
  struct curl_slist *headers;
  static struct write_result response;

  fullpath = realpath(taurball, NULL);
  if (fullpath == NULL) {
    fprintf(stderr, "Error uploading file '%s': ", taurball);
    perror("");
    return(1L);
  }

  post = last = NULL;
  headers = NULL;
  response.memory = NULL;
  response.size = 0;

  snprintf(category, 3, "%d", config->catnum);

  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, "pkgsubmit",
    CURLFORM_COPYCONTENTS, "1", CURLFORM_END);
  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, "category",
    CURLFORM_COPYCONTENTS, category, CURLFORM_END);
  curl_formadd(&post, &last,
    CURLFORM_COPYNAME, "pfile",
    CURLFORM_FILE, fullpath, CURLFORM_END);

  headers = curl_slist_append(headers, "Expect:");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_URL, AUR_SUBMIT_URL);

  if (config->verbose > 0) {
    printf("Uploading taurball: %s\n", config->verbose > 1 ? fullpath : taurball);
  }

  status = curl_easy_perform(curl);
  if (status != CURLE_OK) {
    fprintf(stderr, "curl error: unable to send data to %s\n", AUR_SUBMIT_URL);
    fprintf(stderr, "%s\n", curl_easy_strerror(status));
    ret = status;
    goto cleanup;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
  if(httpcode != 200) {
    fprintf(stderr, "curl error: server responded with code %ld\n", httpcode);
    ret = httpcode;
    goto cleanup;
  }

  if (config->verbose > 1) {
    printf("%s\n", response.memory);
  }

  if (strstr(response.memory, AUR_NO_LOGIN)) {
    fprintf(stderr, "Error: Authentication failed on upload.\n");
    ret = 1;
  } else if (strstr(response.memory, AUR_NO_OVERWRITE)) {
    fprintf(stderr, "Error: You don't have permission to overwrite this file.\n");
    ret = 1;
  } else if (strstr(response.memory, AUR_UNKNOWN_FORMAT)) {
    fprintf(stderr, "Error: Incorrect file format. Upload must conform to AUR "
                    "packaging guidelines.\n");
    ret = 1;
  } else if (strstr(response.memory, AUR_INVALID_NAME)) {
    fprintf(stderr, "Error: Invalid package name. Only lowercase letters are "
                    "allowed. Make sure this isn't a split package.\n");
    ret = 1;
  } else if (strstr(response.memory, AUR_NO_PKGBUILD)) {
    fprintf(stderr, "Error: PKGBUILD does not exist in uploaded source.\n");
    ret = 1;
  } else if (strstr(response.memory, AUR_NO_BUILD_FUNC)) {
    fprintf(stderr, "Error: PKGBUILD is missing build function.\n");
    ret = 1;
  } else if (strstr(response.memory, AUR_MISSING_PROTO)) { 
    fprintf(stderr, "Error: Package URL is missing a protocol\n");
    ret = 1;
  } else if ((ptr = strstr(response.memory, "Missing")) && 
              sscanf(ptr, AUR_MISSING_VAR, missing_var)) {
    fprintf(stderr, "Error: Package is missing %s variable\n", missing_var);
    ret = 1;
  } else {
    printf("%s has been uploaded successfully.\n", basename(taurball));
  }

cleanup:
  free(fullpath);
  free(response.memory);
  curl_slist_free_all(headers);
  curl_formfree(post);

  return(ret);
}

