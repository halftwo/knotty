#include "rope.h"
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <inttypes.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: rope.c,v 1.26 2015/05/26 07:40:53 gremlin Exp $";
#endif

#define DFT_BLOCK_SIZE	512
#define ALIGN_MASK	((size_t)(sizeof(intptr_t)-1))

#define USE_EX(rb)	((rb)->buf != (rb)->data)

#define RB_AVAIL(rb)	((rb)->capacity - (rb)->len)
#define RB_CUR(rb)	((rb)->buf + (rb)->len)

enum
{
	BLOCK_EXTERNAL = 0x01,
	BLOCK_EXACT = 0x02,
};

static rope_block_t *rb_alloc(rope_t *rope, size_t bufsize, int flags)
{
	rope_block_t *rb;
	size_t block_size = ROPE_BLOCK_HEAD_SIZE + bufsize;
	bool external = flags & BLOCK_EXTERNAL;

	if (external)
	{
		if (block_size < sizeof(rope_block_t))
			block_size = sizeof(rope_block_t);
	}
	else
	{
		if (rope->block_size < sizeof(rope_block_t))
			rope->block_size = (rope->block_size <= 0) ? DFT_BLOCK_SIZE : sizeof(rope_block_t);

		if (!(flags & BLOCK_EXACT))
		{
			if (block_size < (size_t)rope->block_size)
				block_size = rope->block_size;
			else
				block_size = (block_size + (bufsize >> 4) + 8 + ALIGN_MASK) & ~ALIGN_MASK;
		}
	}

	if (!rope->xmem)
	{
		rope->xmem = &stdc_xmem;
		rope->xm_cookie = NULL;
	}

	rb = (rope_block_t *)rope->xmem->alloc(rope->xm_cookie, block_size);
	if (rb)
	{
		if (external)
		{
			rb->buf = NULL;
			rb->capacity = -1;
			rb->len = -1;
		}
		else
		{
			rb->buf = rb->data;
			rb->capacity = block_size - ROPE_BLOCK_HEAD_SIZE;
			rb->len = 0;
		}
	}
	return rb;
}

static inline rope_block_t *alloc_and_append(rope_t *rope, size_t bufsize, int flags)
{
	rope_block_t *rb = rb_alloc(rope, bufsize, flags);
	if (rb)
	{
		if (rope->_last)
		{
			rb->next = rope->_last->next;
			rope->_last->next = rb;
		}
		else
		{
			rb->next = rb;
		}
		rope->_last = rb;
		rope->block_count++;
	}
	return rb;
}

static inline void _init_external(rope_block_t *rb, const void *buffer, size_t size,
				void (*xfree)(void *, void *), void *xcookie)
{
	rb->buf = (unsigned char *)buffer;
	rb->capacity = size;
	rb->len = size;
	rb->ex.free = xfree;
	rb->ex.cookie = xcookie;
}


void rope_init(rope_t *rope, size_t block_size, const xmem_t *xm, void *xm_cookie)
{
	rope->_last = NULL;
	rope->length = 0;
	rope->block_count = 0;
	rope->block_size = block_size;
	rope->xmem = xm;
	rope->xm_cookie = xm_cookie;
}

void rope_finish(rope_t *rope)
{
	if (rope->_last)
	{
		rope_block_t *rb, *next;
		next = rope->_last->next;
		do
		{
			rb = next;
			next = rb->next;

			if (USE_EX(rb) && rb->ex.free)
				rb->ex.free(rb->ex.cookie, rb->buf);
			if (rope->xmem->free)
				rope->xmem->free(rope->xm_cookie, rb);
		} while (rb != rope->_last);
		rope->_last = NULL;
	}
}

void rope_clear(rope_t *rope)
{
	rope_finish(rope);
	rope->block_count = 0;
	rope->length = 0;
}

rope_block_t *rope_get_first_block(rope_t *rope)
{
	rope_block_t *rb = rope->_last;
	return rb ? rb->next : NULL;
}

void rope_remove_first_block(rope_t *rope)
{
	if (rope->_last)
	{
		rope_block_t *rb = rope->_last->next;
		if (rb == rope->_last)
			rope->_last = NULL;
		else
			rope->_last->next = rb->next;
		
		rope->length -= rb->len;
		rope->block_count--;

		if (USE_EX(rb) && rb->ex.free)
			rb->ex.free(rb->ex.cookie, rb->buf);
		if (rope->xmem->free)
			rope->xmem->free(rope->xm_cookie, rb);
	}
}

