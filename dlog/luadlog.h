#ifndef LUADLOG_H_
#define LUADLOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dlog_imp.h"
#include <stdbool.h>
#include <lua.h>


#define CENTER_HOST_SIZE	256

extern char dlog_center_host[CENTER_HOST_SIZE];
extern unsigned short dlog_center_port;
extern int dlog_center_revision;


typedef void luadlog_callback(struct dlog_record *rec);

int luadlog_init(luadlog_callback *cb, const char *identity);
int luaopen_dlog(lua_State *L);


#ifdef __cplusplus
}
#endif

#endif
