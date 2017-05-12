#ifndef HMAC_H_
#define HMAC_H_

#include "sha1.h"
#include <stdint.h>

typedef struct
{
	sha1_context i_ctx;
	sha1_context o_ctx;
} hmac_sha1_context;


void hmac_sha1_start(hmac_sha1_context *ctx, const void *key, size_t key_size);

void hmac_sha1_update(hmac_sha1_context *ctx, const void *msg, size_t msg_len);

void hmac_sha1_finish(hmac_sha1_context *ctx, uint8_t *mac, size_t size);

void hmac_sha1_checksum(const void *key, size_t key_size, const void *msg, size_t msg_len,
			uint8_t *mac, size_t mac_size);


#endif
