/* $Id: urandom.h,v 1.3 2013/03/15 15:07:29 gremlin Exp $ */
#ifndef URANDOM_H_
#define URANDOM_H_

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


bool urandom_has_device();

void urandom_get_bytes(void *buf, size_t len);

/* Return an integer in the range [a, b) if b > a. 
 * Otherwise, return an integer in the range [b, a).
 */
int urandom_get_int(int a, int b);


/* The id begins with a lower-case letter and contains only 
 * lower-case base32 characters.
 * The id will always be null-terminated.
 */
ssize_t urandom_generate_id(char id[], size_t size); 


#ifdef __cplusplus
}
#endif

#endif
