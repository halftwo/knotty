#include "MyCipher.h"
#include "xslib/urandom.h"
#include "xslib/xstr.h"
#include "xslib/sha1.h"
#include <string.h>

static bool suite_name_equal(const xstr_t& xs, const char *name)
{
        const uint8_t *s1 = xs.data;
        size_t n = xs.len;
	const char *s2 = name;

        while (n-- > 0)
        {
                int c1 = (uint8_t)*s1++;
                int c2 = (uint8_t)*s2++;

                if (c2 == 0)
                        return false;

                if (c1 != c2)
                {
                        if (c1 >= 'A' && c1 <= 'Z')
                                c1 |= 0x20;
			else if (c1 == '_')
				c1 = '-';

                        if (c2 >= 'A' && c2 <= 'Z')
                                c2 |= 0x20;

                        if (c1 != c2)
				return false;
                }
        }

        return (*s2) ? false : true;
}

MyCipher::CipherSuite MyCipher::get_cipher_id_from_name(const std::string& name)
{
	CipherSuite suite = UNKNOWN_SUITE;
	xstr_t xs = XSTR_CXX(name);
	if (suite_name_equal(xs, "CLEARTEXT"))
		suite = CLEARTEXT;
	else if (suite_name_equal(xs, "AES128-EAX"))
		suite = AES128_EAX;
	else if (suite_name_equal(xs, "AES192-EAX"))
		suite = AES192_EAX;
	else if (suite_name_equal(xs, "AES256-EAX"))
		suite = AES256_EAX;
	return suite;
}

const char *MyCipher::get_cipher_name_from_id(MyCipher::CipherSuite suite)
{
	switch (suite)
	{
	case CLEARTEXT:  return "CLEARTEXT";
	case AES128_EAX: return "AES128-EAX";
	case AES192_EAX: return "AES192-EAX";
	case AES256_EAX: return "AES256-EAX";
	default:
		break;
	}
	return "UNKNOWN";
}

MyCipher::MyCipher(MyCipher::CipherSuite suite, const void *keyInfo, size_t keyInfoSize, bool isServer)
{
	size_t key_len = (suite == AES128_EAX) ? 16
			: (suite == AES192_EAX) ? 24
			: (suite == AES256_EAX) ? 32
			: 0;

	if (key_len == 0)
		throw XERROR_FMT(XError, "Unsupported CipherSuite %d", suite);

	uint8_t key[32];
	if (keyInfoSize > key_len)
	{
		memcpy(key, keyInfo, key_len);
	}
	else
	{
		memset(key, 0, sizeof(key));
		memcpy(key, keyInfo, keyInfoSize);
	}

	if (!rijndael_setup_encrypt(&this->aes, key, key_len))
		throw XERROR_MSG(XError, "rijndael_setup_encrypt() failed");

	sha1_checksum(this->oNonce, keyInfo, keyInfoSize);
	memcpy(this->iNonce, this->oNonce, sizeof(this->oNonce));
	if (isServer)
	{
		this->oNonce[sizeof(this->oNonce)-1] |= 0x01;
		this->iNonce[sizeof(this->iNonce)-1] &= ~0x01;
	}
	else
	{
		this->iNonce[sizeof(this->iNonce)-1] |= 0x01;
		this->oNonce[sizeof(this->oNonce)-1] &= ~0x01;
	}
}

MyCipher::~MyCipher()
{
	rijndael_clear_context(&this->aes);
}


static inline void increase2counter(uint8_t *counter, size_t size)
{
	// +1
        for (size_t i = size - 1; i >= 0; --i)
        {
                ++counter[i];
                if (counter[i])
                        break;
        }

	// +1
        for (size_t i = size - 1; i >= 0; --i)
        {
                ++counter[i];
                if (counter[i])
                        break;
        }
}

void MyCipher::encryptStart(const void *header, size_t header_len)
{
	increase2counter(this->oNonce, sizeof(this->oNonce));
	aes_eax_start(&this->oax, &this->aes, true, this->oNonce, sizeof(this->oNonce), header, header_len);
}

void MyCipher::encryptUpdate(const void *in, void *out, size_t len)
{
	aes_eax_update(&this->oax, in, out, len);
}

void MyCipher::encryptFinish()
{
	aes_eax_finish(&this->oax, this->oMAC);
}

void MyCipher::decryptStart(const void *header, size_t header_len)
{
	increase2counter(this->iNonce, sizeof(this->iNonce));
	aes_eax_start(&this->iax, &this->aes, false, this->iNonce, sizeof(this->iNonce), header, header_len);
}

void MyCipher::decryptUpdate(const void *in, void *out, size_t len)
{
	aes_eax_update(&this->iax, in, out, len);
}

bool MyCipher::decryptFinish()
{
	uint8_t mac[16];
	aes_eax_finish(&this->iax, mac);
	return (memcmp(mac, this->iMAC, sizeof(mac)) == 0);
}

