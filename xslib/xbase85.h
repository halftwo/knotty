/* $Id: xbase85.h 379 2016-10-12 05:09:07Z gremlin $ */
/*
   The used character set is defined in RFC1924.

   Author: XIONG Jiagui
   Date: 2009-02-02
 */
#ifndef XBASE85_H_
#define XBASE85_H_ 1

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


#define XBASE85_LEN(n)	(((n) * 5 + 3) / 4)

/*
 * Not used 9 non-space characters:
 *	" ' , . / : [ \ ]
 */

/*  0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~  */;
extern const char xbase85_alphabet[];


/* Return the number of characters placed in out. 
 */
ssize_t xbase85_encode(char *out, const void *in, size_t len);

 
/* Return the number of characters placed in out. 
 * On error, return a negative number, the absolute value of the number
 * equals to the consumed size of the input string.
 * If ~len~ is negative, length of ~in~ is taken as strlen(~in~);
 */
ssize_t xbase85_decode(void *out, const char *in, size_t len);


#ifdef __cplusplus
}
#endif


#endif

