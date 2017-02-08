#ifndef unit_prefix_h_
#define unit_prefix_h_

#include "xstr.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * unit prefix: kKMGTPEZY
 * Support SI prefix and binary prefix.
 */

intmax_t cstr_unit_multiplier(const char *str, char **end/*NULL*/);

intmax_t xstr_unit_multiplier(const xstr_t *xs, xstr_t *end/*NULL*/);


/* 
 * unit prefix: munpfazy
 * There is only SI prefix.
 */

intmax_t cstr_unit_divider(const char *str, char **end/*NULL*/);

intmax_t xstr_unit_divider(const xstr_t *xs, xstr_t *end/*NULL*/);


#ifdef __cplusplus
}
#endif

#endif
