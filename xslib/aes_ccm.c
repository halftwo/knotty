#include "aes_ccm.h"
#include "xnet.h"
#include <assert.h>
#include <string.h>

/*
 * see: RFC3610
 */

void aes_ccm_config(aes_ccm_t *ac, const rijndael_context *ctx, size_t mac_size, size_t counter_size)
{
	assert(mac_size >= 4 && mac_size <= 16 && mac_size % 2 == 0);
	assert(counter_size >= 2 && counter_size <= 8);

	ac->ctx = ctx;
	ac->mac_size = mac_size;
	ac->counter_size = counter_size;
}


static inline void xor_block(uint8_t *dst, const uint8_t *src)
{
	uint32_t *d = (uint32_t *)dst;
	uint32_t *s = (uint32_t *)src;
	*d++ ^= *s++;
	*d++ ^= *s++;
	*d++ ^= *s++;
	*d++ ^= *s++;
}

static inline void xor_inplace(uint8_t *d, const uint8_t *s, size_t n)
{
	assert(n <= 16);
	switch (n)
	{
		case 16: *d++ ^= *s++;
		case 15: *d++ ^= *s++;
		case 14: *d++ ^= *s++;
		case 13: *d++ ^= *s++;
		case 12: *d++ ^= *s++;
		case 11: *d++ ^= *s++;
		case 10: *d++ ^= *s++;
		case  9: *d++ ^= *s++;
		case  8: *d++ ^= *s++;
		case  7: *d++ ^= *s++;
		case  6: *d++ ^= *s++;
		case  5: *d++ ^= *s++;
		case  4: *d++ ^= *s++;
		case  3: *d++ ^= *s++;
		case  2: *d++ ^= *s++;
		case  1: *d++ ^= *s++;
	}
}

static inline void xor_2string(uint8_t *d, const uint8_t *s1, const uint8_t *s2, size_t n)
{
	assert(n <= 16);
	switch (n)
	{
		case 16: *d++ = *s1++ ^ *s2++;
		case 15: *d++ = *s1++ ^ *s2++;
		case 14: *d++ = *s1++ ^ *s2++;
		case 13: *d++ = *s1++ ^ *s2++;
		case 12: *d++ = *s1++ ^ *s2++;
		case 11: *d++ = *s1++ ^ *s2++;
		case 10: *d++ = *s1++ ^ *s2++;
		case  9: *d++ = *s1++ ^ *s2++;
		case  8: *d++ = *s1++ ^ *s2++;
		case  7: *d++ = *s1++ ^ *s2++;
		case  6: *d++ = *s1++ ^ *s2++;
		case  5: *d++ = *s1++ ^ *s2++;
		case  4: *d++ = *s1++ ^ *s2++;
		case  3: *d++ = *s1++ ^ *s2++;
		case  2: *d++ = *s1++ ^ *s2++;
		case  1: *d++ = *s1++ ^ *s2++;
	}
}

static inline void encrypt_stream_block(aes_ccm_t *ac, ssize_t pos)
{
	size_t i;
	uint8_t *p;
	size_t counter = pos < 0 ? 0 : (pos / 16 + 1);
	for (i = ac->counter_size, p = &ac->A[15]; i-- > 0; --p)
	{
		*p = counter & 0xff;
		counter >>= 8;
	}
	rijndael_encrypt(ac->ctx, ac->A, ac->S);
}

/* length of nonce is (15 - counter_size)
 */
void aes_ccm_start(aes_ccm_t *ac, bool encrypt, const uint8_t *nonce, const uint8_t *aad, size_t aad_len, size_t msg_len)
{
	uint8_t flags;
	size_t i;
	uint8_t *p;

	assert(aad_len < 0xFF00);

	ac->encrypt = encrypt;
	ac->total = msg_len;
	ac->pos = 0;

	flags = aad_len ? 0x40 : 0;
	flags |= (((ac->mac_size - 2) / 2) << 3);
	flags |= (ac->counter_size - 1);

	ac->X[0] = flags;
	memcpy(&ac->X[1], nonce, 15 - ac->counter_size);
	for (i = ac->counter_size, p = &ac->X[15]; i-- > 0; --p)
	{
		*p = msg_len & 0xff;
		msg_len >>= 8;
	}
	assert(msg_len == 0);

	/* X_1 = E(K, B_0) */
	rijndael_encrypt(ac->ctx, ac->X, ac->X); 

	if (aad_len)
	{
		uint8_t buf[16];
		buf[0] = (aad_len >> 8);
		buf[1] = aad_len;
		i = aad_len < 14 ? aad_len : 14;
		memcpy(&buf[2], aad, i);
		aad += i;
		aad_len -= i;

		/* X_2 = E(K, X_1 XOR B_1) */
		xor_inplace(ac->X, buf, 2 + i);
		rijndael_encrypt(ac->ctx, ac->X, ac->X);

		while (aad_len >= 16)
		{
			/* X_i+1 := E( K, X_i XOR B_i )  for i=1, ..., n */
			xor_inplace(ac->X, aad, 16);
			rijndael_encrypt(ac->ctx, ac->X, ac->X);

			aad += 16;
			aad_len -= 16;
		}

		if (aad_len > 0)
		{
			/* X_i+1 := E( K, X_i XOR B_i )  for i=1, ..., n */
			xor_inplace(ac->X, aad, aad_len);
			rijndael_encrypt(ac->ctx, ac->X, ac->X);
		}
	}

	memset(ac->A, 0, sizeof(ac->A));
	ac->A[0] = ac->counter_size - 1;
	memcpy(&ac->A[1], nonce, 15 - ac->counter_size);
}

