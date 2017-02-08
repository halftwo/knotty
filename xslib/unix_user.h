/* $Id: unix_user.h,v 1.4 2013/11/10 10:46:01 gremlin Exp $ */
#ifndef unix_user_h_
#define unix_user_h_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: The following functions return 0 for success.
 * A negative number for error.
 */

int unix_uid2user(uid_t uid, char *user, int size);
int unix_user2uid(const char *user, uid_t *uid, gid_t *gid);

int unix_gid2group(gid_t gid, char *group, int size);
int unix_group2gid(const char *group, gid_t *gid);

/* setgid() and setuid() */
int unix_set_user_group(const char *user/*NULL*/, const char *group/*NULL*/);

/* setegid() and seteuid() */
int unix_set_euser_egroup(const char *user/*NULL*/, const char *group/*NULL*/);


#ifdef __cplusplus
}
#endif

#endif
