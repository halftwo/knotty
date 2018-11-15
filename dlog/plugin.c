#include "plugin.h"
#include "luadlog.h"
#include "misc.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct plugin_t
{
	lua_State *L;
	int chunk;
	int filter;
	int finish;
	int meta;
};

struct udata
{
	char time_str[16];
	const char *time;
	const char *ip;
	const char *rstr;
	uint8_t identity;
	uint8_t identity_len;
	uint8_t tag;
	uint8_t tag_len;
	uint8_t locus;
	uint8_t locus_len;
	int16_t content_len;
	struct dlog_record rec;
};

#define HANDLE	"dlog.thelog"

static inline void _make_time(struct udata *ud)
{
	ud->time = ud->time_str;
	get_time_str(ud->rec.msec/1000, true, ud->time_str);
}

static inline void _make_identity(struct udata *ud)
{
	const char *p = (const char *)memchr(ud->rstr, ' ', ud->rec.locus_end);
	ud->identity = 0;
	ud->identity_len = p ? p - ud->rstr : ud->rec.locus_end;
}

static inline void _make_tag(struct udata *ud)
{
	const char *p;
	if (ud->identity == 0xff)
		_make_identity(ud);

	ud->tag = ud->identity_len < ud->rec.locus_end ? ud->identity_len + 1 : ud->rec.locus_end;
	p = (const char *)memchr(ud->rstr + ud->tag, ' ', ud->rec.locus_end - ud->tag);
	ud->tag_len = (p ? p - ud->rstr : ud->rec.locus_end) - ud->tag;
}

static inline void _make_locus(struct udata *ud)
{
	if (ud->tag == 0)
		_make_tag(ud);

	ud->locus = ud->tag + ud->tag_len < ud->rec.locus_end ? ud->tag + ud->tag_len + 1 : ud->rec.locus_end;
	ud->locus_len = ud->rec.locus_end - ud->locus;
}

int thelog_tostring(lua_State *L)
{
	struct udata *ud = luaL_checkudata(L, 1, HANDLE);
	if (!ud->rstr)
	{
		lua_pushstring(L, "");
		return 1;
	}

	if (!ud->time)
		_make_time(ud);

	lua_pushfstring(L, "%s %s %d+%d %s", ud->time, ud->ip, ud->rec.pid, ud->rec.port, ud->rstr);
	return 1;
}

int thelog_index(lua_State *L)
{
	struct udata *ud = luaL_checkudata(L, 1, HANDLE);
	const char *key;
	int miss = 0;

	if (!ud->rstr)
	{
		lua_pushstring(L, "");
		return 1;
	}

	key = luaL_checkstring(L, 2);
	switch (key[0])
	{
	case 'c':
	 	if (strcmp(key, "content") == 0)
			lua_pushlstring(L, ud->rstr + ud->rec.locus_end + 1, ud->content_len);
		else
			miss = 1;
		break;
	case 'i':
		if (strcmp(key, "identity") == 0)
		{
			if (ud->identity == 0xff)	
				_make_identity(ud);
			lua_pushlstring(L, ud->rstr + ud->identity, ud->identity_len);
		}
		else if (strcmp(key, "ip") == 0)
			lua_pushstring(L, ud->ip);
		else
			miss = 1;
		break;
	case 'l':
		if (strcmp(key, "locus") == 0)
		{
			if (ud->locus == 0)	
				_make_locus(ud);
			lua_pushlstring(L, ud->rstr + ud->locus, ud->locus_len);
		}
		else
			miss = 1;
		break;
	case 'p':
		if (strcmp(key, "pid") == 0)
			lua_pushinteger(L, ud->rec.pid);
		else if (strcmp(key, "port") ==  0)
			lua_pushinteger(L, ud->rec.port);
		else
			miss = 1;
		break;
	case 's':
		if (strcmp(key, "str") == 0)
		{
			if (!ud->time)
				_make_time(ud);
			lua_pushfstring(L, "%s %s %d+%d %s", ud->time, ud->ip, ud->rec.pid, ud->rec.port, ud->rstr);
		}
		else
			miss = 1;
		break;
	case 't':
		if (strcmp(key, "tag") == 0)
		{
			if (ud->tag == 0)
				_make_tag(ud);
			lua_pushlstring(L, ud->rstr + ud->tag, ud->tag_len);
		}
		else if (strcmp(key, "time") == 0)
		{
			if (!ud->time)
				_make_time(ud);
			lua_pushstring(L, ud->time);
		}
		else if (strcmp(key, "truncated") == 0)
			lua_pushboolean(L, ud->rec.truncated);
		else
			miss = 1;
		break;
	default:
		miss = 1;
	}

	if (miss)
	{
		lua_pushstring(L, "");
	}
	return 1;
}

