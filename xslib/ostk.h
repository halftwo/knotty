/* $Id: ostk.h,v 1.50 2015/05/18 07:12:50 gremlin Exp $ */
#ifndef OSTK_H_
#define OSTK_H_

#include "xsdef.h"
#include "xmem.h"
#include "xstr.h"
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSTK_CHUNK_SIZE_DEFAULT	4096
#define OSTK_CHUNK_SIZE_MIN	256
#define OSTK_CHUNK_SIZE_MAX	(1024*1024*1024)

#define OSTK_ALIGN(X)		(((X) + OSTK_ALIGN_SIZE - 1) & ~(OSTK_ALIGN_SIZE - 1))

#define OSTK_ALIGN_SIZE		sizeof(intptr_t)

#define OSTK_PADDING_SIZE	(OSTK_ALIGN_SIZE - sizeof(unsigned int))


#define OSTK_ALLOC(OSTK, TYPE, N)		((TYPE*)ostk_alloc((OSTK), (N)*sizeof(TYPE)))
#define OSTK_CALLOC(OSTK, TYPE, N)		((TYPE*)ostk_calloc((OSTK), (N)*sizeof(TYPE)))

#define OSTK_ALLOC_ONE(OSTK, TYPE)		OSTK_ALLOC(OSTK, TYPE, 1)
#define OSTK_CALLOC_ONE(OSTK, TYPE)		OSTK_CALLOC(OSTK, TYPE, 1)


extern const xmem_t ostk_xmem;


struct ostk_chunk
{
	struct ostk_chunk *prev;
	unsigned int   capacity;
	unsigned short header_size;
	unsigned short flags;

	char _data[];
};

typedef struct ostk_t ostk_t;
struct ostk_t
{
	struct ostk_chunk *chunk;
	unsigned int   capacity;
	unsigned short header_size;
	unsigned short flags;
	
	/* NB: The fields before this line must be the same of struct ostk_chunk. */

	unsigned int object_base;
	unsigned int current;

 	/* Before this line, the structure fields are attributes of current chunk. */
 	/* ---------------------------------------------------------- */
 	/* The following are attributes of the ostk itself. */

	unsigned int new_chunk_size;
	char _padding[OSTK_PADDING_SIZE];
};


typedef void *(*ostk_chunk_alloc_function)(size_t size);
typedef void (*ostk_chunk_free_function)(void *ptr);

void ostk_set_chunk_memory_function(ostk_chunk_alloc_function chunk_alloc, ostk_chunk_free_function chunk_free);


/* The ostk_t object is initialized on the buffer.
 * The ostk_destroy() will NOT free the ostk_t object itself. 
 * You should take care of the buffer yourself.
 */
ostk_t *ostk_create_on_buffer(void *buffer, size_t buffer_size, size_t chunk_size);

/* The ostk_t object itself is also allocated on the heap.
 * The ostk_destroy() will free the ostk_t object itself.
 */
ostk_t *ostk_create(size_t chunk_size);

void ostk_destroy(ostk_t *ostk);


/* Change the size of the will-be-allocated chunks.
 * Return the max possible object size in the new chunks.
 * if size == 0, the ostk->new_chunk_size is not changed.
 */
size_t ostk_change_chunk_size(ostk_t *ostk, size_t size);


/* Hold some memory before any ostk_alloc()ing actions.
   The holded memory can't be ostk_free()ed, 
   and will not be freed when ostk_clear()ing.
   If some memory already ostk_alloc()ed, ostk_hold() will fail.
   If the size of the ostk_hold()ed memory is larger than
   ostk->capacity - sizeof(ostk_t), the function will fail.
 */
void *ostk_hold(ostk_t *ostk, size_t size);

/* Will not free the ostk_hold()ed memory. */
void ostk_clear(ostk_t *ostk);


void *ostk_alloc(ostk_t *ostk, size_t size);

void *ostk_calloc(ostk_t *ostk, size_t size);

/* If ptr is NULL, do nothing */
void ostk_free(ostk_t *ostk, void *ptr);

size_t ostk_memory_used(ostk_t *ostk);


static inline size_t ostk_room(ostk_t *ostk)
{
	return ostk->chunk->capacity - ostk->current;
}

static inline void *ostk_current(ostk_t *ostk)
{
	return (char *)ostk->chunk + ostk->current;
}

static inline void *ostk_object_base(ostk_t *ostk)
{
	return (char *)ostk->chunk + ostk->object_base;
}

static inline size_t ostk_object_size(ostk_t *ostk)
{
	return ostk->current - ostk->object_base;
}



void *ostk_copy(ostk_t *ostk, const void *src, size_t size);

void *ostk_copyz(ostk_t *ostk, const void *src, size_t size);

char *ostk_strdup(ostk_t *ostk, const char *str);

char *ostk_strdup_xstr(ostk_t *ostk, const xstr_t *xs);

char *ostk_vprintf(ostk_t *ostk, const char *fmt, va_list ap) XS_C_PRINTF(2, 0);

char *ostk_printf(ostk_t *ostk, const char *fmt, ...) XS_C_PRINTF(2, 3);



xstr_t ostk_xstr_alloc(ostk_t *ostk, size_t size);

xstr_t ostk_xstr_calloc(ostk_t *ostk, size_t size);

xstr_t ostk_xstr_dup(ostk_t *ostk, const xstr_t *str);

xstr_t ostk_xstr_dup_cstr(ostk_t *ostk, const char *str);

xstr_t ostk_xstr_dup_mem(ostk_t *ostk, const void *data, size_t size);

xstr_t ostk_xstr_vprintf(ostk_t *ostk, const char *fmt, va_list ap) XS_C_PRINTF(2, 0);

xstr_t ostk_xstr_printf(ostk_t *ostk, const char *fmt, ...) XS_C_PRINTF(2, 3);



/* The ostk_object_xxx() functions return the first byte's offset 
   of the allocated memory from the ostk_object_base() of the object,
   or -1 if error.
 */

/* The allocated memory NOT initialized. */
ssize_t ostk_object_blank(ostk_t *ostk, size_t size);

ssize_t ostk_object_grow(ostk_t *ostk, const void *src, size_t size);

ssize_t ostk_object_growz(ostk_t *ostk, const void *src, size_t size);

ssize_t ostk_object_putc(ostk_t *ostk, char ch);

ssize_t ostk_object_pad(ostk_t *ostk, char ch, size_t size);

/* Do NOT append '\0' character. */
ssize_t ostk_object_puts(ostk_t *ostk, const char *str);

/* Do NOT append '\0' character. */
ssize_t ostk_object_vprintf(ostk_t *ostk, const char *fmt, va_list ap) XS_C_PRINTF(2, 0);

/* Do NOT append '\0' character. */
ssize_t ostk_object_printf(ostk_t *ostk, const char *fmt, ...) XS_C_PRINTF(2, 3);


/* One of following functions should always be called
   after the above ostk_object_xxx() functions.
 */
void *ostk_object_finish(ostk_t *ostk, size_t *size/*NULL*/);

xstr_t ostk_object_finish_xstr(ostk_t *ostk);

void ostk_object_cancel(ostk_t *ostk);


#ifdef __cplusplus
}
#endif


#endif
