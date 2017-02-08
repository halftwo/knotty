#include "ostk.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: ostk.c,v 1.41 2015/05/21 09:31:38 gremlin Exp $";
#endif


enum
{
	FLAG_CHUNK_EMPTY_OBJECT = 0x01,
	FLAG_FAILED 		= 0x02,
	FLAG_NOT_ALLOCATED	= 0x04,
};


#define ADJUST(os)	(os)->object_base = (os)->current = OSTK_ALIGN((os)->current);


#define FINISH(os, p)			do {				\
	if ((os)->flags & FLAG_FAILED) {				\
		(p) = NULL;						\
		(os)->current = (os)->object_base;			\
		(os)->flags &= ~FLAG_FAILED;				\
	} else {							\
		(p) = BASE(os);						\
		if ((os)->current == (os)->chunk->header_size)		\
			(os)->chunk->flags |= FLAG_CHUNK_EMPTY_OBJECT;	\
		else							\
			ADJUST(os);					\
	}								\
} while (0)


#define ROOM(os)	((os)->chunk->capacity - (os)->current)

#define BASE(os)	((char *)(os)->chunk + (os)->object_base)

#define CURRENT(os)	((char *)(os)->chunk + (os)->current)


static ostk_chunk_alloc_function _chunk_alloc = malloc;
static ostk_chunk_free_function _chunk_free = free;

void ostk_set_chunk_memory_function(ostk_chunk_alloc_function chunk_alloc, ostk_chunk_free_function chunk_free)
{
	_chunk_alloc = chunk_alloc ? chunk_alloc : malloc;
	_chunk_free = chunk_free ? chunk_free : free;
}

static inline size_t _adjust_chunk_size(size_t chunk_size)
{
	if ((ssize_t)chunk_size <= 0)
		chunk_size = OSTK_CHUNK_SIZE_DEFAULT;
	else if (chunk_size < OSTK_CHUNK_SIZE_MIN)
		chunk_size = OSTK_CHUNK_SIZE_MIN;
	else if (chunk_size > OSTK_CHUNK_SIZE_MAX)
		chunk_size = OSTK_CHUNK_SIZE_MAX;

	return OSTK_ALIGN(chunk_size);
}

static inline void _init_ostk(ostk_t *os, size_t ostk_size, size_t chunk_size)
{
	os->chunk = (struct ostk_chunk *)os;
	os->capacity = ostk_size;
	os->header_size = OSTK_ALIGN(sizeof(ostk_t));
	os->flags = 0;
	os->object_base = os->header_size;
	os->current = os->object_base;
	os->new_chunk_size = chunk_size;
	memset(os->_padding, 0xff, OSTK_PADDING_SIZE);
}


ostk_t *ostk_create_on_buffer(void *buffer, size_t buffer_size, size_t chunk_size)
{
	size_t header_size = OSTK_ALIGN(sizeof(ostk_t));
	ostk_t *os = (ostk_t *)buffer;

	assert(buffer_size >= header_size);
	chunk_size = _adjust_chunk_size(chunk_size);
	_init_ostk(os, buffer_size, chunk_size);
	os->flags |= FLAG_NOT_ALLOCATED;
	return os;
}

ostk_t *ostk_create(size_t chunk_size)
{
	ostk_t *os;

	chunk_size = _adjust_chunk_size(chunk_size);
	os = (ostk_t *)_chunk_alloc(chunk_size);
	if (os)
	{
		_init_ostk(os, chunk_size, chunk_size);
	}
	return os;
}

size_t ostk_change_chunk_size(ostk_t *ostk, size_t chunk_size)
{
	if ((ssize_t)chunk_size > 0)
	{
		if (chunk_size < OSTK_CHUNK_SIZE_MIN)
			chunk_size = OSTK_CHUNK_SIZE_MIN;
		else if (chunk_size > OSTK_CHUNK_SIZE_MAX)
			chunk_size = OSTK_CHUNK_SIZE_MAX;

		ostk->new_chunk_size = OSTK_ALIGN(chunk_size);
	}
	return ostk->new_chunk_size - OSTK_ALIGN(sizeof(struct ostk_chunk));
}

