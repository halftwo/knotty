#ifndef MyCipher_h_
#define MyCipher_h_

#include "xslib/XRefCount.h"
#include "xslib/aes_eax.h"

class MyCipher: public XRefCount
{
	rijndael_context aes;
	aes_eax_context oax;	// encrypt
	aes_eax_context iax;	// decrypt

	/* nonce = salt + IV */
	uint8_t salt[16];
	size_t salt_size;

public:
	uint8_t oSeq[8];
	uint8_t oIV[16];	// encrypt
	uint8_t oMAC[16];	// encrypt

	uint8_t iSeq[8];
	uint8_t iIV[16];	// decrypt
	uint8_t iMAC[16];	// decrypt

	enum CipherSuite
	{
		UNKNOWN_SUITE 	= -1,
		CLEARTEXT 	= 0,
		AES128_EAX	= 1,
		AES192_EAX	= 2,
		AES256_EAX	= 3,
		MAX_SUITE,
	};

	static CipherSuite get_cipher_id_from_name(const std::string& name);
	static const char *get_cipher_name_from_id(CipherSuite suite);

	MyCipher(CipherSuite suite, const void *key, size_t key_size, bool isServer);
	virtual ~MyCipher();

	size_t extraSize()
	{
		return sizeof(iIV) + sizeof(iMAC);
	}

	bool oSeqIncrease();
	bool iSeqIncrease();

	/*
 	 * oSeqIncrease() should be called before encryptStart().
	 * After calling encryptStart(), oIV is set.
 	 */
	void encryptStart(const void *header, size_t header_size);
	void encryptUpdate(const void *in, void *out, size_t len);
	/* After calling encryptFinish(), oMAC is set */
	void encryptFinish();

	/* 
 	 * iSeqIncrease() should be called before decryptCheckSequence().
	 * Must set iIV before calling decryptCheckSequence() 
	 */
	bool decryptCheckSequence();
	void decryptStart(const void *header, size_t header_size);
	void decryptUpdate(const void *in, void *out, size_t len);
	/* Must set iMAC before calling decryptFinish() */
	bool decryptFinish();
};
typedef XPtr<MyCipher> MyCipherPtr;

#endif
