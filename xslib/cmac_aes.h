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
	const rijndael_context *aes;
	uint8_t X[16];
	ssize_t count;
} cmac_aes_context;


void cmac_aes_start(cmac_aes_context *ctx, const rijndael_context *aes);

void cmac_aes_update(cmac_aes_context *ctx, const void *msg, size_t msg_len);

void cmac_aes_finish(cmac_aes_context *ctx, uint8_t mac[16]);

void cmac_aes_checksum(const rijndael_context *aes, const void *msg, size_t msg_len, uint8_t mac[16]);


#ifdef __cplusplus
}
#endif

#endif
