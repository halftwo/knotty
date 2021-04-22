#ifndef PLUGIN_H_
#define PLUGIN_H_

#include "dlog_imp.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct plugin_t plugin_t;

plugin_t *plugin_load(const char *filename);

void plugin_close(plugin_t *pg);

int plugin_filterout(plugin_t *pg, const char *time_str, const char *ip, struct dlog_record *rec, const char *recstr);


#ifdef __cplusplus
}
#endif

#endif

