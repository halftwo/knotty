/* $Id: coroutine.h,v 1.3 2012/09/20 03:21:47 jiagui Exp $ */
/* coroutine.h
 * 
 * Coroutine mechanics, implemented on top of standard ANSI C. See
 * http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html for
 * a full discussion of the theory behind this.
 * 
 * Ground rules:
 *  - never put `CR_RETURN' within an explicit `switch'.
 *  - never put two `CR_RETURN' statements on the same source line.
 * 
 * This mechanism could have been better implemented using GNU C
 * and its ability to store pointers to labels, but sadly this is
 * not part of the ANSI C standard and so the mechanism is done by
 * case statements instead. That's why you can't put a crReturn()
 * inside a switch() statement.
 */

/*
 * coroutine.h is copyright 1995,2000 Simon Tatham.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#ifndef COROUTINE_H_
#define COROUTINE_H_ 1


typedef struct
{
	int cr_line;
} cr_ctx_t;

#define CR_CTX_INITIALIZER	{ 0 }


#define CR_CTX_INIT(ctx)	do { (ctx)->cr_line = 0; } while (0)


#define CR_BEGIN(ctx)      do {			\
	cr_ctx_t *_x__ = (ctx);		\
	switch (_x__->cr_line) { case 0:


#define CR_RETURN(z)     do {		\
	(_x__)->cr_line = __LINE__;	\
	return z; case __LINE__:	\
} while (0)


#define CR_END(ctx)        } (ctx)->cr_line = -1; } while (0)


#define CR_CANCEL(ctx)     do { (ctx)->cr_line = -1; } while (0)


#endif /* COROUTINE_H_ */

