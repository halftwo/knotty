/* $Id: rdtsc.h,v 1.6 2012/06/11 10:35:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-03-09
 */
#ifndef RDTSC_H_
#define RDTSC_H_ 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


extern uint64_t xs_cpu_frequency;


/* Read the Time Stamp Counter.
 */
uint64_t rdtsc();


uint64_t tsc_diff(uint64_t a, uint64_t b);


/* This function will update extern variable xs_cpu_frequency. 
 */
uint64_t get_cpu_frequency(unsigned int milliseconds);


uint64_t cpu_frequency();


#ifdef __cplusplus
}
#endif

#endif
