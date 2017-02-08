/* $Id: loc.h,v 1.16 2015/04/08 09:57:35 gremlin Exp $ */
/* 
 * Local Continuation.
 * Implemented on top of standard ANSI C. See
 * http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html 
 * for a full discussion of the theory behind this.
 *
 */
#ifndef LOC_H
#define LOC_H 1

#include <assert.h>


/*
 * Ground rules:
 *  - never put LOC_YIELD or LOC_ANCHOR within an explicit 'switch'.
 *  - never put two LOC_YIELD statements on the same source line.
 */


typedef struct
{
	int line;
} loc_t;


#define LOC_INITIALIZER		{ 0 }


#define LOC_RESET(loc)		((void)((loc)->line = 0))


#define LOC_HALT(loc)		((void)((loc)->line = -1))



#define LOC_BEGIN(loc)		do {		\
	loc_t *_lOc__X__ = (loc);		\
	switch (_lOc__X__->line) { case 0:


#define LOC_ANCHOR		_lOc__X__->line = __LINE__; case __LINE__: 					


#define LOC_PAUSE(Z)		return Z


#define LOC_YIELD(Z)		do { _lOc__X__->line = __LINE__; return Z; case __LINE__: ; } while(0)


#define LOC_CANCEL(Z)		do { _lOc__X__->line = -1; return Z; } while(0)


#define LOC_END(loc)		(void)0; }	\
	assert((loc) == _lOc__X__);		\
	_lOc__X__->line = -1;			\
} while (0)



#endif /* LOC_H */

