/* $Id: msec.h,v 1.1 2011/06/27 14:30:20 jiagui Exp $ */
#ifndef MSEC_H_
#define MSEC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Fast but less precision
 */
int64_t slack_mono_msec();
int64_t slack_real_msec();


/* Slow but accurate
 */
int64_t exact_mono_msec();
int64_t exact_real_msec();



#ifdef __cplusplus
}
#endif

#endif
