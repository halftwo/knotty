#ifndef AES_CCM_H_
#define AES_CCM_H_

#include "rijndael.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>		/* for ssize_t */

typedef struct aes_ccm_t aes_ccm_t;

struct aes_ccm_t
{
	const rijndael_context *ctx;
	uint8_t mac_size;
	uint8_t counter_size;

	bool encrypt;
	ssize_t total;
	ssize_t pos;
	uint8_t X[16];
	uint8_t A[16];
	uint8_t S[16];
};


void aes_ccm_config(aes_ccm_t *ac, const rijndael_context *ctx, size_t mac_size, size_t counter_size);

/* length of nonce is (15 - counter_size)
 */
void aes_ccm_start(aes_ccm_t *ac, bool encrypt, const uint8_t *nonce, const uint8_t *aad, size_t aad_len, size_t msg_len);

ssize_t aes_ccm_update(aes_ccm_t *ac, const uint8_t *in, uint8_t *out, size_t len);

ssize_t aes_ccm_finish(aes_ccm_t *ac, uint8_t *mac);


#endif
