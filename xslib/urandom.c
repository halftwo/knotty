#include "urandom.h"
#include "xbase32.h"
#include "xbase57.h"
#include "xnet.h"
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: urandom.c,v 1.3 2013/03/15 15:07:29 gremlin Exp $";
#endif

static int get_random_fd(void)
{
	static int fd = -2;

	if (fd == -2)
	{
		struct timeval tv;
		int i;

		gettimeofday(&tv, 0);
		fd = open("/dev/urandom", O_RDONLY);
		if (fd == -1)
			fd = open("/dev/random", O_RDONLY | O_NONBLOCK);
		srandom((getpid() << 16) ^ getuid() ^ tv.tv_sec ^ tv.tv_usec);

		/* Crank the random number generator a few times */
		gettimeofday(&tv, 0);
		for (i = (tv.tv_sec ^ tv.tv_usec) & 0x1F; i > 0; i--)
			random();
	}
	return fd;
}

bool urandom_has_device()
{
	return (get_random_fd() >= 0);
}

/*
 * Generate a series of random bytes.  Use /dev/urandom if possible,
 * and if not, use srandom/random.
 */
void urandom_get_bytes(void *buf, size_t nbytes)
{
	ssize_t k, n = nbytes;
	int fd = get_random_fd();
	int lose_counter = 0;
	unsigned char *cp = (unsigned char *)buf;

	if (fd >= 0)
	{
		while (n > 0)
		{
			k = read(fd, cp, n);
			if (k <= 0)
			{
				if (lose_counter++ > 8)
				{
					struct timeval tv;
					int i;
					/* Crank the random number generator a few times */
					gettimeofday(&tv, 0);
					for (i = (tv.tv_sec ^ tv.tv_usec) & 0x1F; i > 0; i--)
					{
						random();
					}
					break;
				}
				continue;
			}
			n -= k;
			cp += k;
			lose_counter = 0;
		}
	}
	
	/*
	 * We do this all the time, but this is the only source of
	 * randomness if /dev/random/urandom is out to lunch.
	 */
	for (cp = (unsigned char *)buf, k = 0; k < (ssize_t)nbytes; k++)
		*cp++ ^= (random() >> 7) & 0xFF;
}

int urandom_get_int(int a, int b)
{
	double range = (double)b - a;
	if (range > 0)
	{
		unsigned int n = range * random() / (RAND_MAX + 1.0);
		return a + n;
	}
	else
	{
		unsigned int n = -range * random() / (RAND_MAX + 1.0);
		return b + n;
	}
}

ssize_t urandom_generate_base32id(char id[], size_t size)
{
	uint64_t *x = 0;
	ssize_t i, num, k;
	ssize_t len = size - 1;

	if (len < 0)
		return -1;

	if (len == 0)
	{
		id[0] = 0;
		return 0;
	}

	num = (len + 11) / 12;
	x = alloca(sizeof(x[0]) * num);
	urandom_get_bytes(x, sizeof(x[0]) * num);

	for (k = 0, i = 0; k < len; k += 12, i++)
	{
		ssize_t n = len - k;
		if (n > 12)
			n = 12;
		xbase32_pad_from_uint64(id + k, n, x[i]);
	}
	id[0] = xbase32_alphabet[x[0] / (UINT64_MAX / 22 + 1)]; /* make the first character always letter instead of digit */
	id[len] = 0;
	return len;
}

ssize_t urandom_generate_base57id(char id[], size_t size)
{
	uint64_t *x = 0;
	ssize_t i, num, k;
	ssize_t len = size - 1;

	if (len < 0)
		return -1;

	if (len == 0)
	{
		id[0] = 0;
		return 0;
	}

	num = (len + 9) / 10;
	x = alloca(sizeof(x[0]) * num);
	urandom_get_bytes(x, sizeof(x[0]) * num);

	for (k = 0, i = 0; k < len; k += 10, i++)
	{
		ssize_t n = len - k;
		if (n > 10)
			n = 10;
		xbase57_pad_from_uint64(id + k, n, x[i]);
	}
	id[0] = xbase57_alphabet[x[0] / (UINT64_MAX / 49 + 1)]; /* make the first character always letter instead of digit */
	id[len] = 0;
	return len;
}

