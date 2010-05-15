#ifndef _BURP_UTIL_H
#define _BURP_UTIL_H

#define FREE(x) do { free(x); x = NULL; } while (0)

void delete_file(const char*);
void get_password(char**, int);
int get_tmpfile(char**, const char*);
void get_username(char**, int);
char *strtrim(char*);

#endif /* _BURP_UTIL_H */
