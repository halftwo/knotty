#include "cmac_aes.h"
#include "rijndael.h"
#include <string.h>

#define BLOCK_SIZE	16

static inline void xor_block(uint8_t *d, const uint8_t *s)
{
	*d++ ^= *s++; 	*d++ ^= *s++;
	*d++ ^= *s++; 	*d++ ^= *s++;
	*d++ ^= *s++; 	*d++ ^= *s++;
	*d++ ^= *s++; 	*d++ ^= *s++;

	*d++ ^= *s++; 	*d++ ^= *s++;
	*d++ ^= *s++; 	*d++ ^= *s++;
	*d++ ^= *s++; 	*d++ ^= *s++;
	*d++ ^= *s++; 	*d++ ^= *s++;
}

void cmac_aes_start(cmac_aes_context *ca, const rijndael_context *aes)
{
	ca->aes = aes;
	memset(ca->X, 0, sizeof(ca->X));
	ca->count = 0;
}

void cmac_aes_update(cmac_aes_context *ca, const void *msg, size_t len)
{
	const uint8_t *p = (const uint8_t *)msg;
	ssize_t k = ca->count % BLOCK_SIZE;
	ssize_t i;

	if ((ssize_t)len <= 0)
		return;

	if (k == 0 && ca->count)
		rijndael_encrypt(ca->aes, ca->X, ca->X);

	for (i = 0; i < len && k < BLOCK_SIZE; ++i)
		ca->X[k++] ^= *p++;

	len -= i;
	ca->count += i;

	while (len >= BLOCK_SIZE)
	{
		rijndael_encrypt(ca->aes, ca->X, ca->X);
		xor_block(ca->X, p);

		p += BLOCK_SIZE;
		len -= BLOCK_SIZE;
		ca->count += BLOCK_SIZE;
	}

	if (len > 0)
	{
		rijndael_encrypt(ca->aes, ca->X, ca->X);
		for (k = 0; k < len;)
			ca->X[k++] ^= *p++;

		ca->count += len;
	}
}


static const uint8_t c_xor[4] = { 0x00, 0x87, 0x0e, 0x89 };

static void gf_mulx1(uint8_t pad[BLOCK_SIZE])
{
	int i;
	int t = pad[0] >> 7;

	for (i = 0; i < BLOCK_SIZE - 1; ++i)
		pad[i] = (pad[i] << 1) | (pad[i + 1] >> 7);

	pad[BLOCK_SIZE - 1] = (pad[BLOCK_SIZE - 1] << 1) ^ c_xor[t];
}

static void gf_mulx2(uint8_t pad[BLOCK_SIZE])
{
	int i;
	int t = pad[0] >> 6;

	for(i = 0; i < BLOCK_SIZE - 1; ++i)
		pad[i] = (pad[i] << 2) | (pad[i + 1] >> 6);

	pad[BLOCK_SIZE - 2] ^= (t >> 1);
	pad[BLOCK_SIZE - 1] = (pad[BLOCK_SIZE - 1] << 2) ^ c_xor[t];
}

void cmac_aes_finish(cmac_aes_context *ca, uint8_t mac[16])
{
	uint8_t pad[BLOCK_SIZE];
	ssize_t k = ca->count % BLOCK_SIZE;

	memset(pad, 0, sizeof(pad));
	rijndael_encrypt(ca->aes, pad, pad);

	if (ca->count == 0 || k)
	{
		ca->X[k] ^= 0x80;
		gf_mulx2(pad);
	}
	else
	{
		gf_mulx1(pad);
	}

	xor_block(pad, ca->X);
	rijndael_encrypt(ca->aes, pad, pad);

	memcpy(mac, pad, sizeof(pad));
}

void cmac_aes_checksum(const rijndael_context *aes, const void *msg, size_t msg_len, uint8_t mac[16])
{
	cmac_aes_context ca;
	cmac_aes_start(&ca, aes);
	cmac_aes_update(&ca, msg, msg_len);
	cmac_aes_finish(&ca, mac);
}



#ifdef TEST_CMAC_AES

#include "hex.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	rijndael_context aes;
	cmac_aes_context ca;

	char *key_hex = "2B7E1516 28AED2A6 ABF71588 09CF4F3C";
	char *msg_hex = "6BC1BEE2 2E409F96 E93D7E11 7393172A AE2D8A57";
	char *mac_hex[] = {
		"BB1D6929 E9593728 7FA37D12 9B756746",
		"070A16B4 6B4D4144 F79BDD9D D04A287C",
		"7D85449E A6EA19C8 23A7BF78 837DFADE",
	};
	size_t msg_len[] = { 0, 16, 20 };

	uint8_t key[16];
	uint8_t msg[20];
	uint8_t mac1[16];
	uint8_t mac2[16];
	char buf[64];
	int i;

	unhexlify_ignore_space(key, key_hex, -1);
	unhexlify_ignore_space(msg, msg_hex, -1);

	rijndael_setup_encrypt(&aes, key, sizeof(key));

	for (i = 0; i < 3; ++i)
	{
		int k;
		unhexlify_ignore_space(mac1, mac_hex[i], -1);

		cmac_aes_start(&ca, &aes);
//		cmac_aes_update(&ca, msg, msg_len[i]);
		for (k = 0; k < msg_len[i]; ++k)
			cmac_aes_update(&ca, msg + k, 1);
		cmac_aes_finish(&ca, mac2);

		if (memcmp(mac1, mac2, sizeof(mac2)) != 0)
		{
			fprintf(stderr, "test failed\n");
			exit(1);	
		}
	}

	fprintf(stderr, "test success\n");
	return 0;
}

#endif