void *ostk_hold(ostk_t *os, size_t size)
{
	void *p;
	if (os->chunk != (struct ostk_chunk *)os)
		return NULL;
	if (os->current != os->header_size)
		return NULL;
	if (os->current + size > os->capacity || os->current + size > USHRT_MAX)
		return NULL;

	os->current += size;
	FINISH(os, p);
	os->header_size = os->current;
	if (p && size > 0)
		memset(p, 0, size);
	return p;
}

static inline void _delete_chunks_except_first(ostk_t *os)
{
	struct ostk_chunk *ck, *prev;
	for (ck = os->chunk; ck != (struct ostk_chunk *)os; ck = prev)
	{
		prev = ck->prev;
		_chunk_free(ck);
	}
}

void ostk_destroy(ostk_t *os)
{
	if (os)
	{
		_delete_chunks_except_first(os);
		if (!(os->flags & FLAG_NOT_ALLOCATED))
			_chunk_free(os);
	}
}

static void _do_free(ostk_t *os, void *ptr)
{
	struct ostk_chunk *ck = os->chunk;
	if (ptr > (void *)ck && ptr <= (void *)((char *)ck + ck->capacity))
	{
		assert(ptr >= (void *)((char *)ck + ck->header_size));
		os->current = (char *)ptr - (char *)ck;
		ADJUST(os);
	}
	else 
	{
		if (ck != (struct ostk_chunk *)os)
		{
			do
			{
				struct ostk_chunk *prev = ck->prev;
				_chunk_free(ck);
				ck = prev;
				if (ptr > (void *)ck && ptr <= (void *)((char *)ck + ck->capacity))
				{
					assert(ptr >= (void *)((char *)ck + ck->header_size));
					os->chunk = ck;
					os->current = (char *)ptr - (char *)ck;
					ADJUST(os);
					return;
				}
			} while (ck != (struct ostk_chunk *)os);
		}

		if (ptr)
		{
			assert(!"Free a pointer that is not from any of the chunks!");
		}

		os->chunk = (struct ostk_chunk *)os;
		os->object_base = os->current = os->header_size;
	}
}

void ostk_clear(ostk_t *os)
{
	_do_free(os, NULL);
}

void ostk_free(ostk_t *os, void *ptr)
{
	if (ptr)
		_do_free(os, ptr);
}

inline size_t ostk_memory_used(ostk_t *os)
{
	struct ostk_chunk *ck;
	size_t used = os->capacity;
	for (ck = os->chunk; ck != (struct ostk_chunk *)os; ck = ck->prev)
	{
		used += ck->capacity;
	}

	return used;
}

static bool _newchunk(ostk_t *os, size_t size)
{
	struct ostk_chunk *chunk;
	size_t chunk_size = os->new_chunk_size;
	size_t obj_size = os->current - os->object_base;
	size_t new_size = OSTK_ALIGN(sizeof(struct ostk_chunk) + obj_size + size + (obj_size >> 2) + 16);

	if (chunk_size < new_size)
	{
		chunk_size = new_size;
	}

	if (chunk_size > OSTK_CHUNK_SIZE_MAX)
		return false;

	chunk = (struct ostk_chunk *)_chunk_alloc(chunk_size);
	if (chunk)
	{
		chunk->capacity = chunk_size;
		chunk->header_size = OSTK_ALIGN(sizeof(struct ostk_chunk));
		chunk->flags = 0;

		if (obj_size)
			memcpy((char *)chunk + chunk->header_size, BASE(os), obj_size);

		if (os->object_base == os->chunk->header_size && os->chunk != (struct ostk_chunk *)os
			&& !(os->chunk->flags & FLAG_CHUNK_EMPTY_OBJECT))
		{
			chunk->prev = os->chunk->prev;
			_chunk_free(os->chunk);
		}
		else
			chunk->prev = os->chunk;
		os->chunk = chunk;

		os->object_base = os->chunk->header_size;
		os->current = os->object_base + obj_size;
		return true;
	}

	os->flags |= FLAG_FAILED;
	return false;
}

