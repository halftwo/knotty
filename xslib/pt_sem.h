/* $Id: pt_sem.h,v 1.5 2012/09/20 03:21:47 jiagui Exp $ */
/*
 * Copyright (c) 2004, Swedish Institute of Computer Science.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * This file is part of the protothreads library.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef PT_SEM_H__
#define PT_SEM_H__ 1


/**
 *
 * This module implements counting semaphores on top of
 * protothreads. Semaphores are a synchronization primitive that
 * provide two operations: "wait" and "signal". The "wait" operation
 * checks the semaphore counter and blocks the thread if the counter
 * is zero. The "signal" operation increases the semaphore counter but
 * does not block. If another thread has blocked waiting for the
 * semaphore that is signalled, the blocked thread will become
 * runnable again.
 *
 * Semaphores can be used to implement other, more structured,
 * synchronization primitives such as monitors and message
 * queues/bounded buffers (see below).
 *
 */
   
#include "pt.h"

typedef struct
{
	unsigned int count;
} pt_sem_t;


/**
 * Initialize a semaphore
 *
 * This macro initializes a semaphore with a value for the
 * counter. Internally, the semaphores use an "unsigned int" to
 * represent the counter, and therefore the "count" argument should be
 * within range of an unsigned int.
 *
 * \param s (pt_sem_t *) A pointer to the pt_sem_t
 * representing the semaphore
 *
 * \param c (unsigned int) The initial count of the semaphore.
 */
#define PT_SEM_INIT(s, c) 	(s)->count = (c)


/**
 * Wait for a semaphore
 *
 * This macro carries out the "wait" operation on the semaphore. The
 * wait operation causes the protothread to block while the counter is
 * zero. When the counter reaches a value larger than zero, the
 * protothread will continue.
 *
 * \param pt (pt_t *) A pointer to the protothread (pt_t) in
 * which the operation is executed.
 *
 * \param s (pt_sem_t *) A pointer to the pt_sem_t
 * representing the semaphore
 */
#define PT_SEM_WAIT(pt, s) 	do {		\
	PT_WAIT_UNTIL(pt, (s)->count > 0);	\
	--(s)->count;				\
} while(0)


/**
 * Signal a semaphore
 *
 * This macro carries out the "signal" operation on the semaphore. The
 * signal operation increments the counter inside the semaphore, which
 * eventually will cause waiting protothreads to continue executing.
 *
 * \param pt (pt_t *) A pointer to the protothread (pt_t) in
 * which the operation is executed.
 *
 * \param s (pt_sem_t *) A pointer to the pt_sem_t
 * representing the semaphore
 */
#define PT_SEM_SIGNAL(pt, s) 	++(s)->count


#endif /* PT_SEM_H__ */

