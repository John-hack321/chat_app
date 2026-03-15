/*
 * SCS3304 — One-on-One Chat Application
 * Authentication Module Header
 *
 * Declares password hashing and verification functions.
 * Uses the djb2 algorithm — no external libraries required.
 */

#ifndef AUTH_H
#define AUTH_H

// for password length check during registaration
#define MIN_PASSWORD_LEN 4

/*
 * hash_password
 * Hash a plain-text password using djb2
 * password — plain-text string
 * unsigned int hash value
 */
unsigned int hash_password(const char *password);

/*
 *  verify_password
 * PURPOSE Check if a plain-text password matches a stored hash
 * INPUT plain   — plain-text password to check
 * stored  — stored hash string (as written in users.txt)
 * OUTPUT 1 if match, 0 if no match
 */
int verify_password(const char *plain, const char *stored);

#endif