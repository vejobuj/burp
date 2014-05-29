#ifndef _AUR_H
#define _AUR_H

#include <stdbool.h>

typedef struct aur_t aur_t;

int aur_new(aur_t **ret, const char *domainname, bool secure);
void aur_free(aur_t *aur);

int aur_set_username(aur_t *aur, const char *username);
int aur_set_password(aur_t *aur, const char *password);
int aur_set_cookiefile(aur_t *aur, const char *cookiefile);
int aur_set_debug(aur_t *aur, bool enable);

int aur_login(aur_t *aur, char **error);
int aur_logout(aur_t *aur);
int aur_upload(aur_t *aur, const char *tarball_path, const char *category,
    char **error);

/* vim: set et ts=2 sw=2: */

#endif  /* _AUR_H */
