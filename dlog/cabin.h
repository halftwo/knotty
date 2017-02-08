#ifndef CABIN_H_
#define CABIN_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cabin_t cabin_t;


cabin_t *cabin_create(const char *log_dir);

void cabin_destroy(cabin_t *cab);

void cabin_flush(cabin_t *cab);

void cabin_put(cabin_t *cab, const char *label, const char *content, size_t len);


#ifdef __cplusplus
}
#endif

#endif
