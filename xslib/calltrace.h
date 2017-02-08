/* $Id: calltrace.h,v 1.2 2010/03/25 07:51:12 jiagui Exp $ */
#ifndef CALLTRACE_H_
#define CALLTRACE_H_

#include "iobuf.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


ssize_t calltrace(int level, char *buf, size_t size);

ssize_t calltrace_iobuf(int level, iobuf_t *ob);


#ifdef __cplusplus
}
#endif

#endif
