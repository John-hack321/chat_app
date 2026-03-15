/*
 * SCS3304 — One-on-One Chat Application
 * Authentication Module Implementation
 *
 * Implements password hashing using the djb2 algorithm and
 * password verification by comparing computed vs stored hashes.
 */
#include <stdio.h>
#include <string.h>
#include "auth.h"

/* 
 *
 * Initialise hash to 5381
 * For each character: hash = hash * 33 + char
 * Return final hash
 * */
unsigned int hash_password(const char *password) {
    unsigned int hash = 5381;
    int c;

    while ((c = (unsigned char)*password++))
        hash = ((hash << 5) + hash) + c;   /* hash * 33 + c */

    return hash;
}

/* 
 * Hash the plain-text password
 * Format the hash as a string
 * Compare with stored string
 * */
int verify_password(const char *plain, const char *stored) {
    char computed[16];
    snprintf(computed, sizeof(computed), "%u", hash_password(plain));
    return (strcmp(computed, stored) == 0);
}