plugin_t *plugin_load(const char *filename)
{
	int rc;
	plugin_t *pg = calloc(1, sizeof(pg[0]));
	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	luaopen_dlog(L);
	lua_pop(L, 1);

	rc = luaL_loadfile(L, filename);
	if (rc)
	{
		goto error;
	}
	pg->chunk = lua_gettop(L);
	lua_pushvalue(L, pg->chunk);
	rc = lua_pcall(L, 0, 0, 0);
	if (rc)
	{
		goto error;
	}

	lua_getglobal(L, "init");
	if (!lua_isfunction(L, lua_gettop(L)))
	{
		lua_pop(L, 1);
	}
	else
	{
		rc = lua_pcall(L, 0, 0, 0);
		if (rc)
			goto error;
	}

	lua_getglobal(L, "filter");
	pg->filter = lua_gettop(L);
	if (!lua_isfunction(L, pg->filter))
	{
		lua_pop(L, 1);
		pg->filter = 0;
	}

	lua_getglobal(L, "finish");
	pg->finish = lua_gettop(L);
	if (!lua_isfunction(L, pg->finish))
	{
		lua_pop(L, 1);
		pg->finish = 0;
	}

	luaL_newmetatable(L, "dlog.thelog");
	pg->meta = lua_gettop(L);
	lua_pushcfunction(L, thelog_index);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, thelog_tostring);
	lua_setfield(L, -2, "__tostring");

	pg->L = L;
	return pg;
error:
	if (L)
		lua_close(L);
	if (pg)
		free(pg);
	return NULL;
}

void plugin_close(plugin_t *pg)
{
	if (pg)
	{
		lua_State *L = pg->L;
		if (pg->finish)
		{
			lua_pushvalue(L, pg->finish);
			lua_pcall(L, 0, 0, 0);
		}
		lua_close(L);
		free(pg);
	}
}

int plugin_filter(plugin_t *pg, const char *time_str, const char *ip, struct dlog_record *rec, const char *rstr)
{
	if (!pg->filter)
		return 0;

	int res = 0;
	int rc;
	lua_State *L = pg->L;

	int top = lua_gettop(L);
	lua_pushvalue(L, pg->filter);

	struct udata *ud = lua_newuserdata(L, sizeof(struct udata) + 1);
	if (!rstr)
		rstr = rec->str;

	ud->rec = *rec;
	ud->rec.str[0] = 0;

	ud->ip = ip;
	ud->time = time_str;
	ud->rstr = rstr;

	ud->identity = 0xff;
	ud->identity_len = 0;
	ud->tag = 0;
	ud->tag_len = 0;
	ud->locus = 0;
	ud->locus_len = 0;

	ud->content_len = rec->size - (DLOG_RECORD_HEAD_SIZE + ud->rec.locus_end + 2);

	lua_pushvalue(L, pg->meta);
	lua_setmetatable(L, -2);

	rc = lua_pcall(L, 1, 1, 0);
	res = rc ? -1 : lua_toboolean(L, -1);
	ud->rstr = NULL;	/* invalidate it */

	lua_settop(L, top);

	return res;
}

