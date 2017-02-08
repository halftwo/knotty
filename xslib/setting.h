/* $Id: setting.h,v 1.1 2015/05/26 07:39:49 gremlin Exp $ */
#ifndef SETTING_H_
#define SETTING_H_

#include "xstr.h"
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct setting_t setting_t;


setting_t *setting_create();

void setting_destroy(setting_t *st);


bool setting_load(setting_t *st, const char *filename, bool replace);


xstr_t setting_get_xstr(setting_t *st, const char *key);

/* Return "" if the key is not found.
 */
const char *setting_get_cstr(setting_t *st, const char *key);

intmax_t setting_get_integer(setting_t *st, const char *key, intmax_t dft);

bool setting_get_bool(setting_t *st, const char *key, bool dft);

double setting_get_floating(setting_t *st, const char *key, double dft);



void setting_set(setting_t *st, const char *key, const char *value);

bool setting_insert(setting_t *st, const char *key, const char *value);

bool setting_update(setting_t *st, const char *key, const char *value);


#endif
