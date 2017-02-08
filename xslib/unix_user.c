#include "unix_user.h"
#include "cstr.h"
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: unix_user.c,v 1.6 2013/11/10 10:46:01 gremlin Exp $";
#endif

#define BSIZE	1024

int unix_uid2user(uid_t uid, char *user, int size)
{
	char buf[BSIZE];
	struct passwd pwbuf, *pw;

	getpwuid_r(uid, &pwbuf, buf, sizeof(buf), &pw);
	if (!pw)
		return -1;

	return cstr_ncopy(user, size, pw->pw_name);
}

int unix_user2uid(const char *user, uid_t *uid, gid_t *gid)
{
	char buf[BSIZE];
	struct passwd pwbuf, *pw;

	getpwnam_r(user, &pwbuf, buf, sizeof(buf), &pw);
	if (!pw)
		return -1;

	if (uid)
		*uid = pw->pw_uid;
	if (gid)
		*gid = pw->pw_gid;
	return 0;
}


int unix_gid2group(gid_t gid, char *group, int size)
{
	char buf[BSIZE];
	struct group grbuf, *gr;

	getgrgid_r(gid, &grbuf, buf, sizeof(buf), &gr);
	if (!gr)
		return -1;

	return cstr_ncopy(group, size, gr->gr_name);
}

int unix_group2gid(const char *group, gid_t *gid)
{
	char buf[BSIZE];
	struct group grbuf, *gr;

	getgrnam_r(group, &grbuf, buf, sizeof(buf), &gr);
	if (!gr)
		return -1;

	if (gid)
		*gid = gr->gr_gid;
	return 0;
}

static int _set_ug(const char *user, const char *group, int effective)
{
	char buf[BSIZE];
	uid_t uid = (uid_t)-1;
	gid_t gid = (gid_t)-1;
	int r = 0;

	if (user && user[0])
	{
		struct passwd pwbuf, *pw;

		getpwnam_r(user, &pwbuf, buf, sizeof(buf), &pw);
		if (!pw)
			r = -1;
		else
		{
			uid = pw->pw_uid;
			gid = pw->pw_gid;
		}
	}

	if (group && group[0])
	{
		struct group grbuf, *gr;

		getgrnam_r(group, &grbuf, buf, sizeof(buf), &gr);
		if (!gr)
			r = -1;
		else
			gid = gr->gr_gid;
	}

	if (gid != (gid_t)-1)
	{
		int rc = effective ? setegid(gid) : setgid(gid);
		if (rc < 0)
			r = -1;
	}

	if (uid != (uid_t)-1)
	{
		int rc = effective ? seteuid(uid) : setuid(uid);
		if (rc < 0)
			r = -1;
	}

	return r;
}

int unix_set_euser_egroup(const char *user, const char *group)
{
	return _set_ug(user, group, 1);
}

int unix_set_user_group(const char *user, const char *group)
{
	return _set_ug(user, group, 0);
}

