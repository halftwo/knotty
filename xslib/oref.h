/* $Id: oref.h,v 1.9 2010/10/22 03:07:38 jiagui Exp $ */
#ifndef OREF_H
#define OREF_H 1

#include "xatomic.h"
#include <assert.h>


#define OREF_DECLARE()		xatomic_t _oReF_CnT_


#define OREF_INIT(obj)		xatomic_set(&(obj)->_oReF_CnT_, 1)


#define OREF_COUNT(obj)		xatomic_get(&(obj)->_oReF_CnT_)


#define OREF_INC(obj) 		xatomic_inc(&(obj)->_oReF_CnT_)


#define OREF_DEC_AND_TEST(obj)	xatomic_dec_and_test(&(obj)->_oReF_CnT_)


/* If no destroy operation needed when the reference count drop to 0,
 * the second argument @destroy may be given to 'void'.
 */
#define OREF_DEC(obj, destroy)						\
	(	assert(OREF_COUNT(obj) > 0),				\
		(OREF_DEC_AND_TEST(obj) ? (void)((destroy)((obj)))	\
			: (void)0)					\
	)								\


#endif

