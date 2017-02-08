/* $Id: wdict.h,v 1.8 2011/01/28 08:27:19 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-08-09
 */
#ifndef WDICT_H_
#define WDICT_H_ 1

#include "xstr.h"
#include <stdio.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


#define WDICT_WORD_MAX_SIZE	64	/* including the trailing zero. */


typedef struct wdict_t wdict_t;


/*-
  Return length of the word, and should be less than WD_WORD_MAX_SIZE.
  If it's equal or greater than WD_WORD_MAX_SIZE, the word is truncated
  to length of (WD_WORD_MAX_SIZE - 1).
  If the returned number is 0, then the word (empty string) is ignored,
  and the str_generator_t() is called again. 
  Return a negative number to indicate finishing of words generation.
-*/
typedef int wdict_generator_t(char **p_word, void *source);


wdict_t *wdict_compile(wdict_generator_t *gen, void *source);


wdict_t *wdict_load_file(const char *filename);


wdict_t *wdict_load_multi_file(char *filenames[], int num);


void wdict_destroy(wdict_t *ss);


void wdict_dump(wdict_t *ss, FILE *out);


int wdict_num_word(wdict_t *ss);



#define WDICT_ICASE	0x00010000
#define WDICT_ISPACE	0x00020000
#define WDICT_NOTGBK	0x00040000

/*-
  The function searchs the string 'str' to find a word that is in the 'wd'.
  If found, a pointer to the beginning of the word in the 'str' is returned.
  Otherwise, NULL is returned. 

  The 'flag' parameter specifies the searching options by bit-wise OR'ing the
  following values:

	WDICT_NOTGBK	Do not check GBK character boundery

	WDICT_ICASE	Do not differentiate case. The words that are 
			different only in case are treated as the same. 

	WDICT_ISPACE	Ignore space characters that are between the non-space
			characters as if they do not exist.

  If 'p_end' is not NULL, the pointer point to the end of the found word in 
  the 'str' is stored in '*p_end'.

  If 'theword' is not NULL, the canonical form of the word in the dictionary
  is copied to it. The size of 'theword' should be no less than 
  WDICT_WORD_MAX_SIZE. 
-*/

char *wdict_search(wdict_t *wd, const char *str, int flag, char **p_end, char *theword/*NULL*/);

char *wdict_match(wdict_t *wd, const char *str, int flag, char **p_end, char *theword/*NULL*/);


bool wdict_search_xstr(wdict_t *wd, const xstr_t *str, int flag, xstr_t *found, char *theword/*NULL*/);

bool wdict_match_xstr(wdict_t *wd, const xstr_t *str, int flag, xstr_t *found, char *theword/*NULL*/);



#ifdef __cplusplus
}
#endif

#endif

