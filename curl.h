#ifndef _BURP_CURL_H
#define _BURP_CURL_H

#define AUR_USER_MAX      16
#define AUR_PASSWORD_MAX  128

#define AUR_LOGIN_URL       "http://aur.archlinux.org/"
#define AUR_SUBMIT_URL      "http://aur.archlinux.org/pkgsubmit.php"

#define AUR_LOGIN_FIELD     "user"
#define AUR_PASSWD_FIELD    "passwd"

#define AUR_LOGIN_FAIL_MSG  "Bad username or password."
#define AUR_NO_OVERWRITE    "not allowed to overwrite"
#define AUR_UNKNOWN_FORMAT  "Unknown file format"

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
