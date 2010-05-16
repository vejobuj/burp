/*
 *  curl.h
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

#ifndef _BURP_CURL_H
#define _BURP_CURL_H

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/types.h>

#define AUR_USER_MAX      16
#define AUR_PASSWORD_MAX  128

#define AUR_LOGIN_URL           "http://aur.archlinux.org/"
#define AUR_SUBMIT_URL          "http://aur.archlinux.org/pkgsubmit.php"

#define AUR_LOGIN_FIELD         "user"
#define AUR_PASSWD_FIELD        "passwd"
#define AUR_REMEMBERME_FIELD    "remember_me"

#define AUR_LOGIN_FAIL_MSG      "Bad username or password."
#define AUR_NO_OVERWRITE        "not allowed to overwrite"
#define AUR_UNKNOWN_FORMAT      "Unknown file format"

struct write_result {
  char *memory;
  size_t size;
};

void curl_local_init();
long aur_login(void);
long aur_upload(const char*);

CURL *curl;
extern CURL *curl;

#endif /* _BURP_CURL_H */
