#ifndef RECPOOL_H_
#define RECPOOL_H_

#include "dlog_imp.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dlog_record *recpool_acquire();
void recpool_release(struct dlog_record *rec);
void recpool_release_all(struct dlog_record *recs[], size_t num);


#ifdef __cplusplus
}
#endif

#endif
