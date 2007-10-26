//
// Fast hash support
//
#ifndef _H_HASHING
#define _H_HASHING

#include <CommonCrypto/CommonDigest.h>
#include <cstring>
#include <sys/types.h>

namespace Security {


//
// Common prefix for all hashers
//
template <uint32_t _size>
class Hash {
public:
	typedef unsigned char Byte;
	static const size_t digestLength = _size;
	typedef Byte Digest[_size];
	struct SDigest {
		Digest data;
	};
};


//
// Concrete hash operators. These all follow a common pattern,
// corresponding to the one established in <CommonCrypto/CommonDigest.h>.
//
class SHA1 : public CC_SHA1_CTX, public Hash<CC_SHA1_DIGEST_LENGTH>	{
public:
	SHA1() { CC_SHA1_Init(this); }
	void operator () (const void *data, size_t length)
		{ CC_SHA1_Update(this, data, length); }
	void finish(Byte *digest) { CC_SHA1_Final(digest, this); }
	
	void finish(SDigest &digest) { finish(digest.data); }
	bool verify(const Byte *digest)
	{ Digest d; finish(d); return !memcmp(d, digest, digestLength); }
};

class SHA256 : public CC_SHA256_CTX, public Hash<CC_SHA256_DIGEST_LENGTH>	{
public:
	SHA256() { CC_SHA256_Init(this); }
	void operator () (const void *data, size_t length)
		{ CC_SHA256_Update(this, data, length); }
	void finish(Byte *digest) { CC_SHA256_Final(digest, this); }
	
	void finish(SDigest &digest) { finish(digest.data); }
	bool verify(const Byte *digest)
	{ Digest d; finish(d); return !memcmp(d, digest, digestLength); }
};


}	// Security

#endif //_H_HASHING
