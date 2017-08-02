/* $Id: xstr.h,v 1.49 2015/07/14 08:42:38 gremlin Exp $ */
#ifndef XSTR_H_
#define XSTR_H_

#include "xsdef.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>


#ifdef SSIZE_MAX
#define XSTR_MAXLEN	SSIZE_MAX	/* The xstr_t.len SHOULD NOT exceed this value */
#else
#define XSTR_MAXLEN	(SIZE_MAX/2)	/* The xstr_t.len SHOULD NOT exceed this value */
#endif


typedef struct xstr_t xstr_t;
typedef struct bset_t bset_t;


struct xstr_t
{
	uint8_t *data;			/* NOT '\0' terminated. */
	ssize_t len;
};

extern const xstr_t xstr_null;


struct bset_t
{
	uint32_t data[8];
};

extern const bset_t full_bset;		/* all 8-bit char in it */
extern const bset_t empty_bset;		/* no char in it */
extern const bset_t x80_bset;		/* [\x80-\xff] */
extern const bset_t alnum_bset;		/* [0-9A-Za-z] */
extern const bset_t alpha_bset;		/* [A-Za-z] */
extern const bset_t ascii_bset;		/* [\0-\x7f] */
extern const bset_t blank_bset;		/* [ \t] */
extern const bset_t cntrl_bset;		/* [\0-\x1f\x7f] */
extern const bset_t digit_bset;		/* [0-9] */
extern const bset_t graph_bset;		/* [\x21-\x7e]  all print excet [ \x7f] */
extern const bset_t lower_bset;		/* [a-z] */
extern const bset_t print_bset;		/* [\x20-\x7e]  all ascii exept cntrl */
extern const bset_t punct_bset;		/* all graph except alnum */
extern const bset_t space_bset;		/* [ \t\n\v\f\r] */
extern const bset_t upper_bset;		/* [A-Z] */
extern const bset_t xdigit_bset;	/* [0-9A-Fa-f] */



#define XSTR_INIT(STR, LEN)	{(STR), ((ssize_t)(LEN)>0) ? (ssize_t)(LEN) : 0}
#define XSTR_CONST(CONSTSTR)	{(uint8_t *)"" CONSTSTR, (ssize_t)(sizeof(CONSTSTR)-1)}
#define XSTR_C(STR)		{(uint8_t *)(STR), (ssize_t)strlen(STR)}
#define XSTR_CXX(STRING)	{(uint8_t *)const_cast<char *>((STRING).data()), (ssize_t)(STRING).length()}


#define xstr_const(XS, CONSTSTR)	xstr_init((XS), (uint8_t *)"" CONSTSTR, (ssize_t)sizeof(CONSTSTR)-1)


/* Used with printf("%.*s", XSTR_P(xs)) */
#define XSTR_P(XS)		((int)(XS)->len), ((XS)->data)



#define BSET_SET(BSET, CH)	((BSET)->data[(uint8_t)(CH) >> 5] |= (1 << ((CH) & 0x1f)))
#define BSET_CLEAR(BSET, CH)	((BSET)->data[(uint8_t)(CH) >> 5] &= ~(1 << ((CH) & 0x1f)))
#define BSET_FLIP(BSET, CH)	((BSET)->data[(uint8_t)(CH) >> 5] ^= (1 << ((CH) & 0x1f)))
#define BSET_TEST(BSET, CH)	((BSET)->data[(uint8_t)(CH) >> 5] & (1 << ((CH) & 0x1f)))