ssize_t	rope_write(rope_t *rope, const void *buffer, size_t size)
{
	ssize_t r = size;
	rope_block_t *rb = rope->_last;

	assert(size <= INT_MAX);
	if (rb)
	{
		ssize_t num = RB_AVAIL(rb);
		if (size == 1 && num > 0)
		{
			*RB_CUR(rb) = *(char *)buffer;
			rb->len++;
			rope->length++;
			return 1;
		}

		if ((size_t)num > size)
			num = size;

		if (num > 0)
		{
			memcpy(RB_CUR(rb), buffer, num);
			rb->len += num;
			rope->length += num;

			buffer = (char *)buffer + num;
			size -= num;
		}
	}

	if (size > 0)
	{
		rb = alloc_and_append(rope, size, 0);
		if (!rb)
		{
			return -1;
		}

		memcpy(RB_CUR(rb), buffer, size);
		rb->len += size;
		rope->length += size;
	}

	return r;
}

ssize_t rope_pad(rope_t *rope, char c, size_t size)
{
	ssize_t r = size;
	rope_block_t *rb = rope->_last;

	assert(size <= INT_MAX);
	if (rb)
	{
		ssize_t num = RB_AVAIL(rb);
		if (size == 1 && num > 0)
		{
			*RB_CUR(rb) = c;
			rb->len++;
			rope->length++;
			return 1;
		}

		if ((size_t)num > size)
			num = size;

		if (num > 0)
		{
			memset(RB_CUR(rb), c, num);
			rb->len += num;
			rope->length += num;

			size -= num;
		}
	}

	if (size > 0)
	{
		rb = alloc_and_append(rope, size, 0);
		if (!rb)
		{
			return -1;
		}

		memset(RB_CUR(rb), c, size);
		rb->len += size;
		rope->length += size;
	}

	return r;
}

rope_block_t *rope_append_block(rope_t *rope, const void *buffer, size_t size)
{
	rope_block_t *rb = alloc_and_append(rope, size, BLOCK_EXACT);
	if (rb)
	{
		memcpy(RB_CUR(rb), buffer, size);
		rb->len += size;
		rope->length += size;
	}
	return rb;
}

rope_block_t *rope_append_external(rope_t *rope, const void *buffer, size_t size,
				void (*xfree)(void *, void *), void *xcookie)
{
	rope_block_t *rb = alloc_and_append(rope, 0, BLOCK_EXTERNAL);
	if (rb)
	{
		_init_external(rb, buffer, size, xfree, xcookie);
		rope->length += size;
	}
	return rb;
}

unsigned char *rope_reserve(rope_t *rope, size_t size, bool monolith)
{
	unsigned char *result = NULL;
	rope_block_t *rb = rope->_last;

	assert(size <= INT_MAX);
	if (rb)
	{
		ssize_t num = RB_AVAIL(rb);
		if (size <= (size_t)num)
			goto done;
		else if (!monolith)
		{
			result = RB_CUR(rb);
			rb->len += num;
			rope->length += num;
			size -= num;
		}
	}

	if (size > 0)
	{
		rb = alloc_and_append(rope, size + 8, 0);
		if (!rb)
		{
			return NULL;
		}
	}

done:
	if (!result)
		result = RB_CUR(rb);
	rb->len += size;
	rope->length += size;
	return result;
}

ssize_t	rope_putc(rope_t *rope, char ch)
{
	rope_block_t *rb = rope->_last;

	if (!rb || RB_AVAIL(rb) <= 0)
	{
		rb = alloc_and_append(rope, 1, 0);
		if (!rb)
		{
			return -1;
		}
	}

	rb->buf[rb->len++] = ch;
	rope->length++;
	return 1;

}

ssize_t	rope_puts(rope_t *rope, const char *str)
{
	ssize_t r = 0;
	ssize_t space;
	rope_block_t *rb = rope->_last;

	if (rb && (space = RB_AVAIL(rb)) > 0)
	{
		while (*str && rb->len < rb->capacity)
		{
			rb->buf[rb->len++] = *str++;
		}
		r += space - (rb->capacity - rb->len);
	}

	while (*str)
	{
		rb = alloc_and_append(rope, 1, 0);
		if (!rb)
		{
			rope->length += r;
			return -1;
		}

		space = rb->capacity - rb->len;
		while (*str && rb->len < rb->capacity)
		{
			rb->buf[rb->len++] = *str++;
		}
		r += space - (rb->capacity - rb->len);
	}
	rope->length += r;
	return r;
}

