/* $Id: cstr.h,v 1.19 2012/04/28 01:46:57 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2007-03-08
 */
#ifndef cstr_h_
#define cstr_h_

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif



/* Must call free() on the return value.
 */
void *cmem_dup(const void *src, size_t n);


/* Must call free() on the return value.
 */
char *cstr_ndup(const char *src, size_t n);


size_t cstr_from_long(char buf[], long n);
size_t cstr_from_llong(char buf[], long long n);
size_t cstr_from_ulong(char buf[], unsigned long n);
size_t cstr_from_ullong(char buf[], unsigned long long n);


/* If the second argument ~rubbish~ is NULL or "", 
   cstr_lstrip(), cstr_rstrip(), and cstr_strip() willl strip off 
   the space character in the first argument.
 */

/* Return the begin of the left stripped string. */
char *cstr_lstrip(const char *str, const char *rubbish);


/* Return the end of the right stripped string. */
char *cstr_rstrip(char *str, const char *rubbish);


/* Return the begin of the stripped string. */
char *cstr_strip(char *str, const char *rubbish);


/* The same as cstr_strip(str, NULL). */
char *cstr_trim(char *str);


/* Return the end of the string.
 */
char *cstr_lower(char *str);
char *cstr_upper(char *str);


/* If dst is NULL, the converstion is done in place. */
size_t cstr_tolower(char *dst/*NULL*/, const char *str);
size_t cstr_toupper(char *dst/*NULL*/, const char *str);


bool cstr_isalpha(const char *str);
bool cstr_isalnum(const char *str);
bool cstr_isupper(const char *str);
bool cstr_islower(const char *str);
bool cstr_isspace(const char *str);
bool cstr_isdigit(const char *str);
bool cstr_isxdigit(const char *str);


bool cstr_start_with(const char *str, const char *prefix);
bool cstr_end_with(const char *str, const char *suffix);


size_t cstr_count(const char *str, char needle);


/* Return the length of str.
 */
size_t cstr_translate(char *str, const unsigned char table[256]);


/* Replace the ~match~ character in ~src~ as ~target~. The number of occurrence of 
   ~match~ is returned, and the result string is placed in ~dst~. 
   If no ~match~ found, the content of ~dst~ is unchanged.
   If the ~dst~ is NULL, the replacement is done in place.
*/
size_t cstr_replace(char *dst, const char *src, char match, char target);

/* Same as cstr_replace() except that the ~src~ is ~size~ long and may not ended
   with NUL character.  The ~dst~ is also not ended with NUL character.
 */
size_t cstr_nreplace(char *dst, const char *src, size_t size, char match, char target);


/* Escape meta characters with backslash \ .
   Backslash itself should be put into meta too.
   Return the size of the result escaped string. 
   ~dst~ is always ended with NUL character.
 */
size_t cstr_backslash_escape(char *dst, const char *src, const char *meta, const char *subst);


/* This function is quite like strsep(), except the second argument is 
   a char. All other behavior is the same as strsep().
*/
char *cstr_delimit(char **strptr, char delimiter);

char *cstr_rchar(const char *str, const char *end, char ch);

char *cstr_rstr(const char *haystack, const char *end, const char *needle);


/* The following case-function only for ASCII. These function can avoid the
   locale overhead in the libc library.
 */
int cstr_casecmp(const char *s1, const char *s2);

int cstr_ncasecmp(const char *s1, const char *s2, size_t size);

char *cstr_casestr(const char *haystack, const char *needle);

char *cstr_casestrn(const char *haystack, const char *needle, size_t n);


char *cstr_strn(const char *haystack, const char *needle, size_t n);


/* The cstr_nlen() function returns the length of the string, but at most 
   @maxlen if the actual length is greater than @maxlen.
 */
size_t cstr_nlen(const char *str, size_t maxlen);


/* Like strchr(), except if the character is not found, return a pointer to 
   the last nul-character instead of NULL.
 */
char *cstr_chrnul(const char *str, int ch);


size_t cstr_move(char *dst, const char *src);


/* Return required size for the buffer, it may be larger than 
   the actual buffer size.
   The buf is always nul-terminated.
 */
size_t cstr_ncopy(char *buf, size_t size, const char *src);
size_t cstr_ncopyn(char *buf, size_t size, const char *src, size_t max);
size_t cstr_nputc(char *buf, size_t size, char ch);


/* Return a pointer to buf + strlen(src), it may be greater than 
   the end parameter.
   The buf is always nul-terminated.
 */
char *cstr_pcopy(char *buf, const char *end/*NULL*/, const char *src);
char *cstr_pcopyn(char *buf, const char *end/*NULL*/, const char *src, size_t max);
char *cstr_pputc(char *buf, const char *end/*NULL*/, char ch);


/* Return a pointer to the end of the string buf 
   (the address of the terminating null byte) 
 */
char *cstr_copy(char *buf, const char *src);
char *cstr_copyn(char *buf, const char *src, size_t max);


/* Just like memcpy(), but return a pointer to the end of the dest
   instead dest itself.
 */
void *cmem_copy(void *dest, const void *src, size_t n);



#ifdef __cplusplus
}
#endif

#endif