static inline void *_alloc(ostk_t *os, size_t size)
{
	void *p;
	if (os->current + size > os->chunk->capacity)
	{
		if (!_newchunk(os, size))
			return NULL;
	}
	p = CURRENT(os);
	os->current += size;
	return p;
}

 
inline void *ostk_alloc(ostk_t *os, size_t size)
{
	void *p;
	if (!_alloc(os, size))
		return NULL;
	FINISH(os, p);
	return p;
}

inline void *ostk_calloc(ostk_t *os, size_t size)
{
	void *p;
	if (!_alloc(os, size))
		return NULL;
	FINISH(os, p);
	if (p && size)
		memset(p, 0, size);
	return p;
}

inline ssize_t ostk_object_blank(ostk_t *os, size_t size)
{
	char *p = (char *)_alloc(os, size);
	return p ? p - BASE(os) : -1;
}

inline ssize_t ostk_object_grow(ostk_t *os, const void *src, size_t size)
{
	char *p = (char *)_alloc(os, size);
	if (p)
	{
		memcpy(p, src, size);
		return p - BASE(os);
	}
	return -1;
}

inline ssize_t ostk_object_growz(ostk_t *os, const void *src, size_t size)
{
	char *p = (char *)_alloc(os, size + 1);
	if (p)
	{
		memcpy(p, src, size);
		p[size] = 0;
		return p - BASE(os);
	}
	return -1;
}

inline ssize_t ostk_object_putc(ostk_t *os, char ch)
{
	char *p = (char *)_alloc(os, 1);
	if (p)
	{
		*p = ch;
		return p - BASE(os);
	}
	return -1;
}

inline ssize_t ostk_object_pad(ostk_t *os, char ch, size_t size)
{
	char *p = (char *)_alloc(os, size);
	if (p)
	{
		memset(p, ch, size);
		return p - BASE(os);
	}
	return -1;
}

inline ssize_t ostk_object_puts(ostk_t *os, const char *str)
{
	ssize_t rc = CURRENT(os) - BASE(os);
	int room = ROOM(os);
	for (; room > 0 && *str; --room)
	{
		*CURRENT(os) = *str++;
		os->current++;
	}

	if (room == 0)
	{
		size_t size = strlen(str);
		if (size)
		{
			char *p = (char *)_alloc(os, size);
			if (!p)
				return -1;
			memcpy(p, str, size);
		}
	}

	return rc;
}

inline ssize_t ostk_object_vprintf(ostk_t *os, const char *fmt, va_list ap)
{
	ssize_t rc = CURRENT(os) - BASE(os);
	va_list ap2;
	int need;
	int room = ROOM(os);

	va_copy(ap2, ap);
	need = vsnprintf(CURRENT(os), room, fmt, ap2);
	va_end(ap2);

	if (need < 0)
	{
		os->flags |= FLAG_FAILED;
		return -1;
	}

	if (need <= room)
	{
		os->current += need;
	}
	else
	{
		char *p = (char *)_alloc(os, need);
		if (!p)
			return -1;
		if (vsprintf(p, fmt, ap) != need)
		{
			os->flags |= FLAG_FAILED;
			return -1;
		}
	}

	return rc;
}

inline ssize_t ostk_object_printf(ostk_t *os, const char *fmt, ...)
{
	va_list ap;
	ssize_t rc;
	va_start(ap, fmt);
	rc = ostk_object_vprintf(os, fmt, ap);
	va_end(ap);
	return rc;
}


inline void *ostk_object_finish(ostk_t *os, size_t *psize)
{
	void *p;
	size_t size = ostk_object_size(os);
	FINISH(os, p);

	if (psize)
		*psize = p ? size : 0;
	return p;
}

inline xstr_t ostk_object_finish_xstr(ostk_t *os)
{
	xstr_t xs;
	xs.data = ostk_object_finish(os, (size_t*)&xs.len);
	return xs;
}

void ostk_object_cancel(ostk_t *os)
{
	os->current = os->object_base;
}


inline void *ostk_copy(ostk_t *os, const void *src, size_t size)
{
	void *p;
	ostk_object_grow(os, src, size);
	FINISH(os, p);
	return p;
}

inline void *ostk_copyz(ostk_t *os, const void *src, size_t size)
{
	void *p;
	ostk_object_growz(os, src, size);
	FINISH(os, p);
	return p;
}

