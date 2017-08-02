/* $Id: rope.h,v 1.23 2015/05/26 07:40:53 gremlin Exp $ */
#ifndef ROPE_H_
#define ROPE_H_

#include "xmem.h"
#include "xio.h"
#include <sys/uio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


extern const xio_t rope_xio;


typedef struct rope_block_t 	rope_block_t;
typedef struct rope_t 		rope_t;


#define ROPE_BLOCK_HEAD_SIZE	offsetof(rope_block_t, data)

struct rope_block_t
{
	rope_block_t *next;
	unsigned char *buf;		/* if buf != data, it's extenral buffer */
	int len;
	int capacity;

	union
	{
		unsigned char data[0];
		struct
		{
			void (*free)(void *cookie, void *buf);
			void *cookie;
		} ex;
	};
};

struct rope_t
{
	rope_block_t *_last;
	size_t length;
	int block_count;
	int block_size;
	const xmem_t *xmem;
	void *xm_cookie;
};


#define ROPE_INIT(block_size, xm, xm_cookie)	{ NULL, 0, 0, (block_size), (xm), (xm_cookie) }

void rope_init(rope_t *rope, size_t block_size, const xmem_t *xm, void *xm_cookie);
void rope_finish(rope_t *rope);
void rope_clear(rope_t *rope);

unsigned char *rope_reserve(rope_t *rope, size_t size, bool monolith);


ssize_t rope_write(rope_t *rope, const void *buffer, size_t size);
ssize_t rope_puts(rope_t *rope, const char *str);
ssize_t rope_putc(rope_t *rope, char ch);
ssize_t rope_pad(rope_t *rope, char c, size_t n);


ssize_t rope_insert(rope_t *rope, size_t pos, const void *buffer, size_t size);
ssize_t rope_replace(rope_t *rope, size_t pos, const void *buffer, size_t size);
ssize_t rope_erase(rope_t *rope, size_t pos, size_t size);


rope_block_t *rope_get_first_block(rope_t *rope);
void rope_remove_first_block(rope_t *rope);


rope_block_t *rope_append_block(rope_t *rope, const void *buffer, size_t size);

rope_block_t *rope_append_external(rope_t *rope, const void *buffer, size_t size,
				void (*xfree)(void *xcookie, void *), void *xcookie);

rope_block_t *rope_insert_external(rope_t *rope, size_t pos, const void *buffer, size_t size,
				void (*xfree)(void *xcookie, void *), void *xcookie);


ssize_t rope_join(rope_t *rope, rope_t *joined);


ssize_t rope_find(const rope_t *rope, ssize_t pos, const void *needle, size_t size);

size_t rope_substr_copy_cstr(const rope_t *rope, ssize_t pos, void *out, size_t n);

size_t rope_substr_copy_mem(const rope_t *rope, ssize_t pos, void *out, size_t n);


bool rope_next_block(const rope_t *rope, rope_block_t **pblock, unsigned char **pbuf, ssize_t *psize);

void rope_copy_to(const rope_t *rope, unsigned char *buf);

void rope_iovec(const rope_t *rope, struct iovec iov[]);

int rope_dump(const rope_t *rope, xio_write_function xio_write, void *xio_cookie);


#ifdef __cplusplus
}
#endif

#endif
