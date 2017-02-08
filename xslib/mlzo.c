/* $Id: mlzo.c,v 1.9 2013/11/13 07:51:58 gremlin Exp $ */
#include "mlzo.h"
#include "minilzo.h"
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: mlzo.c,v 1.9 2013/11/13 07:51:58 gremlin Exp $";
#endif

int mlzo_init()
{
	int rc = lzo_init();
	assert(rc == 0);
	return rc;
}

int mlzo_compress(const unsigned char *src, int src_len, unsigned char *dst)
{
	lzo_align_t wrkmem[(LZO1X_MEM_COMPRESS + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)];
	lzo_uint d_len;
	int rc;
	assert(src_len >= 0);
	rc = lzo1x_1_compress((unsigned char *)src, src_len, (unsigned char *)dst, &d_len, wrkmem);
	if (rc != LZO_E_OK)
	{
		return rc < 0 ? rc : -rc;
	}
	return d_len;
}

int mlzo_decompress_safe(const unsigned char *src, int src_len, unsigned char *dst, int dst_size)
{
	lzo_uint d_len = dst_size;
	int rc;
	assert(src_len >= 0);
	rc = lzo1x_decompress_safe((unsigned char *)src, src_len, (unsigned char *)dst, &d_len, NULL);
	if (rc != LZO_E_OK)
	{
		return rc < 0 ? rc : -rc;
	}
	return d_len;
}


