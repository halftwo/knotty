#ifndef CMAC_AES_H_
#define CMAC_AES_H_

#include "rijndael.h"
#include <stdint.h>
#include <sys/types.h>		/* for ssize_t */

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
	rijndael_context aes;
	uint8_t X[16];
	ssize_t count;
} cmac_aes_context;


void cmac_aes_start(cmac_aes_context *ctx, const void *key, size_t key_size);

void cmac_aes_update(cmac_aes_context *ctx, const void *msg, size_t msg_len);

void cmac_aes_finish(cmac_aes_context *ctx, uint8_t *mac, size_t mac_size);

void cmac_aes_checksum(const void *key, size_t key_size, const void *msg, size_t msg_len,
			uint8_t *mac, size_t mac_size);


#ifdef __cplusplus
}
#endif

#endif
