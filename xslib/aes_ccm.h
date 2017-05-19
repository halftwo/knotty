#ifndef AES_CCM_H_
#define AES_CCM_H_

#include "rijndael.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>		/* for ssize_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
	const rijndael_context *aes;
	uint8_t mac_size;
	uint8_t counter_size;

	bool encrypt;
	ssize_t total;
	ssize_t pos;
	uint8_t X[16];
	uint8_t A[16];
	uint8_t S[16];
} aes_ccm_context;


void aes_ccm_config(aes_ccm_context *ac, const rijndael_context *aes, size_t mac_size, size_t counter_size);

/* length of nonce is (15 - counter_size)
 */
void aes_ccm_start(aes_ccm_context *ac, bool encrypt, const void *nonce, const void *aad, size_t aad_len, size_t msg_len);

ssize_t aes_ccm_update(aes_ccm_context *ac, const void *in, void *out, size_t len);

ssize_t aes_ccm_finish(aes_ccm_context *ac, uint8_t *mac);


#ifdef __cplusplus
}
#endif

#endif
