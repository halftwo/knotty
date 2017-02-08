#include "gzipper.h"
#include "xnet.h"
#include <zlib.h>
#include <string.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: gzipper.c,v 1.3 2015/04/08 02:44:49 gremlin Exp $";
#endif


ssize_t gzip_compress(const void *in, size_t isize, unsigned char **out, size_t *osize)
{
	ssize_t retval = -1;	/* error */
	int rc;
	z_stream zsm;
	size_t bound;

	zsm.zalloc = Z_NULL;
	zsm.zfree = Z_NULL;
	zsm.opaque = Z_NULL;
	rc = deflateInit2(&zsm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16+15, 8, Z_DEFAULT_STRATEGY);
	if (rc != Z_OK)
		return -1;
	
	bound = deflateBound(&zsm, isize);
	if (bound < 64)
		bound = 64;

	if (*out == NULL || *osize < bound)
	{
		unsigned char *p = (unsigned char *)realloc(*out, bound);
		if (p)
		{
			*out = p;
			*osize = bound;
		}
		else if (*out == NULL || *osize < 1)
		{
			goto error;
		}
	}

	zsm.next_in = (unsigned char *)in;
	zsm.avail_in = isize;
	zsm.next_out = *out;
	zsm.avail_out = *osize;

	rc = deflate(&zsm, Z_FINISH);
	if (rc != Z_STREAM_END)
		goto error;

	retval = *osize - zsm.avail_out;
error:
	deflateEnd(&zsm);
	return retval;
}


ssize_t gzip_decompress(const void *in, size_t isize, unsigned char **out, size_t *osize)
{
	ssize_t retval = -1;	/* error */
	int rc;
	z_stream zsm;
	uint32_t origsize;

	if (isize < 18)
		return -1;

	zsm.zalloc = Z_NULL;
	zsm.zfree = Z_NULL;
	zsm.opaque = Z_NULL;
	rc = inflateInit2(&zsm, 16+15);
	if (rc != Z_OK)
		return -1;

	memcpy(&origsize, in + isize - 4, 4);
	xnet_lsb32(&origsize);
	if (*out == NULL || *osize < origsize)
	{
		unsigned char *p = (unsigned char *)realloc(*out, origsize);
		if (p)
		{
			*out = p;
			*osize = origsize;
		}
		else 
		{
			goto error;
		}
	}

	zsm.next_in = (unsigned char *)in;
	zsm.avail_in = isize;
	zsm.next_out = *out;
	zsm.avail_out = *osize;

	rc = inflate(&zsm, Z_FINISH);
	if (rc != Z_STREAM_END)
		goto error;

	retval = *osize - zsm.avail_out;
error:
	inflateEnd(&zsm);
	return retval;
}


#ifdef TEST_GZIPPER

#include <stdio.h>

#define MAX_BUF		(1024*1024)

int main(int argc, char **argv)
{
	FILE *fp;
	char *infile, *outfile;
	unsigned char *raw = NULL;
	unsigned char *raw2 = NULL;
	unsigned char *zip = NULL;
	size_t zip_size = 0;
	size_t raw2_size = 0;
	ssize_t rawlen, raw2len, ziplen;

	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <in_file> <out_file>\n", argv[0]);
		exit(1);
	}

	infile = argv[1];
	outfile = argv[2];

	fp = fopen(infile, "rb");
	if (!fp)
	{
		fprintf(stderr, "Failed to open file \"%s\"\n", infile);
		exit(1);
	}
	raw = (char *)malloc(MAX_BUF);
	rawlen = fread(raw, 1, MAX_BUF, fp);
	fclose(fp);

	ziplen = gzip_compress(raw, rawlen, &zip, &zip_size);
	if (ziplen < 0)
	{
		fprintf(stderr, "gzip_compress() failed\n");
		exit(1);
	}

	fp = fopen(outfile, "wb");
	if (!fp)
	{
		fprintf(stderr, "Failed to open file \"%s\"\n", outfile);
		exit(1);
	}
	fwrite(zip, 1, ziplen, fp);
	fclose(fp);

	raw2len = gzip_decompress(zip, ziplen, &raw2, &raw2_size);
	if (raw2len < 0)
	{
		fprintf(stderr, "gzip_decompress() failed\n");
		exit(1);
	}

	if (raw2len != rawlen || memcmp(raw, raw2, rawlen) != 0)
	{
		fprintf(stderr, "The data is changed after compressing and decompressing\n");
		exit(1);
	}

	free(raw);
	free(raw2);
	free(zip);
	fprintf(stderr, "OK rawlen=%zd ziplen=%zd\n", rawlen, ziplen);
	return 0;
}

#endif 
