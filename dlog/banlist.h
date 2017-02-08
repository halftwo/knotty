#ifndef BANLIST_H_
#define BANLIST_H_

#include "dlog_imp.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct banlist_t banlist_t;


banlist_t *banlist_load(const char *filename);


void banlist_close(banlist_t *b);


bool banlist_check(banlist_t *b, const char *ip, struct dlog_record *rec);



#ifdef __cplusplus
}
#endif

#endif


