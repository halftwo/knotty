#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "zlog.h"
#include "xlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

#define COMPRESS_SUFFIX		".lz4"

struct zlog_t
{
	pthread_mutex_t mutex;
	char *prefix;
	int flags;
	size_t file_size;
	size_t cur_size;
	FILE *fp;
	char *filename;
};

static void get_time(time_t *t)
{
	time(t);
}

static void _do_compress(const char *pathname)
{
	char zfile[PATH_MAX];
	char cmd[PATH_MAX];
	char *file = basename(strdupa(pathname));
	char *dir = dirname(strdupa(pathname));
	int rc;
	int status = -1;

	snprintf(zfile, sizeof(zfile), "%s/%s%s", dir, file, COMPRESS_SUFFIX);
	snprintf(cmd, sizeof(cmd), 
		"/usr/bin/env PATH=.:/usr/local/bin:/usr/bin lz4 -f %s %s 2> /dev/null", 
		pathname, zfile);
	rc = system(cmd);
	status = WEXITSTATUS(rc);
	if (status == 0)
	{
		unlink(pathname);
	}
}

static void *_compressor(void *arg)
{
	char *pathname = (char *)arg;
	if (pathname)
	{
		_do_compress(pathname);
		free(pathname);
	}
	return NULL;
}

static void _do_lz4(char *filename)
{
	if (filename)
	{
		pthread_t thr;
		pthread_attr_t attr;

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&thr, &attr, _compressor, filename) != 0)
			free(filename);
		pthread_attr_destroy(&attr);
	}
}

zlog_t *zlog_create(const char *prefix, size_t file_size, int flags)
{
	time_t now;
	struct tm tm;
	int flen = strlen(prefix) + 14;
	zlog_t *zl = (zlog_t *)calloc(1, sizeof(zlog_t));
	if (zl == NULL)
		return NULL;
	pthread_mutex_init(&zl->mutex, NULL);

	zl->prefix = strdup(prefix);
	if (zl->prefix == NULL)
		goto error;

	zl->filename = (char *)malloc(flen);
	if (zl->filename == NULL)
		goto error;

	zl->file_size = file_size;
	zl->flags = flags;
		
	memcpy(zl->filename, zl->prefix, flen - 14);
	get_time(&now);
	localtime_r(&now, &tm);
	strftime(&zl->filename[flen - 14], 14, "%y%m%d-%H%M%S",  &tm);

	zl->fp = fopen(zl->filename, "ab");
	if (zl->fp == NULL)
		goto error;

	return zl;
error:
	zlog_destroy(zl);
	return NULL;
}

void zlog_destroy(zlog_t *zl)
{
	if (zl)
	{
		pthread_mutex_destroy(&zl->mutex);

		if (zl->fp)
			fclose(zl->fp);

	   	if ((zl->flags & ZLOG_AUTO_COMPRESS) && zl->cur_size > 0)
		{
			pthread_t thr;
			if (pthread_create(&thr, NULL, _compressor, zl->filename) == 0)
			{
				pthread_join(thr, NULL);
				zl->filename = NULL;
			}
		}

		free(zl->prefix);
		free(zl->filename);
		free(zl);
	}
}

static void _file_truncate(FILE *fp, int size)
{
	fflush(fp);
	ftruncate(fileno(fp), size);
}

static char* _do_zlog_switch(zlog_t *zl)
{
	time_t now;
	struct tm tm;
	int flen = strlen(zl->filename) + 1;
	char *filename = (char *)alloca(flen);
	char *oldfile = NULL;
	FILE *fp;

	assert(flen >= 14);
	get_time(&now);
	localtime_r(&now, &tm);
	memcpy(filename, zl->filename, flen - 14);
	strftime(&filename[flen - 14], 14, "%y%m%d-%H%M%S",  &tm);
	if (strcmp(filename, zl->filename) == 0)
	{
		return NULL;
	}

	fp = fopen(filename, "ab");
	if (fp == NULL)
		return NULL;

	fflush(zl->fp);
	fseek(zl->fp, 0L, SEEK_END);
	if (ftell(zl->fp) == 0)
	{
		remove(zl->filename);
	}
	fclose(zl->fp);

	zl->fp = fp;
	fseek(zl->fp, 0L, SEEK_END);
	zl->cur_size = ftell(zl->fp);

	if (zl->flags & ZLOG_AUTO_COMPRESS)
	{
		oldfile = zl->filename;	
		zl->filename = strdup(filename);
	}
	else
		memcpy(zl->filename, filename, flen);
	return oldfile;
}