#ifdef __cplusplus
extern "C" {
#endif

static inline void xstr_init(xstr_t *xs, const void *data, size_t len)
{
	xs->data = (uint8_t *)data;
	xs->len = ((ssize_t)len > 0) ? len : 0;
}

static inline void xstr_c(xstr_t *xs, const char *str)
{
	xs->data = (uint8_t *)str;
	xs->len = (ssize_t)strlen(str);
}

static inline uint8_t *xstr_end(const xstr_t *xs)
{
	return xs->data + xs->len;
}

static inline xstr_t make_xstr(const char *buf, size_t size)
{
	xstr_t xs = XSTR_INIT((uint8_t *)buf, (ssize_t)size);
	return xs;
}

static inline bool xstr_char_equal(const xstr_t *xs, ssize_t pos, char ch)
{
	return (pos >= 0) ? (pos < xs->len && xs->data[pos] == (uint8_t)ch)
		: (xs->len + pos >= 0 && xs->data[xs->len + pos] == (uint8_t)ch);
}

static inline bool xstr_char_in_bset(const xstr_t *xs, ssize_t pos, const bset_t *bset)
{
	return (pos >= 0) ? (pos < xs->len && BSET_TEST(bset, xs->data[pos]))
		: (xs->len + pos >= 0 && BSET_TEST(bset, xs->data[xs->len + pos]));
}

bset_t make_bset_from_xstr(const xstr_t *more);
bset_t make_bset_from_cstr(const char *more);
bset_t make_bset_from_mem(const void *more, size_t n);

bset_t make_bset_by_add_xstr(const bset_t *prototype, const xstr_t *more);
bset_t make_bset_by_add_cstr(const bset_t *prototype, const char *more);
bset_t make_bset_by_add_mem(const bset_t *prototype, const void *more, size_t n);
bset_t make_bset_by_add_bset(const bset_t *prototype, const bset_t *more);

bset_t make_bset_by_del_xstr(const bset_t *prototype, const xstr_t *unwanted);
bset_t make_bset_by_del_cstr(const bset_t *prototype, const char *unwanted);
bset_t make_bset_by_del_mem(const bset_t *prototype, const void *unwanted, size_t n);
bset_t make_bset_by_del_bset(const bset_t *prototype, const bset_t *unwanted);

bset_t *bset_add_xstr(bset_t *bset, const xstr_t *more);
bset_t *bset_add_cstr(bset_t *bset, const char *more);
bset_t *bset_add_mem(bset_t *bset, const void *more, size_t n);
bset_t *bset_add_bset(bset_t *bset, const bset_t *more);

bset_t *bset_del_xstr(bset_t *bset, const xstr_t *unwanted);
bset_t *bset_del_cstr(bset_t *bset, const char *unwanted);
bset_t *bset_del_mem(bset_t *bset, const void *unwanted, size_t n);
bset_t *bset_del_bset(bset_t *bset, const bset_t *unwanted);


/* The returned xstr_t.data is malloc()ed, the caller should free() it. */
xstr_t xstr_dup(const xstr_t *str);
xstr_t xstr_dup_cstr(const char *str);
xstr_t xstr_dup_mem(const void *s, size_t n);

/* Similar to strdup(), except the argument is xstr_t * instead of char *. */
char *strdup_xstr(const xstr_t *str);
char *strdup_mem(const void *s, size_t n);


/* If n > 0, out will always be terminated by '\0'. */
size_t xstr_copy_cstr(const xstr_t *str, void *out, size_t n);

/* out will NOT be terminated by '\0'. */
size_t xstr_copy_mem(const xstr_t *str, void *out, size_t n);


int xstr_compare(const xstr_t *s1, const xstr_t *s2);
int xstr_compare_cstr(const xstr_t *s1, const char *s2);
int xstr_compare_mem(const xstr_t *s1, const void *s2, size_t n);

bool xstr_equal(const xstr_t *s1, const xstr_t *s2);
bool xstr_equal_cstr(const xstr_t *s1, const char *s2);
bool xstr_equal_mem(const xstr_t *s1, const void *s2, size_t n);

bool xstr_start_with(const xstr_t *str, const xstr_t *prefix);
bool xstr_start_with_cstr(const xstr_t *str, const char *prefix);
bool xstr_start_with_mem(const xstr_t *str, const void *prefix, size_t n);

bool xstr_end_with(const xstr_t *str, const xstr_t *suffix);
bool xstr_end_with_cstr(const xstr_t *str, const char *suffix);
bool xstr_end_with_mem(const xstr_t *str, const void *suffix, size_t n);


int xstr_alphabet_compare(const xstr_t *s1, const xstr_t *s2);
int xstr_alphabet_compare_cstr(const xstr_t *s1, const char *s2);
int xstr_alphabet_compare_mem(const xstr_t *s1, const void *s2, size_t n);


int xstr_case_compare(const xstr_t *s1, const xstr_t *s2);
int xstr_case_compare_cstr(const xstr_t *s1, const char *s2);
int xstr_case_compare_mem(const xstr_t *s1, const void *s2, size_t n);

bool xstr_case_equal(const xstr_t *s1, const xstr_t *s2);
bool xstr_case_equal_cstr(const xstr_t *s1, const char *s2);
bool xstr_case_equal_mem(const xstr_t *s1, const void *s2, size_t n);

bool xstr_case_start_with(const xstr_t *str, const xstr_t *prefix);
bool xstr_case_start_with_cstr(const xstr_t *str, const char *prefix);
bool xstr_case_start_with_mem(const xstr_t *str, const void *prefix, size_t n);

bool xstr_case_end_with(const xstr_t *str, const xstr_t *suffix);
bool xstr_case_end_with_cstr(const xstr_t *str, const char *suffix);
bool xstr_case_end_with_mem(const xstr_t *str, const void *suffix, size_t n);


ssize_t xstr_case_find(const xstr_t *xs, ssize_t pos, const xstr_t *needle);
ssize_t xstr_case_find_cstr(const xstr_t *xs, ssize_t pos, const char *needle);
ssize_t xstr_case_find_mem(const xstr_t *xs, ssize_t pos, const void *needle, size_t n);

ssize_t xstr_case_rfind(const xstr_t *str, ssize_t pos, const xstr_t *needle);
ssize_t xstr_case_rfind_cstr(const xstr_t *str, ssize_t pos, const char *needle);
ssize_t xstr_case_rfind_mem(const xstr_t *str, ssize_t pos, const void *needle, size_t n);


ssize_t xstr_find(const xstr_t *str, ssize_t pos, const xstr_t *needle);
ssize_t xstr_find_cstr(const xstr_t *str, ssize_t pos, const char *needle);
ssize_t xstr_find_mem(const xstr_t *str, ssize_t pos, const void *needle, size_t n);
ssize_t xstr_find_char(const xstr_t *str, ssize_t pos, char ch);
ssize_t xstr_find_not_char(const xstr_t *str, ssize_t pos, char ch);

ssize_t xstr_rfind(const xstr_t *str, ssize_t pos, const xstr_t *needle);
ssize_t xstr_rfind_cstr(const xstr_t *str, ssize_t pos, const char *needle);
ssize_t xstr_rfind_mem(const xstr_t *str, ssize_t pos, const void *needle, size_t n);
ssize_t xstr_rfind_char(const xstr_t *str, ssize_t pos, char ch);
ssize_t xstr_rfind_not_char(const xstr_t *str, ssize_t pos, char ch);


ssize_t xstr_find_in(const xstr_t *str, ssize_t pos, const xstr_t *chset);
ssize_t xstr_find_in_cstr(const xstr_t *str, ssize_t pos, const char *chset);
ssize_t xstr_find_in_mem(const xstr_t *str, ssize_t pos, const void *chset, size_t n);
ssize_t xstr_find_in_bset(const xstr_t *str, ssize_t pos, const bset_t *bset);

ssize_t xstr_find_not_in(const xstr_t *str, ssize_t pos, const xstr_t *chset);
ssize_t xstr_find_not_in_cstr(const xstr_t *str, ssize_t pos, const char *chset);
ssize_t xstr_find_not_in_mem(const xstr_t *str, ssize_t pos, const void *chset, size_t n);
ssize_t xstr_find_not_in_bset(const xstr_t *str, ssize_t pos, const bset_t *bset);

ssize_t xstr_rfind_in(const xstr_t *str, ssize_t pos, const xstr_t *chset);
ssize_t xstr_rfind_in_cstr(const xstr_t *str, ssize_t pos, const char *chset);
ssize_t xstr_rfind_in_mem(const xstr_t *str, ssize_t pos, const void *chset, size_t n);
ssize_t xstr_rfind_in_bset(const xstr_t *str, ssize_t pos, const bset_t *bset);

ssize_t xstr_rfind_not_in(const xstr_t *str, ssize_t pos, const xstr_t *chset);
ssize_t xstr_rfind_not_in_cstr(const xstr_t *str, ssize_t pos, const char *chset);
ssize_t xstr_rfind_not_in_mem(const xstr_t *str, ssize_t pos, const void *chset, size_t n);
ssize_t xstr_rfind_not_in_bset(const xstr_t *str, ssize_t pos, const bset_t *bset);


/* Return the position of the first delimiter found in `str`.
 * If no delimiter found, return -1 and key == xstr_null, value == xstr_trim(str).
 */
ssize_t xstr_key_value(const xstr_t *str, char delimiter, xstr_t *key/*NULL*/, xstr_t *value/*NULL*/);

xstr_t xstr_prefix(const xstr_t *str, ssize_t end);			/* [0, end) */
xstr_t xstr_suffix(const xstr_t *str, ssize_t start);			/* [start, XSTR_MAXLEN) */
xstr_t xstr_slice(const xstr_t *str, ssize_t start, ssize_t end);	/* [start, end) */
xstr_t xstr_substr(const xstr_t *str, ssize_t pos, size_t length);


size_t xstr_count(const xstr_t *str, const xstr_t *needle);
size_t xstr_count_cstr(const xstr_t *str, const char *needle);
size_t xstr_count_mem(const xstr_t *str, const void *needle, size_t n);
size_t xstr_count_char(const xstr_t *str, char needle);

size_t xstr_count_in(const xstr_t *str, const xstr_t *chset);
size_t xstr_count_in_cstr(const xstr_t *str, const char *chset);
size_t xstr_count_in_mem(const xstr_t *str, const void *chset, size_t n);
size_t xstr_count_in_bset(const xstr_t *str, const bset_t *bset);


size_t xstr_replace_char(xstr_t *str, char needle, char replace);

size_t xstr_replace_in(xstr_t *str, const xstr_t *chset, const xstr_t *replace);
size_t xstr_replace_in_cstr(xstr_t *str, const char *chset, const char *replace);


void make_translate_table_from_xstr(uint8_t table[256], const xstr_t *chset, const xstr_t *replace);
void make_translate_table_from_cstr(uint8_t table[256], const char *chset, const char *replace);

size_t xstr_translate(xstr_t *str, const uint8_t table[256]);


bool xstr_delimit(xstr_t *str, const xstr_t *delimiter, xstr_t *result/*NULL*/);
bool xstr_delimit_cstr(xstr_t *str, const char *delimiter, xstr_t *result/*NULL*/);
bool xstr_delimit_mem(xstr_t *str, const void *delimiter, size_t n, xstr_t *result/*NULL*/);
bool xstr_delimit_char(xstr_t *str, char delimiter, xstr_t *result/*NULL*/);

/* like strsep() */
bool xstr_delimit_in(xstr_t *str, const xstr_t *chset, xstr_t *result/*NULL*/);
bool xstr_delimit_in_cstr(xstr_t *str, const char *chset, xstr_t *result/*NULL*/);
bool xstr_delimit_in_mem(xstr_t *str, const void *chset, size_t n, xstr_t *result/*NULL*/);
bool xstr_delimit_in_bset(xstr_t *str, const bset_t *bset, xstr_t *result/*NULL*/);
bool xstr_delimit_in_space(xstr_t *str, xstr_t *result/*NULL*/);


/* like strtok_r() */
bool xstr_token(xstr_t *str, const xstr_t *chset, xstr_t *result/*NULL*/);
bool xstr_token_cstr(xstr_t *str, const char *chset, xstr_t *result/*NULL*/);
bool xstr_token_mem(xstr_t *str, const void *chset, size_t n, xstr_t *result/*NULL*/);
bool xstr_token_bset(xstr_t *str, const bset_t *bset, xstr_t *result/*NULL*/);
bool xstr_token_char(xstr_t *str, char delimiter, xstr_t *result/*NULL*/);
bool xstr_token_space(xstr_t *str, xstr_t *result/*NULL*/);


xstr_t *xstr_advance(xstr_t *str, size_t n); 

xstr_t *xstr_lstrip(xstr_t *str, const xstr_t *rubbish);
xstr_t *xstr_lstrip_cstr(xstr_t *str, const char *rubbish);
xstr_t *xstr_lstrip_mem(xstr_t *str, const void *rubbish, size_t n);
xstr_t *xstr_lstrip_bset(xstr_t *str, const bset_t *rubbish);
xstr_t *xstr_lstrip_char(xstr_t *str, char rubbish);
xstr_t *xstr_ltrim(xstr_t *str);

xstr_t *xstr_rstrip(xstr_t *str, const xstr_t *rubbish);
xstr_t *xstr_rstrip_cstr(xstr_t *str, const char *rubbish);
xstr_t *xstr_rstrip_mem(xstr_t *str, const void *rubbish, size_t n);
xstr_t *xstr_rstrip_bset(xstr_t *str, const bset_t *rubbish);
xstr_t *xstr_rstrip_char(xstr_t *str, char rubbish);
xstr_t *xstr_rtrim(xstr_t *str);

xstr_t *xstr_strip(xstr_t *str, const xstr_t *rubbish);
xstr_t *xstr_strip_cstr(xstr_t *str, const char *rubbish);
xstr_t *xstr_strip_mem(xstr_t *str, const void *rubbish, size_t n);
xstr_t *xstr_strip_bset(xstr_t *str, const bset_t *rubbish);
xstr_t *xstr_strip_char(xstr_t *str, char rubbish);
xstr_t *xstr_trim(xstr_t *str);

xstr_t *xstr_lower(xstr_t *str);
xstr_t *xstr_upper(xstr_t *str);


/* Same as xstr_to_long(str, NULL, 10); */
int xstr_atoi(const xstr_t *str);
long xstr_atol(const xstr_t *str);
long long xstr_atoll(const xstr_t *str);
unsigned long long xstr_atoull(const xstr_t *str);

long xstr_to_long(const xstr_t *str, xstr_t *end/*NULL*/, int base/*0*/);
long long xstr_to_llong(const xstr_t *str, xstr_t *end/*NULL*/, int base/*0*/);

unsigned long xstr_to_ulong(const xstr_t *str, xstr_t *end/*NULL*/, int base/*0*/);
unsigned long long xstr_to_ullong(const xstr_t *str, xstr_t *end/*NULL*/, int base/*0*/);

intmax_t xstr_to_integer(const xstr_t *str, xstr_t *end/*NULL*/, int base/*0*/);
uintmax_t xstr_to_uinteger(const xstr_t *str, xstr_t *end/*NULL*/, int base/*0*/);


double xstr_to_double(const xstr_t *str, xstr_t *end/*NULL*/);




#ifdef __cplusplus
}
#endif



