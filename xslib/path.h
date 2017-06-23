/* $Id: path.h,v 1.7 2013/09/20 13:26:34 gremlin Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-09-12
 */
#ifndef path_h_
#define path_h_

#include "xstr.h"
#include <limits.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/* If ~dir~ is NULL, use the current working directory as ~dir~.
 */
char *path_realpath(char real_path[PATH_MAX], const char *dir, const char *name);


/* If ~path~ is an absolute pathname, ~dst~ is set to ~path~.
 * Otherwise, ~dst~ is (directory of ~reference~) + '/' + ~path~.
 * The length of the result string is returned.
 */
size_t path_join(char *dst, const char *reference, const char *path);


/* If ~dst~ is ~path~, the normalization is done in place.
 */
size_t path_normalize(char *dst, const char *path);

/* ~dst~ is not terminated with '\0' byte.
 */
size_t path_normalize_mem(void *dst, const void *path, size_t len);



#ifdef __cplusplus
}
#endif

#endif

