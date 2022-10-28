#ifndef misc_h_
#define misc_h_

#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


char *get_time_str(time_t t, bool local, char buf[]);


/* 
 * [+|-]hh[:mm[:ss]]
 */
char *get_timezone(char buf[]);


#ifdef __cplusplus
}
#endif

#endif

