/* $Id: cirqueue.h,v 1.4 2009/07/10 08:30:48 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2007-01-19
 */
#ifndef CIRQUEUE_H_
#define CIRQUEUE_H_ 1


#include <stddef.h>
#include <stdbool.h>
#include <time.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct cirqueue_t cirqueue_t;


cirqueue_t *cirqueue_create(size_t size);

void cirqueue_destroy(cirqueue_t *q);


size_t cirqueue_used(cirqueue_t *q);

size_t cirqueue_size(cirqueue_t *q);


bool cirqueue_put(cirqueue_t *q, void *element, bool block);

void *cirqueue_get(cirqueue_t *q, bool block); 


size_t cirqueue_putv(cirqueue_t *q, void *elements[], size_t num, bool block); 

size_t cirqueue_getv(cirqueue_t *q, void *elements[], size_t num, bool block); 


size_t cirqueue_timed_putv(cirqueue_t *q, void *elements[], size_t num, const struct timespec *abstime);

size_t cirqueue_timed_getv(cirqueue_t *q, void *elements[], size_t num, const struct timespec *abstime);


#ifdef __cplusplus
}
#endif

#endif

