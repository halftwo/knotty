/* $Id: cpu.h,v 1.2 2012/11/01 03:55:38 jiagui Exp $ */
#ifndef CPU_H_
#define CPU_H_ 1

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


int cpu_count();


void cpu_alignment_check(bool on);



#ifdef __cplusplus
}
#endif

#endif
