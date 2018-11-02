/* $Id: xbase64.h 379 2016-10-12 05:09:07Z gremlin $ */
/*
   Author: XIONG Jiagui
   Date: 2006-04-28
 */
#ifndef XBASE64_H_
#define XBASE64_H_ 1

#include "xsdef.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XBASE64_ENCODED_LEN(n)	(((n) * 4 + 2) / 3)	/* without padding */
#define XBASE64_DECODED_LEN(n)	(((n) * 3) / 4)		/* without padding */
#define XBASE64_TOTAL_LEN(n) 	(((n) + 2) / 3 * 4)	/* with padding */

typedef struct xbase64_t xbase64_t;

struct xbase64_t
{
	unsigned char alphabet[66];
	unsigned char detab[256];
};

/*  ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=  */
extern const xbase64_t std_xbase64;

/*  ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=  */
extern const xbase64_t url_xbase64;


enum
{
	XBASE64_NO_PADDING = 0x01,
	XBASE64_AUTO_NEWLINE = 0x02,
	XBASE64_IGNORE_SPACE = 0x10,
	XBASE64_IGNORE_NON_ALPHABET = 0x20,
};


/* The alphabet[64] used as padding character if not '\0'.
 * Return 0 if success, otherwise -1.
 */
int xbase64_init(xbase64_t *b64, const char *alphabet);


/* Return the number of characters placed in out. 
 */
ssize_t xbase64_encode(const xbase64_t *b64, char *out, const void *in, size_t len, int flag);

 
/* Return the number of characters placed in out. 
 * On error, return a negative number, the absolute value of the number
 * equals to the consumed size of the input string.
 */
ssize_t xbase64_decode(const xbase64_t *b64, void *out, const char *in, size_t len, int flag);


#ifdef __cplusplus
}
#endif


#endif