static char* _do_zlog_vprintf(zlog_t *zl, const char *format, va_list ap)
{
	va_list ap2;
	int nl = 0;
	int err = 0;
	bool switched = false;
	int size;
	char *oldfile = NULL;

	if (format[0] == 0)
		return NULL;

	if (!(zl->flags & ZLOG_NO_NEWLINE))
	{
		int len = strlen(format);
		if (format[len-1] != '\n')
			nl = 1;
	}
	
	va_copy(ap2, ap);
	size = vfprintf(zl->fp, format, ap);
	if (zl->cur_size + size + nl > zl->file_size)
	{
		_file_truncate(zl->fp, zl->cur_size);
		oldfile = _do_zlog_switch(zl);
		switched = true;
		size = vfprintf(zl->fp, format, ap2);
	}

	err |= size < 0;
	err |= (nl && fputc_unlocked('\n', zl->fp) == EOF);
	if (err)
	{
		if (!switched && zl->cur_size > 0)
		{
			oldfile = _do_zlog_switch(zl);
			switched = true;
			size = vfprintf(zl->fp, format, ap2);
			err = 0;
			err |= size < 0;
			err |= (nl && fputc_unlocked('\n', zl->fp) == EOF);
		}
	}
	va_end(ap2);

	if (err)
	{
		xlog(XLOG_ALERT, "Error at %s", XS_FILE_LINE);
		fseek(zl->fp, 0, SEEK_END);
		zl->cur_size = ftell(zl->fp);
	}
	else
		zl->cur_size += size + nl;

	if (zl->flags & ZLOG_FLUSH_EVERYLOG)
		fflush_unlocked(zl->fp);
	return oldfile;
}

static char* _do_zlog_write(zlog_t *zl, const char *str, int size)
{
	int nl = 0;
	bool switched = false;
	int err = 0;
	char *oldfile = NULL;

	assert(size > 0);

	if (!(zl->flags & ZLOG_NO_NEWLINE) && str[size-1] != '\n')
		nl = 1;

	if (zl->cur_size + size + nl > zl->file_size)
	{
		oldfile = _do_zlog_switch(zl);
		switched = true;
	}
again:
	err = 0;
	err |= (fwrite_unlocked(str, size, 1, zl->fp) < 1);
	err |= (nl && putc_unlocked('\n', zl->fp) == EOF);
	if (err)
	{
		if (!switched && zl->cur_size > 0)
		{
			_file_truncate(zl->fp, zl->cur_size);
			oldfile = _do_zlog_switch(zl);
			switched = true;
			goto again;
		}
		else
		{
			/* Error */
			xlog(XLOG_ALERT, "Error at %s", XS_FILE_LINE);
			fseek(zl->fp, 0, SEEK_END);
			zl->cur_size = ftell(zl->fp);
		}
	}
	else
		zl->cur_size += size + nl;
		
	if (zl->flags & ZLOG_FLUSH_EVERYLOG)
		fflush_unlocked(zl->fp);
	return oldfile;
}

void zlog_flush(zlog_t *zl)
{
	pthread_mutex_lock(&zl->mutex);
	fflush_unlocked(zl->fp);
	pthread_mutex_unlock(&zl->mutex);
}

void zlog_switch(zlog_t *zl)
{
	char *oldfile;

	pthread_mutex_lock(&zl->mutex);
	oldfile = _do_zlog_switch(zl);
	pthread_mutex_unlock(&zl->mutex);

	if (oldfile)
		_do_lz4(oldfile);
}

void zlog_printf(zlog_t *zl, const char *format, ...)
{
	va_list ap;
	char *oldfile;

	va_start(ap, format);
	pthread_mutex_lock(&zl->mutex);
	oldfile = _do_zlog_vprintf(zl, format, ap);
	pthread_mutex_unlock(&zl->mutex);
	va_end(ap);

	if (oldfile)
		_do_lz4(oldfile);
}

void zlog_vprintf(zlog_t *zl, const char *format, va_list ap)
{
	char *oldfile;

	pthread_mutex_lock(&zl->mutex);
	oldfile = _do_zlog_vprintf(zl, format, ap);
	pthread_mutex_unlock(&zl->mutex);

	if (oldfile)
		_do_lz4(oldfile);
}

void zlog_write(zlog_t *zl, const char *str, int size)
{
	char *oldfile;

	if (size <= 0)
	{
		size = strlen(str);
		if (size == 0)
			return;
	}

	pthread_mutex_lock(&zl->mutex);
	oldfile = _do_zlog_write(zl, str, size);
	pthread_mutex_unlock(&zl->mutex);

	if (oldfile)
		_do_lz4(oldfile);
}


#ifdef TEST_ZLOG

int main()
{
	int i;
	zlog_t *zl = zlog_create("zzzz.", 300, ZLOG_AUTO_COMPRESS);
	for (i = 0; i < 100; ++i)
	{
		usleep(100*1000);
		zlog_printf(zl, "%d hahahahaha", i);
	}
	zlog_destroy(zl);
	return 0;
}

#endif 
