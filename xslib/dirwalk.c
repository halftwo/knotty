#include "dirwalk.h"
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: dirwalk.c,v 1.9 2015/06/23 09:07:02 gremlin Exp $";
#endif


typedef struct 
{
	dirwalk_item_callback_function callback;
	void *ctx;
	dirwalk_item_t item;
	int maxlen;
	struct stat lstat;
	struct stat stat;
} direnv_t;


static int _walk(direnv_t *env, int base)
{
	int ret = -1;
	dirwalk_item_t *item = &env->item;

	DIR *dirp = opendir(item->path);
	if (!dirp)
		return -1;

	((char*)item->path)[base++] = '/';

	while (1)
	{
		int rc, namelen;
		struct dirent *de = readdir(dirp);
		if (!de)
			break;

		if (de->d_name[0] == '.' && (de->d_name[1] == 0 || (de->d_name[1] == '.' && de->d_name[2] == 0)))
			continue;
		
		namelen = strlen(de->d_name);
		if (base + namelen >= env->maxlen)
			goto done;

		item->name = item->path + base;
		memcpy((char*)item->name, de->d_name, namelen + 1);

		rc = lstat(item->path, &env->lstat);
		item->lstat = (rc == 0) ? &env->lstat : NULL;
		if (item->lstat && S_ISLNK(item->lstat->st_mode))
		{
			rc = stat(item->path, &env->stat);
			item->stat = (rc == 0) ? &env->stat : NULL;
		}
		else
		{
			item->stat = &env->lstat;
		}

		item->isdir = item->stat && S_ISDIR(item->stat->st_mode);

		rc = env->callback(item, env->ctx);
		if (rc < 0)
			goto done;

		if (item->isdir && rc > 0)
		{
			item->level++;
			rc = _walk(env, base + namelen);
			item->level--;
			if (rc < 0)
				goto done;
		}
	}
	ret = 0;
done:
	closedir(dirp);
	return ret;
}

int dirwalk_run(const char *dir, dirwalk_item_callback_function callback, void *ctx)
{
	int rc;
	int n = strlen(dir);
	int maxlen;
	char *path;
	direnv_t env;

	if (n == 0)
		return -1;

	while (n > 0 && dir[n-1] == '/')
	{
		--n;
	}

	maxlen = pathconf(dir, _PC_PATH_MAX);
	if (maxlen == -1)
		maxlen = 4096;

	if (n + 2 >= maxlen)
		return -1;

	path = (char*)malloc(maxlen);
	memcpy(path, dir, n);
	path[n] = 0;

	env.callback = callback;
	env.ctx = ctx;
	env.maxlen = maxlen;
	env.item.path = path;
	env.item.level = 0;
	env.item.isdir = true;

	rc = _walk(&env, n);

	free(path);
	return rc;
}


#ifdef TEST_DIRWALK

#include <stdio.h>

static int callback(const dirwalk_item_t *item, void *ctx)
{
	printf("%d %s %s\n", item->level, item->name, item->path);
	return (item->lstat && !S_ISLNK(item->lstat->st_mode));
}

int main(int argc, char **argv)
{
	char *dir = argc > 1 ? argv[1] : ".";
	int rc = dirwalk_run(dir, callback, NULL);
	return 0;
}

#endif