ssize_t rope_join(rope_t *rope, rope_t *joined)
{
	ssize_t rc = joined->length;
	if (!joined->_last)
		return 0;

	if (rope->xmem == joined->xmem && rope->xm_cookie == joined->xm_cookie)
	{
		if (rope->_last)
		{
			rope_block_t *the_first = rope->_last->next;
			rope->_last->next = joined->_last->next;
			joined->_last->next = the_first;
		}
		else
		{
			rope->_last = joined->_last;
		}
		rope->length += joined->length;
		rope->block_count += joined->block_count;

		joined->_last = NULL;
		joined->length = 0;
		joined->block_count = 0;
	}
	else
	{
		rope_block_t *block = NULL;
		unsigned char *buf;
		ssize_t len;
		while (rope_next_block(joined, &block, &buf, &len))
		{
			if (len > 0)
			{
				ssize_t r = rope_write(rope, buf, len);
				if ((size_t)r != len)
					return -1;
			}
		}
	}
	return rc;
}

ssize_t rope_insert(rope_t *rope, size_t pos, const void *buffer, size_t size)
{
	ssize_t top, avail, num2move;
	unsigned char *ptr;
	rope_block_t *current, *next;

	assert(size <= INT_MAX);
	if (pos > rope->length)
		return -1;

	if (size == 0)
		return 0;

	if (pos == rope->length)
		return rope_write(rope, buffer, size);

	assert(rope->_last);

	top = 0;
	next = rope->_last->next;
	do
	{
		current = next;
		next = current->next;
		top += current->len;
		if ((size_t)top >= pos)
			break;
	} while (current != rope->_last);

	assert((size_t)top >= pos);

	avail = RB_AVAIL(current);
	num2move = top - pos;
	ptr = RB_CUR(current) - num2move;
	if (size <= (size_t)avail)
	{
		if (num2move > 0)
			memmove(ptr + size, ptr, num2move);
		memcpy(ptr, buffer, size);
		current->len += size;
	}
	else
	{
		ssize_t more = size - avail;
		rope_block_t *rb = rb_alloc(rope, more, 0);
		if (!rb)
			return -1;

		if (more < num2move)
		{
			size_t k = num2move - more;
			memcpy(rb->buf, RB_CUR(current) - more , more);
			memmove(current->buf + current->capacity - k, ptr, k);
			memcpy(ptr, buffer, size);
		}
		else
		{
			size_t k = more - num2move;
			if (num2move > 0)
				memcpy(rb->buf + k, ptr, num2move);
			if (k > 0)
				memcpy(rb->buf, buffer + size - k, k);
			memcpy(ptr, buffer, size - k);
		}
		current->len = current->capacity;
		rb->len = more;

		current->next = rb;
		rb->next = next;
		if (current == rope->_last)
			rope->_last = rb;
		rope->block_count++;
	}
	rope->length += size;

	return size;
}

rope_block_t *rope_insert_external(rope_t *rope, size_t pos, const void *buffer, size_t size,
			void (*xfree)(void *, void *), void *xcookie)
{
	ssize_t top;
	rope_block_t *current, *next;

	if (pos >= rope->length)
		return rope_append_external(rope, buffer, size, xfree, xcookie);

	assert(rope->_last);

	top = 0;
	next = rope->_last->next;
	do
	{
		current = next;
		next = current->next;
		top += current->len;
		if ((size_t)top >= pos)
			break;
	} while (current != rope->_last);

	if ((size_t)top >= pos)
	{
		ssize_t num = top - pos;
		rope_block_t *rb = rb_alloc(rope, 0, BLOCK_EXTERNAL);
		if (!rb)
			return NULL;

		_init_external(rb, buffer, size, xfree, xcookie);
		current->next = rb;
		rb->next = next;
		if (current == rope->_last)
			rope->_last = rb;
		rope->block_count++;
		rope->length += size;

		if (num)
		{
			rope_block_t *extra = rb_alloc(rope, num, 0);
			if (!extra)
				return NULL;

			current->len -= num;
			memcpy(extra->buf, RB_CUR(current), num);
			extra->len = num;

			rb->next = extra;
			extra->next = next;
			if (rb == rope->_last)
				rope->_last = extra;
			rope->block_count++;
		}
		return rb;
	}
	else
	{
		assert(!"Can't reach here!");
	}

	return NULL;
}


ssize_t rope_replace(rope_t *rope, size_t pos, const void *buffer, size_t size)
{
	ssize_t r;
	ssize_t top;
	rope_block_t *current, *next;

	if (pos > rope->length || pos + size > rope->length)
		return -1;

	if (size == 0)
		return 0;

	r = size;
	top = 0;
	next = rope->_last->next;
	do
	{
		current = next;
		next = current->next;
		top += current->len;
		if ((size_t)top >= pos)
			break;
	} while (current != rope->_last);

	if ((size_t)top >= pos)
	{
		ssize_t num = top - pos;
		if ((size_t)num > size)
			num = size;
		memcpy(RB_CUR(current) - num, buffer, num);
		buffer = (char *)buffer + num;
		size -= num;

		while (size > 0)
		{
			current = next;
			next = current->next;
			if (size <= current->len)
			{
				memcpy(current->buf, buffer, size);
				break;
			}

			memcpy(current->buf, buffer, current->len);
			buffer = (char *)buffer + current->len;
			size -= current->len;
		}
	}
	else
	{
		assert(!"Can't reach here!");
	}

	return r;
}

