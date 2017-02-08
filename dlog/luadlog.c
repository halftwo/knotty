#include "luadlog.h"
#include "dlog_imp.h"
#include "recpool.h"
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#define LUA_DLOGLIBNAME		"dlog"

char dlog_center_host[CENTER_HOST_SIZE];
unsigned short dlog_center_port = DLOG_CENTER_PORT;
int dlog_center_revision;

static struct dlog_record _record_prototype = { .version = DLOG_RECORD_VERSION, .type = DLOG_TYPE_COOKED, };
static luadlog_callback *_callback;
static const char *_identity;
static size_t _identity_len;

int luadlog_init(luadlog_callback *cb, const char *identity)
{
	_record_prototype.pid = getpid();
	_callback = cb;
	_identity = identity;
	_identity_len = strlen(_identity);
	return 0;
}

static int ldlog_set_center(lua_State *L)
{
	size_t len;
	const char *host = luaL_checklstring(L, 1, &len);
	int port = luaL_optint(L, 2, INT_MIN);

	if (len >= CENTER_HOST_SIZE)
		return luaL_error(L, "%s", "hostname too long for dlog.set_center()");

	if (port == INT_MIN)
	{
		char *p = strchr(host, '+');
		if (p)
		{
			char *end;
			len = p - host;
			port = strtol(p + 1, &end, 10);
			if (*end)
				port = -1;
		}
		else
		{
			port = DLOG_CENTER_PORT;
		}
	}

	if (port <= 0 || port > 65535)
		return luaL_error(L, "%s", "port is invalid for dlog.set_center()");

	if (strncmp(dlog_center_host, host, len) != 0)
	{
		memcpy(dlog_center_host, host, len);
		dlog_center_host[len] = 0;
		++dlog_center_revision;
	}

	if (dlog_center_port != port)
	{
		dlog_center_port = port;
		++dlog_center_revision;
	}

	return 0;
}

static int ldlog_cook(lua_State *L)
{
	struct dlog_record *rec;
	char *p, *end;
	size_t size;
	const char *locus;
	int num, i;

	if (!_callback)
		return 0;

	locus = luaL_checklstring(L, 1, &size);
	num  = lua_gettop(L);  /* number of arguments */
	if (!locus)
		return 0;

	rec = recpool_acquire();
	*rec = _record_prototype;

	p = rec->str; 
	end = (char *)rec + DLOG_RECORD_MAX_SIZE - 1;

	memcpy(p, _identity, _identity_len);
	p += _identity_len;
	*p++ = ' ';

	memcpy(p, "COOK", 4);
	p += 4;
	*p++ = ' ';

	if (size > DLOG_LOCUS_MAX)
		size = DLOG_LOCUS_MAX;
	memcpy(p, locus, size);
	p += size;
	rec->locus_end = p - rec->str;
	*p++ = ' ';

	lua_getglobal(L, "tostring");
	for (i = 2; i <= num; ++i)
	{
		const char *s;
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);
		s = lua_tolstring(L, -1, &size);
		if (s != NULL)
		{
			if (p + size >= end)
			{
				memcpy(p, s, end - p);
				p = end;
				break;
			}
			else
			{
				memcpy(p, s, size);
				p += size;
				if (p + 1 >= end)
					break;
				*p++ = ' ';
			}
		}
		lua_pop(L, 1);
	}
	*p++ = 0;

	rec->size = p - (char *)rec;
	_callback(rec);
	return 0; 
}

static const luaL_Reg dloglib[] = {
	{ "cook", ldlog_cook },
	{ "set_center", ldlog_set_center },
	{ NULL, NULL },
};

int luaopen_dlog(lua_State *L)
{
	luaL_register(L, LUA_DLOGLIBNAME, dloglib);
	return 1; 
} 

