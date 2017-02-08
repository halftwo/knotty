/*
   Author: XIONG Jiagui
   Date: 2007-03-21
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "urlparse.h"
#include "cstr.h"
#include "path.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: urlparse.c,v 1.21 2012/09/20 03:21:47 jiagui Exp $";
#endif


static char _invalid_chars[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


int urlparse(const xstr_t *url, struct urlpart *part)
{
	unsigned char *start = url->data;
	unsigned char *end = xstr_end(url);
	unsigned char *p;
	unsigned char *fragment;
	unsigned char *query;

	memset(part, 0, sizeof(*part));

	if (url->len == 0)
		return -1;

	for (p = start; p < end; ++p)
	{
		if (_invalid_chars[(unsigned char)*p])
			return -1;
	}

	p = (unsigned char *)memchr(start, ':', end - start);
	if (p)
	{
		unsigned char *tmp;
		int c;
		for (tmp = (unsigned char *)start; (c = *tmp++) != ':'; )
		{
			if (!isalnum(c) && c != '+' && c != '-' && c != '.')
				break;
		}
		if (c == ':')
		{
			xstr_init(&part->scheme, start, p - start);
			start = p + 1;
		}
	}

	fragment = (unsigned char *)memchr(start, '#', end - start);
	if (fragment)
	{
		xstr_init(&part->fragment, fragment + 1, end - (fragment + 1));
		end = fragment;
	}

	query = (unsigned char *)memchr(start, '?', end - start);
	if (query)
	{
		xstr_init(&part->query, query + 1, end - (query + 1));
		end = query;
	}

	if (start[0] == '/' && start[1] == '/')
	{
		unsigned char *at, *host;
		unsigned char *netloc = start + 2;
		unsigned char *path = (unsigned char *)memchr(netloc, '/', end - netloc);

		if (path)
		{
			xstr_init(&part->netloc, netloc, path - netloc); 
			xstr_init(&part->path, path, end - path);
			end = path;
		}
		else
		{
			xstr_init(&part->netloc, netloc, end - netloc); 
		}

		at = (unsigned char *)memchr(netloc, '@', end - netloc);
		if (at)
		{
			unsigned char *pass = (unsigned char *)memchr(netloc, ':', at - netloc);
			if (pass)
			{
				xstr_init(&part->user, netloc, pass - netloc);
				++pass;
				xstr_init(&part->password, pass, at - pass);
			}
			else
			{
				xstr_init(&part->user, netloc, at - netloc);
			}
			host = at + 1;
		}
		else
		{
			host = netloc;
		}

		if (host < end)
		{
			unsigned char *port = (unsigned char *)((end[-1] == ']') ? NULL : memrchr(host, ':', end - host));
			if (port)
			{
				xstr_t ptend;
				xstr_t pt = XSTR_INIT(port + 1, end - (port + 1));
				long x = xstr_to_long(&pt, &ptend, 10);
				if (x > 0 && x < 65536 && ptend.len == 0)
				{
					part->port = x;
					end = port;
				}
			}
		}

		xstr_init(&part->host, host, end - host);
	}
	else
	{
		xstr_init(&part->path, start, end - start);
	}

	return 0;
}

#define COPYXSTR(p, end, xstr)	cstr_pcopyn((p), (end), (char *)(xstr)->data, (xstr)->len)

int urlunparse(const struct urlpart *part, char *buf, int size)
{
	char *p = buf;
	char *end = p + size;

	if (part->scheme.len)
	{
		p = COPYXSTR(p, end, &part->scheme);
		p = cstr_pputc(p, end, ':');
	}

	if (part->netloc.len)
	{
		p = cstr_pcopy(p, end, "//");
		p = COPYXSTR(p, end, &part->netloc);
	}

	if (part->path.len)
	{
		if (part->netloc.len && part->path.data[0] != '/')
			p = cstr_pputc(p, end, '/');
		p = COPYXSTR(p, end, &part->path);
	}

	if (part->query.len)
	{
		p = cstr_pputc(p, end, '?');
		p = COPYXSTR(p, end, &part->query);
	}

	if (part->fragment.len)
	{
		p = cstr_pputc(p, end, '#');
		p = COPYXSTR(p, end, &part->fragment);
	}

	if (p < end)
		*p = 0;
	else if (size > 0)
		end[-1] = 0;

	return p - buf;
}



#ifdef TEST_URLPARSE

int main()
{
	int i;
	struct urlpart part;
	char buf[1024];
	char *urls[] =
	{
		"http://user:PASSWD@www.SINA.com.cn:80/a/b;k?hi#faint",
		"http://user:PASSWD@[fdec:ba98:7654:3210:fedc:ba98:7654:3210]:80/a/b;k?hi#faint",
		"http://www.SINA.com.cn:0",
		"http:www.SINA.com.cn/a/b;k?hi#faint",
		"/a/b;k?hi#faint",
		"a/b;k?hi#faint",
	};

	for (i = 0; i < sizeof(urls) / sizeof(urls[0]); ++i)
	{
		xstr_t url = XSTR_C(urls[i]);

		urlparse(&url, &part);
		urlunparse(&part, buf, sizeof(buf));

		printf("--------------------\n");
		printf("url:           %.*s\n", XSTR_P(&url));
		printf("reconstructed: %s\n", buf);
		printf("scheme: %.*s\n", XSTR_P(&part.scheme));
		printf("netloc: %.*s\n", XSTR_P(&part.netloc));
		printf("path: %.*s\n", XSTR_P(&part.path));
		printf("query: %.*s\n", XSTR_P(&part.query));
		printf("fragment: %.*s\n", XSTR_P(&part.fragment));

		printf("user: %.*s\n", XSTR_P(&part.user));
		printf("password: %.*s\n", XSTR_P(&part.password));
		printf("host: %.*s\n", XSTR_P(&part.host));
		printf("port: %u\n", part.port);
	}

	return 0;
}

#endif

