/* $Id: path.c,v 1.10 2013/09/20 13:26:34 gremlin Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-09-12
 */
#include "path.h"
#include "cstr.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: path.c,v 1.10 2013/09/20 13:26:34 gremlin Exp $";
#endif


char *path_realpath(char *real_path, const char *dir, const char *name)
{
	char path[4096];
	char *p = path, *end = path + sizeof(path);
	char *r = NULL;

	if (name[0] == '/')
	{
		p = cstr_pcopy(p, end, name);
	}
	else
	{
		if (dir && dir[0])
			p = cstr_pcopy(p, end, dir);
		else
		{
			r = getcwd(path, sizeof(path));
			if (r == NULL)
				goto done;
			p = path + strlen(path);
		}

		p = cstr_pputc(p, end, '/');
		p = cstr_pcopy(p, end, name);
	}
	r = realpath(path, real_path);
done:
	if (r == NULL)
		real_path[0] = 0;
	return r;
}

size_t path_join(char *dst, const char *reference, const char *path)
{
	char *p;
	size_t len;

	if (path[0] == '/' || (p = strrchr(reference, '/')) == NULL)
	{
		p = cstr_pcopy(dst, NULL, path);
		return p - dst;
	}

	len = p + 1 - reference;
	memcpy(dst, reference, len);
	p = cstr_pcopy(dst + len, NULL, path);
	return p - dst;
}

size_t path_normalize(char *dst, const char *path)
{
	char *d, *p;
	const char *s = path;
	int n;

	if (!dst)
		dst = (char *)path;

	d = dst;
	if (s[0] == '/')
		*d++ = *s++;

	for (; (p = strchr(s, '/')) != NULL; s = p + 1)
	{
		if (p == s)
		{
			continue;
		}
		else if (s[0] == '.' )
		{
			if (p == s + 1)
			{
				s += 2;
				continue;
			}
			else if (p == s + 2 && s[1] == '.')
			{
				char *x, *z;
				s += 3;

				x = cstr_rchar(dst, d - 1, '/');
				z = x ? ++x : dst;
				if (d == dst || (d - z == 3 && z[0] == '.' && z[1] == '.'))
				{
					*d++ = '.';
					*d++ = '.';
					*d++ = '/';
				}
				else if (x || dst[0] != '/')
					d = z;
				continue;
			}
		}

		n = p + 1 - s;
		memmove(d, s, n);
		d += n;
	}

	if (s[0] == '.')
	{
		if (s[1] == 0)
		{
			++s;
		}
		else if (s[1] == '.' && s[2] == 0)
		{
			char *x, *z;
			s += 2;

			x = cstr_rchar(dst, d - 1, '/');
			z = x ? ++x : dst;
			if (d == dst || (d - z == 3 && z[0] == '.' && z[1] == '.'))
			{
				*d++ = '.';
				*d++ = '.';
			}
			else if (x || dst[0] != '/')
				d = z;
		}
	}

	while (*s)
		*d++ = *s++;

	*d = 0;
	return d - dst;
}

size_t path_normalize_mem(void *destination, const void *path, size_t len)
{
	const char *s = (const char *)path, *end = s + len;
	char *dst = destination ? (char *)destination : (char *)path;
	char *d, *p;
	ssize_t n;

	if ((ssize_t)len <= 0)
		return 0;

	d = dst;
	if (s[0] == '/')
		*d++ = *s++;

	for (; (p = memchr(s, '/', end - s)) != NULL; s = p + 1)
	{
		if (p == s)
		{
			continue;
		}
		else if (s[0] == '.' )
		{
			if (p == s + 1)
			{
				s += 2;
				continue;
			}
			else if (p == s + 2 && s[1] == '.')
			{
				char *x, *z;
				s += 3;

				x = cstr_rchar(dst, d - 1, '/');
				z = x ? ++x : dst;
				if (d == dst || (d - z == 3 && z[0] == '.' && z[1] == '.'))
				{
					*d++ = '.';
					*d++ = '.';
					*d++ = '/';
				}
				else if (x || dst[0] != '/')
					d = z;
				continue;
			}
		}

		n = p + 1 - s;
		memmove(d, s, n);
		d += n;
	}

	if (s < end && s[0] == '.')
	{
		if (s[1] == 0)
		{
			++s;
		}
		else if (s[1] == '.' && s[2] == 0)
		{
			char *x, *z;
			s += 2;

			x = cstr_rchar(dst, d - 1, '/');
			z = x ? ++x : dst;
			if (d == dst || (d - z == 3 && z[0] == '.' && z[1] == '.'))
			{
				*d++ = '.';
				*d++ = '.';
			}
			else if (x || dst[0] != '/')
				d = z;
		}
	}

	while (s < end)
		*d++ = *s++;

	return d - dst;
}


#ifdef TEST_PATH

#include <stdio.h>

void test(const char *path, const char *right)
{
	char buf[1024];
	int ok;

	strcpy(buf, path);
	path_normalize(NULL, buf);
	ok = (strcmp(buf, right) == 0); 
	printf("%d\t%s\t\t%s\n", ok, path, buf);
}

int main()
{
	test(".", 		"");
	test("./", 		"");
	test("..", 		"..");
	test("../", 		"../");
	test("a/", 		"a/");
	test("a/..", 		"");
	test("a/../", 		"");
	test("/a/..", 		"/");
	test("/a/../", 		"/");
	test("./a", 		"a");
	test("./a/", 		"a/");
	test("./a/..", 		"");
	test("./a/../", 	"");
	test("../a/..", 	"../");
	test("../a/../", 	"../");
	test("../../a/../b/c", 	"../../b/c");
	test("//a/b./.c/./d", 	"/a/b./.c/d");
	test("a/b/./../c", 	"a/c");
	test("a/b/../../c",	"c");
	test("/a/b/../../c", 	"/c");
	test("a/../b",		"b");
	test("/../ab/c////../../../c", 		"/c");
	test("////a/../ab/c/.//./../../c",	"/c");
	test("a/../ab/c/../../../c",		"../c");
	test("/a/../ab/c/../../../c",		"/c");
	return 0;
}

#endif