#ifdef __cplusplus

#include <string>
#include <iostream>

inline void xstr_cxx(xstr_t *xs, const std::string& str)
{
	xs->data = (uint8_t *)const_cast<char *>(str.data());
	xs->len = (ssize_t)str.length();
}

inline xstr_t make_xstr(const std::string& s)
{
	xstr_t xs = XSTR_CXX(s);
	return xs;
}

inline std::string make_string(const xstr_t& xs)
{
	return std::string((const char *)xs.data, xs.len);
}

std::string make_string_lower(const xstr_t& xs);
std::string make_string_upper(const xstr_t& xs);


std::string operator+(const xstr_t& xs, const xstr_t& str);
std::string operator+(const xstr_t& xs, const std::string& str);
std::string operator+(const xstr_t& xs, const char *str);
std::string operator+(const std::string& str, const xstr_t& xs);
std::string operator+(const char *str, const xstr_t& xs);


inline bool operator< (const xstr_t& xs1, const xstr_t& xs2)
{
	return xstr_compare(&xs1, &xs2) < 0;
}

inline bool operator<=(const xstr_t& xs1, const xstr_t& xs2)
{
	return xstr_compare(&xs1, &xs2) <= 0;
}

inline bool operator> (const xstr_t& xs1, const xstr_t& xs2)
{
	return xstr_compare(&xs1, &xs2) > 0;
}