ssize_t rope_erase(rope_t *rope, size_t pos, size_t size)
{
	ssize_t r;
	ssize_t top;
	rope_block_t *current, *next;

	if (pos > rope->length)
		return -1;

	if (size == 0 || pos == rope->length)
		return 0;

	if (pos == 0 && size >= rope->length)
	{
		size = rope->length;
		rope_clear(rope);
		return size;
	}

	assert(rope->_last);

	if (size > rope->length - pos)
		size = rope->length - pos;

	r = size;
	top = 0;
	next = rope->_last->next;
	do
	{
		current = next;
		next = current->next;
		top += current->len;
		if ((size_t)top >= pos)
			break;
	} while (current != rope->_last);
	
	if ((size_t)top >= pos)
	{
		ssize_t num = top - pos;
		if ((size_t)num >= size)
		{
			if ((size_t)num > size)
			{
				unsigned char *ptr = RB_CUR(current) - num;
				memmove(ptr, ptr + size, num - size);
			}
			current->len -= size;
		}
		else
		{
			rope_block_t *prev;

			current->len -= num;
			size -= num;
			prev = current;
			while (size > 0)
			{
				current = next;
				next = current->next;
				if (size < current->len)
				{
					memmove(current->buf, current->buf + size, current->len - size);
					current->len -= size;
					break;
				}

				prev->next = next;
				if (USE_EX(current) && current->ex.free)
					current->ex.free(current->ex.cookie, current->buf);
				if (rope->xmem->free)
					rope->xmem->free(rope->xm_cookie, current);
				rope->block_count--;
				size -= current->len;

				if (current == rope->_last)
				{
					assert(size == 0);
					rope->_last = prev;
				}
			}
		}
	}
	else
	{
		assert(!"Can't reach here!");
	}

	rope->length -= r;
	return r;
}

bool rope_next_block(const rope_t *rope, rope_block_t **p_block, unsigned char **p_buf, ssize_t *p_size)
{
	rope_block_t *rb = *p_block;
	if (rb == rope->_last)
	{
		*p_block = NULL;
		return false;
	}

	if (rb == NULL)
	{
		if (rope->_last == NULL)
			return false;
		rb = rope->_last;
	}

	rb = rb->next;
	if (p_buf)
		*p_buf = rb->buf;
	if (p_size)
		*p_size = rb->len;

	*p_block = rb;
	return true;
}

void rope_copy_to(const rope_t *rope, unsigned char buf[])
{
	if (rope->_last)
	{
		size_t pos = 0;
		rope_block_t *rb, *next;
		next = rope->_last->next;
		do
		{
			rb = next;
			next = rb->next;
			memcpy(buf + pos, rb->buf, rb->len);
			pos += rb->len;
		} while (rb != rope->_last);
	}
}

int rope_dump(const rope_t *rope, xio_write_function xio_write, void *xio_cookie)
{
	if (rope->_last)
	{
		rope_block_t *rb, *next;
		next = rope->_last->next;
		do
		{
			rb = next;
			next = rb->next;
			if (rb->len > 0)
			{
				if (xio_write(xio_cookie, rb->buf, rb->len) != rb->len)
					return -1;
			}
		} while (rb != rope->_last);
	}
	return 0;
}

void rope_iovec(const rope_t *rope, struct iovec iov[])
{
	if (rope->_last)
	{
		size_t i = 0;
		rope_block_t *rb, *next;
		next = rope->_last->next;
		do
		{
			rb = next;
			next = rb->next;
			iov[i].iov_base = rb->buf;
			iov[i].iov_len = rb->len;
			++i;
		} while (rb != rope->_last);
	}
}


const xio_t rope_xio = {
	NULL,
	(xio_write_function) rope_write,
	NULL,
	NULL,
};


#ifdef TEST_ROPE

int main()
{
	const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
	const char *ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	rope_t rp;
	struct iovec iov[100];

	rope_init(&rp, 0, NULL, NULL);
	rope_strcat(&rp, alphabet);
	rope_strcat(&rp, alphabet);
	rope_strcat(&rp, alphabet);
	rope_insert(&rp, 16, ALPHABET, 7);
	rope_erase(&rp, 16, 7);
	rope_erase(&rp, 15, 87);
	rope_iovec(&rp, iov);
	rope_finish(&rp);
	return 0;
}

#endif
