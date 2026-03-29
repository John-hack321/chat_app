#ifndef AUTH_H
#define AUTH_H

// for password length check during registaration
#define MIN_PASSWORD_LEN 4

// the function definition are just as the function names state.

unsigned int hash_password(const char *password);

int verify_password(const char *plain, const char *stored);

#endif