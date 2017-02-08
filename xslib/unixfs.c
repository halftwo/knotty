#include "unixfs.h"
#include "bit.h"
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>

#define DELTA_MIN	4096
#define DELTA_MAX	(1024*104*4)

static int _mkdir(char *pathname, int len, mode_t mode, uid_t uid, gid_t gid)
{
	int r;

	assert(pathname[len-1] != '/');

	while (len > 0 && pathname[--len] != '/')
		continue;

	if (pathname[len] == '/')
	{
		pathname[len] = 0;

		r = access(pathname, F_OK);
		if (r == -1 && errno == ENOENT)
			r = _mkdir(pathname, len, mode, uid, gid);

		pathname[len] = '/';

		if (r < 0)
			return -1;
	}

	r = mkdir(pathname, mode);

	if (r == 0 || errno == EEXIST)
	{
		if (uid != (uid_t)-1 || gid != (gid_t)-1)
		{
			if (chown(pathname, uid, gid) == -1)
				return 1;
		}
		return 0;
	}

	return -1;
}

int unixfs_mkdir(const char *pathname, mode_t mode, uid_t uid, gid_t gid)
{
	int len;
	int r;

	if (pathname[0] == 0)
		return -1;

	r = mkdir(pathname, mode);

	if (r == 0 || errno == EEXIST)
	{
		if (uid != (uid_t)-1 || gid != (gid_t)-1)
		{
			if (chown(pathname, uid, gid) == -1)
				return 1;
		}
		return 0;
	}

	if (errno != ENOENT)
		return -1;

	len = strlen(pathname);
	if (len > 0 && pathname[len - 1] == '/')
		--len;

	if (len > 0)
	{
		char *path = (char *)malloc(len + 1);
		memcpy(path, pathname, len);
		path[len] = 0;
		r = _mkdir(path, len, mode, uid, gid);
		free(path);
	}

	return r;
}

int unixfs_open(const char *pathname, int flags, mode_t mode)
{
	int fd = open(pathname, flags, mode);
	if (fd == -1 && (flags & O_CREAT) && errno == ENOENT)
	{
		char *p = strrchr(pathname, '/');
		if (p && p > pathname)
		{
			int rc;
			int len = p - pathname;
			char *path = (char *)malloc(len + 1);
			memcpy(path, pathname, len);
			path[len] = 0;
			rc = _mkdir(path, len, 0775, -1, -1);
			free(path);

			if (rc == 0)
			{
				fd = open(pathname, flags, mode);
			}
		}
	}
	return fd;
}

ssize_t unixfs_get_content_from_fd(int fd, unsigned char **p_content, size_t *p_size)
{
	struct stat st;
	unsigned char *content = *p_content;
	ssize_t size = *p_size;
	ssize_t fsize = (fstat(fd, &st) == 0) ? st.st_size : -1;
	ssize_t n = 0;

	if (content == NULL || size < fsize + 2)
	{
		if (size < fsize + 2)
			size = fsize + 2;
		if (size < DELTA_MIN)
			size = DELTA_MIN;

		content = (unsigned char *)realloc(content, size);
		if (!content)
			goto error;

		*p_content = content;
		*p_size = size;
	}

	while (1)
	{
		ssize_t rc;
		if (size - 1 - n <= 0)
		{
			ssize_t delta = round_up_power_two(size) - size;
			if (delta < DELTA_MIN)
				delta = DELTA_MIN;
			else if (delta > DELTA_MAX)
				delta = DELTA_MAX;

			size += delta;
			content = (unsigned char *)realloc(content, size);
			if (!content)
				goto error;

			*p_content = content;
			*p_size = size;
		}

		rc = read(fd, content + n, size - 1 - n);
		if (rc < 0)
			goto error;
		else if (rc == 0)
			break;

		n += rc;
	}
	content[n] = 0;
	return n;
error:
	return -1;
}

ssize_t unixfs_get_content(const char *pathname, unsigned char **p_content, size_t *p_size)
{
	ssize_t rc;
	int fd = open(pathname, O_RDONLY);
	if (fd == -1)
		return -1;

	rc = unixfs_get_content_from_fd(fd, p_content, p_size);
	close(fd);
	return rc;
}

ssize_t unixfs_put_content(const char *pathname, const void *content, size_t size)
{
	int fd;
	ssize_t n;

	if (size < 0)
		return -1;

	fd = open(pathname, O_WRONLY | O_CREAT, 0664);
	if (fd == -1)
		return -1;

	n = write(fd, content, size);
	close(fd);
	return n;
}

#ifdef TEST_UNIXFS

int main()
{
	int r;
	ssize_t n;
	char *buf = NULL;
	size_t size = 0;
	n = unixfs_get_content("/proc/cpuinfo", &buf, &size);
	printf("n=%zd\n", n);
	r = unixfs_mkdir("../../../../../../../../../../a/b/c/d/e/", 0777, -1, -1);
	printf("r=%d\n", r);
	return 0;
}

#endif
