#include "Transform.h"
#include "TransformFactory.h"

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>


// set up an abstraction for digest.  We have to do this because
// CommonCrypto doesn't support a unified digest model

class Digest
{
protected:
	CFStringRef mDigestType;
	int mDigestLength;

public:
	Digest(CFStringRef digestType, size_t digestLength);
	
	virtual ~Digest();
	
	virtual void Update(const void* buffer, size_t length) = 0;
	virtual size_t DigestLength() = 0;
	virtual const void* Finalize() = 0;
	
	virtual CFDictionaryRef CopyState();
    
    static int LengthForType(CFStringRef type);
};



class MD2Digest : public Digest
{
protected:
	CC_MD2_CTX mContext;
	u_int8_t mDigestBuffer[CC_MD2_DIGEST_LENGTH];

public:
	MD2Digest();
	
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};



class MD4Digest : public Digest
{
protected:
	CC_MD4_CTX mContext;
	u_int8_t mDigestBuffer[CC_MD4_DIGEST_LENGTH];

public:
	MD4Digest();
	
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};



class MD5Digest : public Digest
{
protected:
	CC_MD5_CTX mContext;
	u_int8_t mDigestBuffer[CC_MD5_DIGEST_LENGTH];

public:
	MD5Digest();
	
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};



class SHA1Digest : public Digest
{
protected:
	CC_SHA1_CTX mContext;
	u_int8_t mDigestBuffer[CC_SHA1_DIGEST_LENGTH];

public:
	SHA1Digest();
	
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};



class SHA256Digest : public Digest
{
protected:
	CC_SHA256_CTX mContext;
	u_int8_t mDigestBuffer[CC_SHA256_DIGEST_LENGTH];

public:
	SHA256Digest();
	
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};



class SHA224Digest : public Digest
{
protected:
	CC_SHA256_CTX mContext;
	u_int8_t mDigestBuffer[CC_SHA256_DIGEST_LENGTH];

public:
	SHA224Digest();
	
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};



class SHA512Digest : public Digest
{
protected:
	CC_SHA512_CTX mContext;
	u_int8_t mDigestBuffer[CC_SHA512_DIGEST_LENGTH];
	
public:
	SHA512Digest();
	
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};



class SHA384Digest : public Digest
{
protected:
	CC_SHA512_CTX mContext;
	u_int8_t mDigestBuffer[CC_SHA512_DIGEST_LENGTH];
	
public:
	SHA384Digest();
	
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};



class Hmac : public Digest
{
protected:
	bool mInitialized;
	CCHmacContext mHMACContext;
	u_int8_t *mDigestBuffer;
	CFDataRef mKey;
	Transform* mParentTransform;
	CCHmacAlgorithm mAlg;

	void Initialize();

public:
	Hmac(Transform* parentTransform, CFStringRef digestType, CCHmacAlgorithm alg, size_t length);
	virtual ~Hmac();
	void Update(const void* buffer, size_t length);
	size_t DigestLength();
	const void* Finalize();
};


class DigestTransform : public Transform
{
protected:
	Digest* mDigest;
	
	DigestTransform();

public:
	static CFTypeRef Make();
	virtual ~DigestTransform();

	CFErrorRef Setup(CFTypeRef digestType, CFIndex length);
	
	virtual void AttributeChanged(CFStringRef name, CFTypeRef value);

	static TransformFactory* MakeTransformFactory();
	
	CFDictionaryRef CopyState();
	void RestoreState(CFDictionaryRef state);
	static CFTypeID GetCFTypeID();
};
