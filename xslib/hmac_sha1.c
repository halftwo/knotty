#include "hmac_sha1.h"
#include <string.h>


void hmac_sha1_start(hmac_sha1_context *ctx, const void *key, size_t key_size)
{
	size_t i;
	uint8_t ipad[64];
	uint8_t opad[64];

	if (key_size > sizeof(ipad))
	{
		sha1_checksum(ipad, key, key_size);
		key_size = 20;
	}
	else
	{
		memcpy(ipad, key, key_size);
	}

	memcpy(opad, ipad, key_size);

	for (i = 0; i < key_size; ++i)
	{
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}
	
	if (key_size < sizeof(ipad))
	{
		memset(ipad + key_size, 0x36, sizeof(ipad) - key_size);
		memset(opad + key_size, 0x5c, sizeof(opad) - key_size);
	}

	sha1_start(&ctx->i_ctx);
	sha1_update(&ctx->i_ctx, ipad, sizeof(ipad));

	sha1_start(&ctx->o_ctx);
	sha1_update(&ctx->o_ctx, opad, sizeof(opad));
}

void hmac_sha1_update(hmac_sha1_context *ctx, const void *msg, size_t msg_len)
{
	sha1_update(&ctx->i_ctx, msg, msg_len);
}

void hmac_sha1_finish(hmac_sha1_context *ctx, uint8_t *mac, size_t size)
{
	uint8_t digest[20];
	sha1_finish(&ctx->i_ctx, digest);

	sha1_update(&ctx->o_ctx, digest, sizeof(digest));
	sha1_finish(&ctx->o_ctx, digest);
	
	if (size > sizeof(digest))
		size = sizeof(digest);
	memcpy(mac, digest, size);
}

void hmac_sha1_checksum(const void *key, size_t key_size, const void *msg, size_t msg_len,
			uint8_t *mac, size_t mac_size)
{
	hmac_sha1_context ctx;
	hmac_sha1_start(&ctx, key, key_size);
	hmac_sha1_update(&ctx, msg, msg_len);
	hmac_sha1_finish(&ctx, mac, mac_size);
}


#ifdef TEST_HMAC_SHA1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

bool do_test(const char *key, const char *msg, const char *mac)
{
	uint8_t hmac[20];
	size_t key_len = strlen(key);
	size_t msg_len = strlen(msg);
	size_t mac_len = strlen(mac);

	if (mac_len < sizeof(hmac) * 2)
		return false;

	hmac_sha1_checksum(key, key_len, msg, msg_len, hmac, sizeof(hmac));

	int i;
	for (i = 0; i < sizeof(hmac); ++i)
	{
		char a[3];
		snprintf(a, sizeof(a), "%02x", hmac[i]);
		if (strncasecmp(a, &mac[i*2], 2) != 0)
			return false;
	}
	return true;
}

int main()
{
	int i;
	struct test_t {
		char *key;
		char *msg;
		char *mac;
	} tests [] = {
		{"", "", "fbdb1d1b18aa6c08324b7d64b71fb76370690e1d"},
		{"key", "The quick brown fox jumps over the lazy dog", "de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9"},
	};

	for (i = 0; i < sizeof(tests)/sizeof(tests[0]); ++i)
	{
		struct test_t *t = &tests[i];
		bool ok = do_test(t->key, t->msg, t->mac);
		if (!ok)
		{
			fprintf(stderr, "Test failed\n");
			exit(1);
		}
	}

	fprintf(stderr, "Tests success\n");
	return 0;
}

#endif

