#ifndef AES_CCM_H_
#define AES_CCM_H_

#include "rijndael.h"
#include "cmac_aes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
	cmac_aes_context cmac;
	bool encrypt;
	int pos;
	uint8_t S[16];
	uint8_t C[16];
	uint8_t N[16];
	uint8_t H[16];
} aes_eax_context;


void aes_eax_start(aes_eax_context *ax, const rijndael_context *aes, bool encrypt, 
		const void *nonce, size_t nonce_len, const void *header, size_t header_len);

void aes_eax_update(aes_eax_context *ax, const void *in, void *out, size_t len);

void aes_eax_finish(aes_eax_context *ax, uint8_t mac[16]);


#ifdef __cplusplus
}
#endif

#endif

