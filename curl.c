/*
 *  curl.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/types.h>

#include "conf.h"
#include "curl.h"

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

void curl_local_init() {
  curl = curl_easy_init();

  if (config->verbose > 1) {
    printf("::DEBUG:: Initializing curl\n");
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
  }

  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, config->cookies);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
}

long aur_login(void) {
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

  if (config->verbose > 0)
    printf("Logging in to AUR as user %s\n", config->user);

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

  /* We're done using the password. Overwrite its memory */
  config->password = memset(config->password, 42, strlen(config->password));

  return ret;
}

long aur_upload(const char *taurball) {
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

  char category[3];
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

  if (config->verbose > 0)
    printf("Uploading taurball: %s\n", config->verbose > 1 ? fullpath : taurball);

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

  if (strstr(response.memory, AUR_NO_OVERWRITE) != NULL) {
    fprintf(stderr, "Error: You don't have permission to overwrite this file.\n");
    ret = 1;
  } else if (strstr(response.memory, AUR_UNKNOWN_FORMAT) != NULL) {
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