ssize_t aes_ccm_update(aes_ccm_t *ac, const uint8_t *in, uint8_t *out, size_t length)
{
	ssize_t k = ac->pos % 16;
	ssize_t len = length;
	const uint8_t **auth_ptr = (ac->encrypt) ? &in : (const uint8_t **)&out;

	if (ac->pos + len > ac->total)
		return -1;

	if (k)
	{
		ssize_t n = 16 - k;
		if (len < n)
		{
			xor_2string(out, ac->S + k, in, len);
			xor_inplace(ac->X + k, *auth_ptr, len);
			ac->pos += len;
			return len;
		}

		xor_2string(out, ac->S + k, in, n);
		/* X_i+1 := E( K, X_i XOR B_i ) */
		xor_inplace(ac->X + k, *auth_ptr, n);
		rijndael_encrypt(ac->ctx, ac->X, ac->X);

		ac->pos += n;
		in += n;
		out += n;
		len -= n;
	}

	while (len >= 16)
	{
		/* S_i := E( K, A_i ) */
		encrypt_stream_block(ac, ac->pos);
		xor_2string(out, ac->S, in, 16);
		/* X_i+1 := E( K, X_i XOR B_i ) */
		xor_inplace(ac->X, *auth_ptr, 16);
		rijndael_encrypt(ac->ctx, ac->X, ac->X);

		ac->pos += 16;
		in += 16;
		out += 16;
		len -= 16;
	}

	if (len > 0)
	{
		/* S_i := E( K, A_i ) */
		encrypt_stream_block(ac, ac->pos);
		xor_2string(out, ac->S, in, len);
		xor_inplace(ac->X, *auth_ptr, len);
		ac->pos += len;
	}

	return length;
}


ssize_t aes_ccm_finish(aes_ccm_t *ac, uint8_t *mac)
{
	if (ac->pos == ac->total)
	{
		if (ac->pos % 16)
			rijndael_encrypt(ac->ctx, ac->X, ac->X);

		/* S_0 := E( K, A_0 ) */
		encrypt_stream_block(ac, -1);
	}
	else if (ac->pos != -1)
	{
		return -1;
	}

	xor_2string(mac, ac->X, ac->S, ac->mac_size);
	ac->pos = -1;
	return ac->mac_size;
}

#ifdef TEST_AES_CCM

#include "hex.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	aes_ccm_t ac;
	rijndael_context ctx;
	int i;

	char *key_hex = "C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF";
	char *nonce_hex =  "00 00 00 03  02 01 00 A0  A1 A2 A3 A4  A5";
	char *packet_hex = "00 01 02 03  04 05 06 07  08 09 0A 0B  0C 0D 0E 0F"\
              		"10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E";

	uint8_t key[16];
	uint8_t nonce[13];
	uint8_t packet[64];
	uint8_t cipher[64];
	uint8_t out[64];
	uint8_t mac[8];

	char buf[256];
	
	unhexlify_ignore_space(key, key_hex, -1);
	unhexlify_ignore_space(nonce, nonce_hex, -1);
	unhexlify_ignore_space(packet, packet_hex, -1);

	rijndael_setup_encrypt(&ctx, key, sizeof(key));
	
	aes_ccm_config(&ac, &ctx, 8, 2);
	aes_ccm_start(&ac, true, nonce, packet, 8, 31 - 8);

	memcpy(cipher, packet, 8);
//	aes_ccm_update(&ac, packet+8, cipher+8, 31-8);
	for (i = 8; i < 31; ++i)
		aes_ccm_update(&ac, packet+i, cipher+i, 1);
	
	aes_ccm_finish(&ac, mac);
	memcpy(cipher+31, mac, 8);

	hexlify_upper(buf, cipher, 39);
	printf("%s\n", buf);

	aes_ccm_start(&ac, false, nonce, packet, 8, 31 - 8);
	memcpy(out, cipher, 8);
//	aes_ccm_update(&ac, cipher+8, out+8, 31-8);
	for (i = 8; i < 31; ++i)
		aes_ccm_update(&ac, cipher+i, out+i, 1);
	aes_ccm_finish(&ac, mac);

	if (memcmp(mac, cipher+31, 8) != 0)
	{
		fprintf(stderr, "mac not match\n");
		exit(1);
	}
	if (memcmp(out, packet, 31) != 0)
	{
		fprintf(stderr, "packet not match\n");
		exit(1);
	}

	fprintf(stderr, "encrypt and descrypt success!\n");

	/* Test performance */
	aes_ccm_config(&ac, &ctx, 8, 4);
	aes_ccm_start(&ac, true, nonce, packet, 8, 32*1024*1024);
	for (i = 0; i < 1024*1024; ++i)
		aes_ccm_update(&ac, packet, cipher, 32);
	aes_ccm_finish(&ac, mac);

	return 0;
}

#endif
