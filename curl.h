/* Copyright (c) 2010-2011 Dave Reisner
 *
 * curl.h
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

#ifndef _BURP_CURL_H
#define _BURP_CURL_H

#include <curl/curl.h>

#define AUR_USER_MAX            16
#define AUR_PASSWORD_MAX        128

#ifndef AUR_PROTO
#  define AUR_PROTO               "https://"
#endif

#ifndef AUR_DOMAIN
#  define AUR_DOMAIN              "aur.archlinux.org"
#endif

#ifndef AUR_LOGIN_URL
#  define AUR_LOGIN_URL           AUR_PROTO AUR_DOMAIN "/"
#endif

#ifndef AUR_SUBMIT_URL
#  define AUR_SUBMIT_URL          AUR_PROTO AUR_DOMAIN "/pkgsubmit.php"
#endif

#define AUR_COOKIE_NAME         "AURSID"

#define AUR_LOGIN_FIELD         "user"
#define AUR_PASSWD_FIELD        "passwd"
#define AUR_REMEMBERME_FIELD    "remember_me"

#define AUR_LOGIN_FAIL_MSG      "Bad username or password."

#define ERROR_STARTTAG          "<p class=\"pkgoutput\">"
#define ERROR_ENDTAG            "</p>"

struct write_result {
  char *memory;
  size_t size;
};

int cookie_setup(void);
int curl_init(void);
void curl_cleanup(void);
long aur_login(void);
long aur_upload(const char*);

#endif /* _BURP_CURL_H */

/* vim: set et sw=2: */
