/* $Id: opt.h,v 1.12 2014/08/14 02:51:00 gremlin Exp $ */
/*
   Ideas and implementations (with some modification) are from plan9.

   Author: XIONG Jiagui
   Date: 2005-06-20

   The following is an usage example:

++++++++++++++++++++++++++++ EXAMPLE CODE BEGIN ++++++++++++++++++++++++++++++

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [options]\n");
	fprintf(stderr, 
"  -a                 Option a\n"
"  -b barg            Argument for option b\n"
"  -c [carg]          Argument for option c can be omitted\n"
		);
	exit(1);
}

int main(int argc, char **argv)
{
	const char *prog = argv[0];
	bool a = false;
	const char *b = NULL;
	const char *c = NULL;
	int optend;

	OPT_BEGIN(argc, argv, &optend) {
	case 'a':
		a = true;			// No argument
		break;
	case 'b':
		b = OPT_EARG(usage(prog));	// argument can NOT be omitted
		break;
	case 'c':
		c = OPT_ARG();			// argument can be omitted 
		break;
	default:
		usage(prog);
	} OPT_END();

	...... 

	return 0;
}

++++++++++++++++++++++++++++ EXAMPLE CODE END ++++++++++++++++++++++++++++++++

*/
#ifndef OPT_H_
#define OPT_H_ 1

#include <stdlib.h>


/* The ARGC and ARGV should be the first and second parameter of the main()
 * function respectively. The ARGC and ARGV is unchanged.
 * After OPT_END(), the *ENDPOS is set to the number of consumed 
 * argv-elements (the index of the first argv-element that is not 
 * an option).
 */
#define	OPT_BEGIN(ARGC, ARGV, ENDPOS)		do {			\
	int _opt_m__ = (ARGC), _opt_c__ = _opt_m__ - 1;			\
	char **_opt_v__ = (ARGV) + 1;					\
	int *_opt_end__ = (ENDPOS);					\
	for ( ; _opt_c__ && _opt_v__[0][0] == '-' && _opt_v__[0][1];	\
		--_opt_c__, ++_opt_v__)					\
	{								\
		char _opt_o__;						\
		const char *_opt_t__, *_opt_s__ = &_opt_v__[0][1];	\
		if (_opt_s__[0] == '-' && _opt_s__[1] == 0) {		\
			--_opt_c__; ++_opt_v__;				\
			break;						\
		}							\
		while((_opt_o__ = *_opt_s__++) != 0) {			\
			_opt_t__ = _opt_s__;				\
			switch (_opt_o__)


#define	OPT_END()							\
		} /*while*/						\
		(void)_opt_t__; /* make compiler happy */		\
	} /*for*/ 							\
	if (_opt_end__) *_opt_end__ = _opt_m__ - _opt_c__;		\
} while(0)


/* If the arugment for the option is omitted, (const char *)0 returned.
 */
#define	OPT_ARG()							\
	(_opt_s__ = "", (*_opt_t__ 					\
		? _opt_t__						\
		: (_opt_v__[1] 						\
			? (--_opt_c__, *++_opt_v__)			\
			: (const char *)0)				\
		)							\
	)


/* If the arugment for the option is omitted, the XFUNCALL is called. 
   The XFUNCALL should be a function call that never return. That is,
   the function should exit(), longjmp() or throw, etc, but not return.
 */
#define	OPT_EARG(XFUNCALL)						\
	(_opt_s__ = "", (*_opt_t__					\
		? _opt_t__						\
		: (_opt_v__[1]						\
			? (--_opt_c__, *++_opt_v__)			\
			: ((XFUNCALL), abort(), (const char*)0))	\
		)							\
	)


/* The option character itself.
 */
#define	OPT_OPT()	_opt_o__



#endif

