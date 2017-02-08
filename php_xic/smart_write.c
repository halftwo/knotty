#include "smart_write.h"
#include "ext/standard/php_smart_str.h"

ssize_t smart_write(void *cookie, const void *data, size_t size)
{
	smart_str *buf = (smart_str *)cookie;
	if (size > 1)
		smart_str_appendl(buf, (char *)data, size);
	else if (size == 1)
		smart_str_appendc(buf, *(char *)data);
	return size;
}

void file_cookie_init(struct file_cookie* fc)
{
	fc->stream = NULL;
	fc->total = 0;
	fc->blen = 0;
}

ssize_t file_cookie_finish(struct file_cookie* fc)
{
	if (fc->blen > 0)
	{
		ssize_t n = php_stream_write(fc->stream, fc->buf, fc->blen);
		fc->blen = 0;
		if (n > 0)
			fc->total += n;
	}
	return fc->total;
}

ssize_t file_write(void *cookie, const void *data, size_t size)
{
	struct file_cookie *fc = (struct file_cookie *)cookie;

	if (fc->blen + size < FILE_COOKIE_BUF_SIZE)
	{
		if (size == 1)
		{
			fc->buf[fc->blen++] = *(unsigned char *)data;
		}
		else
		{
			memcpy(fc->buf + fc->blen, data, size);
			fc->blen += size;
		}
	}
	else
	{
		ssize_t n;
		if (fc->blen > 0)
		{
			n = php_stream_write(fc->stream, fc->buf, fc->blen);
			if (n < fc->blen)
				return -1;
			fc->total += n;
			fc->blen = 0;
		}

		if (size < FILE_COOKIE_BUF_SIZE/2)
		{
			memcpy(fc->buf + fc->blen, data, size);
			fc->blen += size;
		}
		else
		{
			n = php_stream_write(fc->stream, data, size);
			if (n < size)
				return -1;
			fc->total += n;
		}
	}
	return size;
}

