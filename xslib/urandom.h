/* $Id: urandom.h,v 1.3 2013/03/15 15:07:29 gremlin Exp $ */
#ifndef URANDOM_H_
#define URANDOM_H_

#include "xsdef.h"
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


bool urandom_has_device();


/*
 * Always return 0.
 */
int urandom_get_bytes(void *buf, size_t len);


/*
 * Return an integer in the range [a, b) if b > a. 
 * Otherwise, return an integer in the range [b, a).
 */
int urandom_get_int(int a, int b);


/*
 * return 0 on success, -1 on error.
 */
typedef int(*entropy_fun)(void *buf, size_t len);


/*
 * Return length of the generated id, or negative number on error.
 * The id will always begin with letter and be null-terminated if success.
 * The letters in base32 id is in lowercase.
 * 8 random bytes from entropy() get 12 chars of id.
 */
ssize_t base32id_from_entropy(char id[], size_t size, entropy_fun entropy); 


/*
 * Return length of the generated id, or negative number on error.
 * The id will always begin with letter and be null-terminated if success.
 * 8 random bytes from entropy() get 10 chars of id.
 */
ssize_t base57id_from_entropy(char id[], size_t size, entropy_fun entropy); 


/*
 * Same as base32id_from_entropy(id, size, urandom_get_bytes)
 */
ssize_t urandom_generate_base32id(char id[], size_t size); 


/*
 * Same as base57id_from_entropy(id, size, urandom_get_bytes)
 */
ssize_t urandom_generate_base57id(char id[], size_t size); 


#ifdef __cplusplus
}
#endif

#endif
