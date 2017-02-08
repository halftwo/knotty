/* $Id: pt.h,v 1.5 2012/09/20 03:21:47 jiagui Exp $ */
/*
 * Copyright (c) 2004-2005, Swedish Institute of Computer Science.
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
 * This file is part of the Contiki operating system.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef PT_H__
#define PT_H__ 1

#include "lc.h"


typedef struct
{
	lc_t lc;
} pt_t;

typedef enum
{
	PT_THREAD_WAITING = 0,
	PT_THREAD_EXITED = 1,
} pt_status_t;


/**
 * Declaration of a protothread.
 *
 * This macro is used to declare a protothread. All protothreads must
 * be declared with this macro.
 *
 * \param name_args The name and arguments of the C function
 * implementing the protothread.
 */
#define PT_THREAD(name_args) pt_status_t name_args


/**
 * Initialize a protothread.
 *
 * Initializes a protothread. Initialization must be done prior to
 * starting to execute the protothread.
 *
 * \param pt A pointer to the protothread control structure.
 *
 * \sa PT_SPAWN()
 */
#define PT_INIT(pt)   LC_INIT((pt)->lc)


/**
 * Declare the start of a protothread inside the C function
 * implementing the protothread.
 *
 * This macro is used to declare the starting point of a
 * protothread. It should be placed at the start of the function in
 * which the protothread runs. All C statements above the PT_BEGIN()
 * invokation will be executed each time the protothread is scheduled.
 *
 * \param pt A pointer to the protothread control structure.
 */
#define PT_BEGIN(pt) { PT_YIELDING(); LC_RESUME((pt)->lc)


/**
 * Block and wait until condition is true.
 *
 * This macro blocks the protothread until the specified condition is
 * true.
 *
 * \param pt A pointer to the protothread control structure.
 * \param condition The condition.
 */
#define PT_WAIT_UNTIL(pt, condition) do {	\
	LC_SET((pt)->lc);			\
	if (!(condition)) {			\
		return PT_THREAD_WAITING;	\
	}					\
} while(0)


/**
 * Block and wait while condition is true.
 *
 * This function blocks and waits while condition is true. See
 * PT_WAIT_UNTIL().
 *
 * \param pt A pointer to the protothread control structure.
 * \param cond The condition.
 */
#define PT_WAIT_WHILE(pt, cond)  PT_WAIT_UNTIL((pt), !(cond))


/**
 * Block and wait until a child protothread completes.
 *
 * This macro schedules a child protothread. The current protothread
 * will block until the child protothread completes.
 *
 * \note The child protothread must be manually initialized with the
 * PT_INIT() function before this function is used.
 *
 * \param pt A pointer to the protothread control structure.
 * \param thread The child protothread with arguments
 *
 * \sa PT_SPAWN()
 */
#define PT_WAIT_THREAD(pt, thread) PT_WAIT_WHILE((pt), PT_SCHEDULE(thread))


/**
 * Spawn a child protothread and wait until it exits.
 *
 * This macro spawns a child protothread and waits until it exits. The
 * macro can only be used within a protothread.
 *
 * \param pt A pointer to the protothread control structure.
 * \param child A pointer to the child protothread's control structure.
 * \param thread The child protothread with arguments
 */
#define PT_SPAWN(pt, child, thread)	do {	\
	PT_INIT((child));			\
	PT_WAIT_THREAD((pt), (thread));		\
} while(0)


/**
 * Restart the protothread.
 *
 * This macro will block and cause the running protothread to restart
 * its execution at the place of the PT_BEGIN() call.
 *
 * \param pt A pointer to the protothread control structure.
 */
#define PT_RESTART(pt)		do {	\
	PT_INIT(pt);			\
	return PT_THREAD_WAITING;	\
} while(0)


/**
 * Exit the protothread.
 *
 * This macro causes the protothread to exit. If the protothread was
 * spawned by another protothread, the parent protothread will become
 * unblocked and can continue to run.
 *
 * \param pt A pointer to the protothread control structure.
 */
#define PT_EXIT(pt)		do {	\
	PT_INIT(pt);			\
	return PT_THREAD_EXITED;	\
} while(0)


/**
 * Declare the end of a protothread.
 *
 * This macro is used for declaring that a protothread ends. It must
 * always be used together with a matching PT_BEGIN() macro.
 *
 * \param pt A pointer to the protothread control structure.
 */
#define PT_END(pt) LC_END((pt)->lc); pt_yielded = 0; PT_EXIT(pt); }


/**
 * Schedule a protothread.
 *
 * This function shedules a protothread. The return value of the
 * function is non-zero if the protothread is running or zero if the
 * protothread has exited.
 *
 * \param f The call to the C function implementing the protothread to
 * be scheduled
 */
#define PT_SCHEDULE(f) ((f) == PT_THREAD_WAITING)


/**
 * Declarare that a protothread can yield.
 *
 * If a protothread should be able to yield with the PT_YIELD()
 * statement, this flag must be placed first in the protothread's
 * function body.
 */
#define PT_YIELDING() int pt_yielded = 1


/**
 * Yield from the current protothread.
 *
 * This function will yield the protothread, thereby allowing other
 * processing to take place in the system.
 *
 * \note The PT_YIELDING() flag must be placed first in the
 * protothread's body if the PT_YIELD() function should be used.
 *
 * \param pt A pointer to the protothread control structure.
 */
#define PT_YIELD(pt)	do {		\
	pt_yielded = 0;			\
	PT_WAIT_UNTIL(pt, pt_yielded);	\
} while(0)

#define PT_YIELD_UNTIL(pt, cond) 	do {		\
	pt_yielded = 0;					\
	PT_WAIT_UNTIL(pt, pt_yielded && (cond));	\
} while(0)


#endif /* PT_H__ */