inline bool operator>=(const xstr_t& xs1, const xstr_t& xs2)
{
	return xstr_compare(&xs1, &xs2) >= 0;
}

inline bool operator==(const xstr_t& xs1, const xstr_t& xs2)
{
	return xstr_equal(&xs1, &xs2);
}

inline bool operator!=(const xstr_t& xs1, const xstr_t& xs2)
{
	return !xstr_equal(&xs1, &xs2);
}


inline bool operator==(const xstr_t& xs1, const std::string& s2)
{
	xstr_t xs2 = XSTR_CXX(s2);
	return xstr_equal(&xs1, &xs2);
}

inline bool operator!=(const xstr_t& xs1, const std::string& s2)
{
	xstr_t xs2 = XSTR_CXX(s2);
	return !xstr_equal(&xs1, &xs2);
}

inline bool operator==(const std::string& s1, const xstr_t& xs2)
{
	xstr_t xs1 = XSTR_CXX(s1);
	return xstr_equal(&xs1, &xs2);
}

inline bool operator!=(const std::string& s1, const xstr_t& xs2)
{
	xstr_t xs1 = XSTR_CXX(s1);
	return !xstr_equal(&xs1, &xs2);
}


inline std::ostream& operator<<(std::ostream& os, const xstr_t& xs)
{
	return os.write((const char *)xs.data, xs.len);
}

#endif /* __cplusplus */



#endif
