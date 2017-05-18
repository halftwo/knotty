#include "aes_eax.h"
#include "cmac_aes.h"
#include <string.h>

#define BLOCK_SIZE	16

static inline void counter_increase(uint8_t *counter)
{
	int i;
	for (i = BLOCK_SIZE - 1; i >= 0; --i)
	{
		++counter[i];
		if (counter[i])
			break;
	}
}

void aes_eax_start(aes_eax_context *ax, const rijndael_context *aes, bool encrypt, 
		const uint8_t *nonce, size_t nonce_len, const uint8_t *header, size_t header_len)
{
	uint8_t block[BLOCK_SIZE];

	ax->encrypt = encrypt;
	ax->pos = 0;

	memset(block, 0, sizeof(block));

	block[BLOCK_SIZE - 1] = 0;
	cmac_aes_start(&ax->cmac, aes);
	cmac_aes_update(&ax->cmac, block, sizeof(block));
	cmac_aes_update(&ax->cmac, nonce, nonce_len);
	cmac_aes_finish(&ax->cmac, ax->N);

	block[BLOCK_SIZE - 1] = 1;
	cmac_aes_start(&ax->cmac, aes);
	cmac_aes_update(&ax->cmac, block, sizeof(block));
	cmac_aes_update(&ax->cmac, header, header_len);
	cmac_aes_finish(&ax->cmac, ax->H);

	block[BLOCK_SIZE - 1] = 2;
	cmac_aes_start(&ax->cmac, aes);
	cmac_aes_update(&ax->cmac, block, sizeof(block));

	memcpy(ax->C, ax->N, sizeof(ax->N));
}

static inline void encrypt_stream_block(aes_eax_context *ax)
{
	rijndael_encrypt(ax->cmac.aes, ax->C, ax->S);
	counter_increase(ax->C);
}

static inline void xor_2block(uint8_t *d, const uint8_t *s1, const uint8_t *s2)
{
	*d++ = *s1++ ^ *s2++; 		*d++ = *s1++ ^ *s2++;
	*d++ = *s1++ ^ *s2++; 		*d++ = *s1++ ^ *s2++;
	*d++ = *s1++ ^ *s2++; 		*d++ = *s1++ ^ *s2++;
	*d++ = *s1++ ^ *s2++; 		*d++ = *s1++ ^ *s2++;

	*d++ = *s1++ ^ *s2++; 		*d++ = *s1++ ^ *s2++;
	*d++ = *s1++ ^ *s2++; 		*d++ = *s1++ ^ *s2++;
	*d++ = *s1++ ^ *s2++; 		*d++ = *s1++ ^ *s2++;
	*d++ = *s1++ ^ *s2++; 		*d++ = *s1++ ^ *s2++;
}

void aes_eax_update(aes_eax_context *ax, const uint8_t *input, uint8_t *output, size_t length)
{
	const uint8_t *in = input;
	uint8_t *out = output;
	ssize_t len = length;

	if (!ax->encrypt)
		cmac_aes_update(&ax->cmac, input, length);

	if (ax->pos)
	{
		ssize_t n;
		for (n = 0; n < len && ax->pos < BLOCK_SIZE; ++n)
		{
			*out++ = ax->S[ax->pos++] ^ *in++;
		}
		len -= n;

		if (ax->pos == BLOCK_SIZE)
		{
			ax->pos = 0;
		}
	}

	while (len >= BLOCK_SIZE)
	{
		encrypt_stream_block(ax);
		xor_2block(out, ax->S, in);
		in += BLOCK_SIZE;
		out += BLOCK_SIZE;
		len -= BLOCK_SIZE;
	}

	if (len > 0)
	{
		encrypt_stream_block(ax);
		for (; ax->pos < len; )
		{
			*out++ = ax->S[ax->pos++] ^ *in++;
		}
	}

	if (ax->encrypt)
		cmac_aes_update(&ax->cmac, output, length);
}

void aes_eax_finish(aes_eax_context *ax, uint8_t *mac)
{
	int i;

	cmac_aes_finish(&ax->cmac, mac);

	for (i = 0; i < BLOCK_SIZE; ++i)
	{
		mac[i] ^= (ax->N[i] ^ ax->H[i]);
	}
}


#ifdef TEST_AES_EAX

#include "hex.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	rijndael_context aes;
	aes_eax_context ax;
	int i;

	char *key_hex 	 = "8395FCF1E95BEBD697BD010BC766AAC3";
	char *nonce_hex  = "22E7ADD93CFC6393C57EC0B3C17D6B44";
	char *header_hex = "126735FCC320D25A";
	char *msg_hex 	 = "CA40D7446E545FFAED3BD12A740A659FFBBB3CEAB7";
	char *cipher_hex = "CB8920F87A6C75CFF39627B56E3ED197C552D295A7CFC46AFC253B4652B1AF3795B124AB6E";

	uint8_t key[16];
	uint8_t nonce[16];
	uint8_t header[8];
	uint8_t msg[21];
	uint8_t cipher[37];
	uint8_t out[128];

	unhexlify_ignore_space(key, key_hex, -1);
	unhexlify_ignore_space(nonce, nonce_hex, -1);
	unhexlify_ignore_space(header, header_hex, -1);
	unhexlify_ignore_space(msg, msg_hex, -1);
	unhexlify_ignore_space(cipher, cipher_hex, -1);

	rijndael_setup_encrypt(&aes, key, sizeof(key));
	aes_eax_start(&ax, &aes, true, nonce, sizeof(nonce), header, sizeof(header));
	aes_eax_update(&ax, msg, out, sizeof(msg));
	aes_eax_finish(&ax, out + sizeof(msg));

	if (memcmp(cipher, out, sizeof(cipher)) != 0)
	{
		fprintf(stderr, "test failed\n");
		exit(1);
	}

	fprintf(stderr, "test success\n");

	/* Test performance */
	aes_eax_start(&ax, &aes, true, nonce, sizeof(nonce), header, sizeof(header));
	for (i = 0; i < 1024*1024; ++i)
		aes_eax_update(&ax, out, out, 32);
	aes_eax_finish(&ax, out);
	return 0;
}

#endif
