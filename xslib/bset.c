#include "xstr.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: bset.c,v 1.5 2015/06/15 02:50:32 gremlin Exp $";
#endif


const bset_t empty_bset;

const bset_t x80_bset = 
{
	{
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
	}
};

const bset_t full_bset = 
{
	{
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
	}
};

const bset_t alnum_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x03ff0000, /* 0000 0011 1111 1111  0000 0000 0000 0000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x07fffffe, /* 0000 0111 1111 1111  1111 1111 1111 1110 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x07fffffe, /* 0000 0111 1111 1111  1111 1111 1111 1110 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t alpha_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x07fffffe, /* 0000 0111 1111 1111  1111 1111 1111 1110 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x07fffffe, /* 0000 0111 1111 1111  1111 1111 1111 1110 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t ascii_bset = 
{
	{
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t blank_bset = 
{
	{
	0x00000200, /* 0000 0000 0000 0000  0000 0010 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x00000001, /* 0000 0000 0000 0000  0000 0000 0000 0001 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t cntrl_bset = 
{
	{
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t digit_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x03ff0000, /* 0000 0011 1111 1111  0000 0000 0000 0000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t graph_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0xfffffffe, /* 1111 1111 1111 1111  1111 1111 1111 1110 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x7fffffff, /* 0111 1111 1111 1111  1111 1111 1111 1111 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t lower_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x07fffffe, /* 0000 0111 1111 1111  1111 1111 1111 1110 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t print_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x7fffffff, /* 0111 1111 1111 1111  1111 1111 1111 1111 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t punct_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0xfc00fffe, /* 1111 1100 0000 0000  1111 1111 1111 1110 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0xf8000001, /* 1111 1000 0000 0000  0000 0000 0000 0001 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x78000001, /* 0111 1000 0000 0000  0000 0000 0000 0001 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t space_bset = 
{
	{
	0x00003e00, /* 0000 0000 0000 0000  0011 1110 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x00000001, /* 0000 0000 0000 0000  0000 0000 0000 0001 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t upper_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x07fffffe, /* 0000 0111 1111 1111  1111 1111 1111 1110 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

const bset_t xdigit_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x03ff0000, /* 0000 0011 1111 1111  0000 0000 0000 0000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x0000007e, /* 0000 0000 0000 0000  0000 0000 0111 1110 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x0000007e, /* 0000 0000 0000 0000  0000 0000 0111 1110 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};


inline bset_t *bset_add_mem(bset_t *bset, const void *more, size_t n)
{
	ssize_t i = (ssize_t)n;
	while (i-- > 0)
	{
		int ch = ((unsigned char*)more)[i];
		BSET_SET(bset, ch);
	}
	return bset;
}

bset_t *bset_add_xstr(bset_t *bset, const xstr_t *more)
{
	return bset_add_mem(bset, more->data, more->len);
}

bset_t *bset_add_cstr(bset_t *bset, const char *more)
{
	int ch;
	while ((ch = (unsigned char)*more++) != 0)
	{
		BSET_SET(bset, ch);
	}
	return bset;
}

bset_t *bset_add_bset(bset_t *bset, const bset_t *more)
{
	size_t i;
	for (i = 0; i < XS_ARRCOUNT(bset->data); ++i)
	{
		bset->data[i] |= more->data[i];
	}
	return bset;
}


inline bset_t *bset_del_mem(bset_t *bset, const void *unwanted, size_t n)
{
	ssize_t i = (ssize_t)n;
	while (i-- > 0)
	{
		int ch = ((unsigned char*)unwanted)[i];
		BSET_CLEAR(bset, ch);
	}
	return bset;
}

bset_t *bset_del_xstr(bset_t *bset, const xstr_t *unwanted)
{
	return bset_del_mem(bset, unwanted->data, unwanted->len);
}

bset_t *bset_del_cstr(bset_t *bset, const char *unwanted)
{
	int ch;
	while ((ch = (unsigned char)*unwanted++) != 0)
	{
		BSET_CLEAR(bset, ch);
	}
	return bset;
}

bset_t *bset_del_bset(bset_t *bset, const bset_t *unwanted)
{
	size_t i;
	for (i = 0; i < XS_ARRCOUNT(bset->data); ++i)
	{
		bset->data[i] &= ~unwanted->data[i];
	}
	return bset;
}


inline bset_t make_bset_by_add_mem(const bset_t *prototype, const void *more, size_t n)
{
	bset_t bset = *prototype;
	bset_add_mem(&bset, more, n);
	return bset;
}

bset_t make_bset_by_add_xstr(const bset_t *prototype, const xstr_t *more)
{
	return make_bset_by_add_mem(prototype, more->data, more->len);
}

bset_t make_bset_by_add_cstr(const bset_t *prototype, const char *more)
{
	bset_t bset = *prototype;
	bset_add_cstr(&bset, more);
	return bset;
}

bset_t make_bset_by_add_bset(const bset_t *prototype, const bset_t *more)
{
	bset_t bset = *prototype;
	bset_add_bset(&bset, more);
	return bset;
}


inline bset_t make_bset_by_del_mem(const bset_t *prototype, const void *unwanted, size_t n)
{
	bset_t bset = *prototype;
	bset_del_mem(&bset, unwanted, n);
	return bset;
}

bset_t make_bset_by_del_xstr(const bset_t *prototype, const xstr_t *unwanted)
{
	return make_bset_by_del_mem(prototype, unwanted->data, unwanted->len);
}

bset_t make_bset_by_del_cstr(const bset_t *prototype, const char *unwanted)
{
	bset_t bset = *prototype;
	bset_del_cstr(&bset, unwanted);
	return bset;
}

bset_t make_bset_by_del_bset(const bset_t *prototype, const bset_t *unwanted)
{
	bset_t bset = *prototype;
	bset_del_bset(&bset, unwanted);
	return bset;
}


bset_t make_bset_from_xstr(const xstr_t *more)
{
	return make_bset_by_add_mem(&empty_bset, more->data, more->len);
}

bset_t make_bset_from_cstr(const char *more)
{
	bset_t bset = empty_bset;
	bset_add_cstr(&bset, more);
	return bset;
}

bset_t make_bset_from_mem(const void *more, size_t n)
{
	return make_bset_by_add_mem(&empty_bset, more, n);
}

