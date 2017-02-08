/* $Id: dirwalk.h,v 1.7 2015/06/23 09:05:48 gremlin Exp $ */
#ifndef DIRWALK_H_
#define DIRWALK_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct dirwalk_item_t dirwalk_item_t;

struct dirwalk_item_t
{
	const char *path;		/* Full pathname of the file or sub-directory */
	const char *name;		/* Base name of the file or sub-directory, a pointer to the middle of path string */
	int level;			/* the level of the items in the top directory is 0 */
	bool isdir;
	const struct stat *lstat;	/* May be NULL, if lstat() failed. */
	const struct stat *stat;	/* May be NULL, if stat() failed. */
};


/* Return negative number to abort the whole walk.
 * If the returned number is positive and the item is a directory, 
 * the directory is walked recursively.
 */
typedef int (*dirwalk_item_callback_function)(const dirwalk_item_t *item, void *ctx);


/* The directory is traversed in depth first mode.
 */
int dirwalk_run(const char *dir, dirwalk_item_callback_function callback, void *ctx);



#ifdef __cplusplus
}
#endif

#endif

