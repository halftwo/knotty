#ifndef MyCipher_h_
#define MyCipher_h_

#include "xslib/XRefCount.h"
#include "xslib/aes_eax.h"

class MyCipher: public XRefCount
{
	rijndael_context aes;
	aes_eax_context oax;	// encrypt
	aes_eax_context iax;	// decrypt

	uint8_t oNonce[20];	// encrypt
	uint8_t iNonce[20];	// decrypt

	bool _mode0;
	/* nonce = salt + IV */
	uint8_t salt[16];
	size_t salt_size;

public:
	uint8_t oMAC[16];	// encrypt
	uint8_t iMAC[16];	// decrypt

	// mode0
	uint8_t oSeq[8];
	uint8_t oIV[16];	// encrypt

	uint8_t iSeq[8];
	uint8_t iIV[16];	// decrypt

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
	static int get_cipher_mode_from_name(const std::string& name);
	static const char *get_cipher_name_from_id(CipherSuite suite);

	MyCipher(CipherSuite suite, const void *key, size_t key_size, bool isServer);
	virtual ~MyCipher();

	void setMode0(bool mode0)
	{
		_mode0 = mode0;
	}

	bool mode0()
	{
		return _mode0;
	}

	size_t extraSize()
	{
		return sizeof(iMAC);
	}

	void encryptStart(const void *header, size_t header_size);
	void encryptUpdate(const void *in, void *out, size_t len);
	/* After calling encryptFinish(), oMAC is set */
	void encryptFinish();

	void decryptStart(const void *header, size_t header_size);
	void decryptUpdate(const void *in, void *out, size_t len);
	/* Must set iMAC before calling decryptFinish() */
	bool decryptFinish();


	//
	// ---------- mode0 ----------------
	//

	size_t extraSizeMode0()
	{
		return sizeof(iIV) + sizeof(iMAC);
	}

	/*
 	 * oSeqIncreaseMode0() should be called before encryptStartMode0().
	 * After calling encryptStartMode0(), oIV is set.
 	 */
	void oSeqIncreaseMode0();
	void iSeqIncreaseMode0();

	void encryptStartMode0(const void *header, size_t header_size);

	/* 
 	 * iSeqIncreaseMode0() should be called before decryptCheckSequenceMode0().
	 * Must set iIV before calling decryptCheckSequenceMode0() 
	 */
	bool decryptCheckSequenceMode0();
	void decryptStartMode0(const void *header, size_t header_size);
};
typedef XPtr<MyCipher> MyCipherPtr;

#endif
