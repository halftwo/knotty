#ifndef misc_h_
#define misc_h_

#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


char *get_time_str(time_t t, bool local, char buf[]);


#ifdef __cplusplus
}
#endif

#endif

