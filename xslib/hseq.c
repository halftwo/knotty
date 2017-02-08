/*
   The idea is from 
	   Cache Array Routing Protocol v1.0
	   http://icp.ircache.net/carp.txt

   But the original hash function and hash combination function result in
   a quite unbalanced disrubtion. So we changed the hash function for key
   (url) to standard crc32, the hash function for item (member proxy) to
   a kind of crc64, and the hash combination function to a better one. 
*/
#include "xsdef.h"
#include "hseq.h"
#include "carp.h"
#include "crc.h"
#include "crc64.h"
#include "heap.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: hseq.c,v 1.10 2013/03/21 03:10:26 gremlin Exp $";
#endif

struct hseq_t
{
	int total;
	bool weighted;
};

hseq_t *hseq_create(const hseq_bucket_t buckets[], size_t num)
{
	size_t i;
	uint64_t *members = NULL;
	uint32_t *weights = NULL;
	carp_t *carp;

	if ((ssize_t)num <= 0)
		return NULL;

	members = XS_ALLOC(uint64_t, num);
	weights = XS_ALLOC(uint32_t, num);

	for (i = 0; i < num; ++i)
	{
		const hseq_bucket_t *b = &buckets[i];
		members[i] = (b->idlen >= 0) ? crc64_checksum(b->identity, b->idlen)
					: crc64_checksum_cstr((char *)b->identity);
		weights[i] = b->weight;
	}

	carp = carp_create_with_weight(members, weights, num, NULL);
	free(weights);
	free(members);
	return (hseq_t *)carp;
}

void hseq_destroy(hseq_t *hs)
{
	carp_destroy((carp_t *)hs);
}

size_t hseq_total(hseq_t *hs)
{
	return carp_total((carp_t *)hs);
}

int hseq_which(hseq_t *hs, const void *key, size_t len)
{
	uint32_t keyhash = (ssize_t)len >= 0 ? crc32_checksum(key, len) : crc32_checksum_cstr((char *)key);
	return carp_which((carp_t *)hs, keyhash);
}

int hseq_hash_which(hseq_t *hs, uint32_t keyhash)
{
	return carp_which((carp_t *)hs, keyhash);
}


size_t hseq_sequence(hseq_t *hs, const void *key, size_t len, int *seqs, size_t seqs_num)
{
	uint32_t keyhash = (ssize_t)len >= 0 ? crc32_checksum(key, len) : crc32_checksum_cstr((char *)key);
	return carp_sequence((carp_t *)hs, keyhash, seqs, seqs_num);
}

size_t hseq_hash_sequence(hseq_t *hs, uint32_t keyhash, int *seqs, size_t seqs_num)
{
	return carp_sequence((carp_t *)hs, keyhash, seqs, seqs_num);
}

#ifdef TEST_HSEQ

#include <stdio.h>

void get_random_name(char *str, size_t size)
{
	size_t i;
	if (size == 0)
		return;
	--size;
	for (i = 0; i < size; ++i)
		str[i] = 'a' + random() % 26;
	str[i] = 0;
}

#define SIZE    12
#define NUM     256

int main(int argc, char **argv)
{
	int i;
	hseq_bucket_t buckets[NUM];
	int count[NUM] = {0};
	int seq[NUM];
	char buf[SIZE];
	hseq_t *hs;

	for (i = 0; i < NUM; ++i)
	{
		buckets[i].identity = malloc(SIZE);
		buckets[i].idlen = -1;
		buckets[i].weight = i % 2 ? 1 : 2;
		get_random_name((char *)buckets[i].identity, SIZE);
	}

	hs = hseq_create(buckets, NUM);

	get_random_name(buf, sizeof(buf));
	for (i = 0; i < 1024 * 1024; ++i)
	{
		get_random_name(buf, sizeof(buf));
/*		seq[0] = hseq_which(hs, buf, -1); */
		hseq_sequence(hs, buf, -1, seq, 5);
		count[seq[0]]++;
	}

	for (i = 0; i < NUM; ++i)
		printf("%d\t= %d\n", i, count[i]);

	return 0;
}

#endif
