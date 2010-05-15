#ifndef _BURP_CONF_H
#define _BURP_CONF_H

#define COOKIEFILE_FORMAT  "/tmp/burp-%d.cookies"

struct config_t {
  char *user;
  char *password;
  char *cookies;
  char *category;
  int verbose;
};

int read_config_file(void);
struct config_t *config_new(void);
void config_free(struct config_t*);

extern struct config_t *config;

#endif /* _BURP_CONF_H */