inline char *ostk_strdup(ostk_t *os, const char *str)
{
	return (char *)ostk_copyz(os, str, strlen(str));
}

inline char *ostk_strdup_xstr(ostk_t *os, const xstr_t *xs)
{
	return (char *)ostk_copyz(os, xs->data, xs->len);
}

inline xstr_t ostk_xstr_alloc(ostk_t *os, size_t size)
{
	xstr_t xs;
	xs.data = (unsigned char *)ostk_alloc(os, size);
	xs.len = size;
	return xs;
}

inline xstr_t ostk_xstr_calloc(ostk_t *os, size_t size)
{
	xstr_t xs;
	xs.data = (unsigned char *)ostk_calloc(os, size);
	xs.len = size;
	return xs;
}

inline xstr_t ostk_xstr_dup_mem(ostk_t *os, const void *data, size_t size)
{
	xstr_t xs;
	xs.data = (unsigned char *)ostk_copy(os, data, size);
	xs.len = size;
	return xs;
}

inline xstr_t ostk_xstr_dup(ostk_t *os, const xstr_t *str)
{
	return ostk_xstr_dup_mem(os, str->data, str->len);
}

inline xstr_t ostk_xstr_dup_cstr(ostk_t *os, const char *str)
{
	return ostk_xstr_dup_mem(os, str, strlen(str));
}

inline char *ostk_vprintf(ostk_t *os, const char *fmt, va_list ap)
{
	char *p;
	ostk_object_vprintf(os, fmt, ap);
	ostk_object_putc(os, 0);
	FINISH(os, p);
	return p;
}

inline char *ostk_printf(ostk_t *os, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = ostk_vprintf(os, fmt, ap);
	va_end(ap);
	return p;
}

inline xstr_t ostk_xstr_vprintf(ostk_t *os, const char *fmt, va_list ap)
{
	xstr_t xs;
	ostk_object_vprintf(os, fmt, ap);
	xs = ostk_object_finish_xstr(os);
	return xs;
}

inline xstr_t ostk_xstr_printf(ostk_t *os, const char *fmt, ...)
{
	va_list ap;
	xstr_t xs;

	va_start(ap, fmt);
	xs = ostk_xstr_vprintf(os, fmt, ap);
	va_end(ap);
	return xs;
}

const xmem_t ostk_xmem = {
	(xmem_alloc_function)ostk_alloc,
	NULL,
};


#ifdef TEST_OSTK

#include <stdio.h>

static void *my_alloc(size_t size)
{
	return malloc(size);
}

static void my_free(void *ptr)
{
	free(ptr);
}

int main()
{
	int i, size;
	void *first, *second, *third, *empty;
	unsigned char buf[1028];
	ostk_set_chunk_memory_function(my_alloc, my_free);
	ostk_t *os = ostk_create_on_buffer(buf, sizeof(buf), 0);
	printf("%p\n", os);
	size = 400;
	printf("%p\t%d hold\n", ostk_hold(os, size), size);
	printf("%p\t%d\n", ostk_alloc(os, 100), 100);
	printf("%p\t%d hold\n", ostk_hold(os, size), size);
	first = ostk_alloc(os, size);
	printf("%p\t%d\n", first, size);
	second = ostk_alloc(os, size);
	printf("%p\t%d\n", second, size);
	ostk_free(os, second);
	empty = ostk_alloc(os, 0);
	printf("%p\t%d zero-size\n", empty, 0);
	printf("%p\t%d\n", ostk_alloc(os, 6000), 6000);
	ostk_free(os, empty);
	printf("%p\t%d\n", ostk_alloc(os, 6000), 6000);
	for (i = 0; i < 10; ++i)
	{
		int size = random() % 8192;
		void *p = ostk_alloc(os, size);
		printf("%p\t%d\n", p, size);
	}
	ostk_clear(os);
	printf("cleared\n");
	printf("%p\t%d zero-size\n", ostk_alloc(os, 0), 0);
	
	printf("ostk_t size = %zu\n", sizeof(ostk_t));
	printf("ostk_t padding = %zu\n", OSTK_PADDING_SIZE);
	printf("align = %zu\n", OSTK_ALIGN_SIZE);
	return 0;
}

#endif
