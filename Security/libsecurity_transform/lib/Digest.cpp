#include "Digest.h"
#include "SecDigestTransform.h"
#include "Utilities.h"

Digest::Digest(CFStringRef digestType, size_t digestLength) : mDigestType(digestType), mDigestLength(digestLength)
{
	// we don't need to retain the type here because it's a constant
}



Digest::~Digest()
{
}

int Digest::LengthForType(CFStringRef type)
{
    if (!CFStringCompare(kSecDigestSHA1, type, kCFCompareAnchored)) {
        return CC_SHA1_DIGEST_LENGTH;
    } else if (!CFStringCompare(kSecDigestSHA2, type, kCFCompareAnchored)) {
        // XXX SHA2 comes in multiple length flavors, our buest guess is "the big one"
        return CC_SHA256_DIGEST_LENGTH;
    } else if (!CFStringCompare(kSecDigestMD2, type, kCFCompareAnchored)) {
        return CC_MD2_DIGEST_LENGTH;
    } else if (!CFStringCompare(kSecDigestMD4, type, kCFCompareAnchored)) {
        return CC_MD4_DIGEST_LENGTH;
    } else if (!CFStringCompare(kSecDigestMD5, type, kCFCompareAnchored)) {
        return CC_MD5_DIGEST_LENGTH;
    } else {
        return transforms_assume(type == CFSTR("A supported digest type"));
    }
}

CFDictionaryRef Digest::CopyState()
{
	CFMutableDictionaryRef dr = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(dr, kSecDigestTypeAttribute, mDigestType);
	
	CFIndex size = mDigestLength;
	CFNumberRef nr = CFNumberCreate(NULL, kCFNumberCFIndexType, &size);
	CFDictionaryAddValue(dr, kSecDigestLengthAttribute, nr);
	CFRelease(nr);
	
	return dr;
}



MD2Digest::MD2Digest() : Digest(kSecDigestMD2, CC_MD2_DIGEST_LENGTH)
{
	CC_MD2_Init(&mContext);
}



void MD2Digest::Update(const void* buffer, size_t length)
{
	CC_MD2_Update(&mContext, buffer, length);
}



size_t MD2Digest::DigestLength()
{
	return CC_MD2_DIGEST_LENGTH;
}



const void* MD2Digest::Finalize()
{
	CC_MD2_Final(mDigestBuffer, &mContext);
	return mDigestBuffer;
}



MD4Digest::MD4Digest() : Digest(kSecDigestMD4, CC_MD4_DIGEST_LENGTH)
{
	CC_MD4_Init(&mContext);
}



void MD4Digest::Update(const void* buffer, size_t length)
{
	CC_MD4_Update(&mContext, buffer, length);
}



size_t MD4Digest::DigestLength()
{
	return CC_MD4_DIGEST_LENGTH;
}



const void* MD4Digest::Finalize()
{
	CC_MD4_Final(mDigestBuffer, &mContext);
	return mDigestBuffer;
}






MD5Digest::MD5Digest() : Digest(kSecDigestMD5, CC_MD5_DIGEST_LENGTH)
{
	CC_MD5_Init(&mContext);
}



void MD5Digest::Update(const void* buffer, size_t length)
{
	CC_MD5_Update(&mContext, buffer, length);
}



size_t MD5Digest::DigestLength()
{
	return CC_MD5_DIGEST_LENGTH;
}



const void* MD5Digest::Finalize()
{
	CC_MD5_Final(mDigestBuffer, &mContext);
	return mDigestBuffer;
}



SHA1Digest::SHA1Digest() : Digest(kSecDigestSHA1, CC_SHA1_DIGEST_LENGTH)
{
	CC_SHA1_Init(&mContext);
}



void SHA1Digest::Update(const void* buffer, size_t length)
{
	CC_SHA1_Update(&mContext, buffer, length);
}



size_t SHA1Digest::DigestLength()
{
	return CC_SHA1_DIGEST_LENGTH;
}



const void* SHA1Digest::Finalize()
{
	CC_SHA1_Final(mDigestBuffer, &mContext);
	return mDigestBuffer;
}



SHA256Digest::SHA256Digest() : Digest(kSecDigestSHA2, CC_SHA256_DIGEST_LENGTH)
{
	CC_SHA256_Init(&mContext);
}



void SHA256Digest::Update(const void* buffer, size_t length)
{
	CC_SHA256_Update(&mContext, buffer, length);
}



size_t SHA256Digest::DigestLength()
{
	return CC_SHA256_DIGEST_LENGTH;
}



const void* SHA256Digest::Finalize()
{
	CC_SHA256_Final(mDigestBuffer, &mContext);
	return mDigestBuffer;
}



SHA224Digest::SHA224Digest() : Digest(kSecDigestSHA2, CC_SHA224_DIGEST_LENGTH)
{
	CC_SHA224_Init(&mContext);
}



void SHA224Digest::Update(const void* buffer, size_t length)
{
	CC_SHA224_Update(&mContext, buffer, length);
}



size_t SHA224Digest::DigestLength()
{
	return CC_SHA224_DIGEST_LENGTH;
}



const void* SHA224Digest::Finalize()
{
	CC_SHA224_Final(mDigestBuffer, &mContext);
	return mDigestBuffer;
}



SHA512Digest::SHA512Digest() : Digest(kSecDigestSHA2, CC_SHA512_DIGEST_LENGTH)
{
	CC_SHA512_Init(&mContext);
}



void SHA512Digest::Update(const void* buffer, size_t length)
{
	CC_SHA512_Update(&mContext, buffer, length);
}



size_t SHA512Digest::DigestLength()
{
	return CC_SHA512_DIGEST_LENGTH;
}



const void* SHA512Digest::Finalize()
{
	CC_SHA512_Final(mDigestBuffer, &mContext);
	return mDigestBuffer;
}



SHA384Digest::SHA384Digest() : Digest(kSecDigestSHA2, CC_SHA384_DIGEST_LENGTH)
{
	CC_SHA384_Init(&mContext);
}



void SHA384Digest::Update(const void* buffer, size_t length)
{
	CC_SHA384_Update(&mContext, buffer, length);
}



size_t SHA384Digest::DigestLength()
{
	return CC_SHA384_DIGEST_LENGTH;
}



const void* SHA384Digest::Finalize()
{
	CC_SHA384_Final(mDigestBuffer, &mContext);
	return mDigestBuffer;
}



DigestTransform::DigestTransform() : Transform(CFSTR("Digest Transform"))
{
	mDigest = NULL;
}


CFErrorRef DigestTransform::Setup(CFTypeRef dt, CFIndex length)
{
	if (dt == NULL)
	{
		dt = kSecDigestSHA2;
		length = 512;
	}
	transforms_assume_zero(mDigest);
	
	CFStringRef digestType = (CFStringRef) dt;
	
	// figure out what kind of digest is being requested
	if (CFStringCompare(digestType, kSecDigestMD2, 0) == kCFCompareEqualTo)
	{
		mDigest = new MD2Digest();
	}
	else if (CFStringCompare(digestType, kSecDigestMD4, 0) == kCFCompareEqualTo)
	{
		mDigest = new MD4Digest();
	}
	else if (CFStringCompare(digestType, kSecDigestMD5, 0) == kCFCompareEqualTo)
	{
		mDigest = new MD5Digest();
	}
	else if (CFStringCompare(digestType, kSecDigestSHA1, 0) == kCFCompareEqualTo)
	{
		mDigest = new SHA1Digest();
	}
	else if (CFStringCompare(digestType, kSecDigestSHA2, 0) == kCFCompareEqualTo)
	{
		switch (length)
		{
			case 224:
				mDigest = new SHA224Digest();
				break;
			
			case 256:
				mDigest = new SHA256Digest();
				break;
			
			case 384:
				mDigest = new SHA384Digest();
				break;
			
			case 0:
			case 512:
				mDigest = new SHA512Digest();
				break;
				
			default:
			{
				CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidLength, "%d is an invalid digest size (use 224, 256, 384, 512, or 0).", length);
				return result;
			}
		}
	}
	else
	{
		if (CFStringCompare(digestType, kSecDigestHMACMD5, 0) == kCFCompareEqualTo)
		{
			mDigest = new Hmac(this, digestType, kCCHmacAlgMD5, CC_MD5_DIGEST_LENGTH);
		}
		else if (CFStringCompare(digestType, kSecDigestHMACSHA1, 0) == kCFCompareEqualTo)
		{
			mDigest = new Hmac(this, digestType, kCCHmacAlgSHA1, CC_SHA1_DIGEST_LENGTH);
		}
		else if (CFStringCompare(digestType, kSecDigestHMACSHA2, 0) == kCFCompareEqualTo)
		{
			switch (length)
			{
				case 224:
				{
					mDigest = new Hmac(this, digestType, kCCHmacAlgSHA224, CC_SHA224_DIGEST_LENGTH);
				}
				break;
				
				case 256:
				{
					mDigest = new Hmac(this, digestType, kCCHmacAlgSHA256, CC_SHA256_DIGEST_LENGTH);
				}
				break;
				
				case 384:
				{
					mDigest = new Hmac(this, digestType, kCCHmacAlgSHA384, CC_SHA384_DIGEST_LENGTH);
				}
				break;
				
				case 0:
				case 512:
				{
					mDigest = new Hmac(this, digestType, kCCHmacAlgSHA512, CC_SHA512_DIGEST_LENGTH);
				}
				break;
					
				default:
				{
					CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidLength, "%d is an invalid digest size (use 224, 256, 384, 512, or 0).", length);
					return result;
				}
			}
		}
		else
		{
			CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidAlgorithm, "%@ is not a supported digest algorithm (use kSecDigestSHA2, kSecDigestMD2, kSecDigestMD5, kSecDigestSHA or kSecDigestSHA2", digestType);
			return result;
		}
	}
    
    int lengthInt = mDigest->DigestLength();
    CFNumberRef lengthNumber = CFNumberCreate(NULL, kCFNumberIntType, &lengthInt);
    SendAttribute(kSecDigestLengthAttribute, lengthNumber);
    CFRelease(lengthNumber);
	return NULL;
}



static CFStringRef gDigestTransformName = CFSTR("SecDigestTransform");

CFTypeRef DigestTransform::Make()
{
	DigestTransform* dt = new DigestTransform();
	return CoreFoundationHolder::MakeHolder(gDigestTransformName, dt);
}



DigestTransform::~DigestTransform()
{
	if (mDigest != NULL)
	{
		delete mDigest;
	}
}



void DigestTransform::AttributeChanged(CFStringRef name, CFTypeRef value)
{
	if (CFStringCompare(name, kSecTransformInputAttributeName, 0) == kCFCompareEqualTo)
	{
		if (value == NULL)
		{
			// we are done, finalize the digest and send the result forward
			const void* result = mDigest->Finalize();
			
			// send the result
			CFDataRef resultRef = CFDataCreate(NULL, (UInt8*) result, mDigest->DigestLength());
			SendAttribute(kSecTransformOutputAttributeName, resultRef);
			CFRelease(resultRef);
			
			// send the EOS
			SendAttribute(kSecTransformOutputAttributeName, NULL);
		}
		else
		{
			// if we got an error, just pass it on
			CFTypeID valueType = CFGetTypeID(value);
			if (valueType == CFErrorGetTypeID())
			{
				SendAttribute(kSecTransformOutputAttributeName, value);
			}
			else if (valueType != CFDataGetTypeID())
			{
				CFStringRef idType = CFCopyTypeIDDescription(valueType);
				CFErrorRef result = CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "value is not a CFDataRef -- it's a %@ instead", idType);
				CFRelease(idType);
				SetAttributeNoCallback(kSecTransformOutputAttributeName, result);
			}
			else
			{
				CFDataRef valueRef = (CFDataRef) value;
				mDigest->Update(CFDataGetBytePtr(valueRef), CFDataGetLength(valueRef));
			}
		}
	}
}



class DigestTransformFactory : public TransformFactory
{
public:
	DigestTransformFactory();
	virtual CFTypeRef Make();
};


DigestTransformFactory::DigestTransformFactory() : TransformFactory(gDigestTransformName, true)
{
}



CFTypeRef DigestTransformFactory::Make()
{
	return DigestTransform::Make();
}



TransformFactory* DigestTransform::MakeTransformFactory()
{
	return new DigestTransformFactory();
}



CFDictionaryRef DigestTransform::CopyState()
{
	return mDigest->CopyState();
}



void DigestTransform::RestoreState(CFDictionaryRef state)
{
	if (mDigest != NULL)
	{
		delete mDigest;
	}
	
	// get the type and state from the dictionary
	CFStringRef type = (CFStringRef) CFDictionaryGetValue(state, kSecDigestTypeAttribute);
	CFNumberRef size = (CFNumberRef) CFDictionaryGetValue(state, kSecDigestLengthAttribute);
	CFIndex realSize;
	CFNumberGetValue(size, kCFNumberCFIndexType, &realSize);
	
	Setup(type, realSize);
}



CFTypeID DigestTransform::GetCFTypeID()
{
	return CoreFoundationObject::FindObjectType(gDigestTransformName);
}



Hmac::Hmac(Transform* parentTransform, CFStringRef digestType, CCHmacAlgorithm alg, size_t length) :
	Digest(digestType, length), mInitialized(false), mParentTransform(parentTransform), mAlg(alg)
{
}



Hmac::~Hmac()
{
	free(mDigestBuffer);
}



void Hmac::Initialize()
{
	// initialize
	const UInt8* data = NULL;
	size_t dataLength = 0;

	CFDataRef key = (CFDataRef) mParentTransform->GetAttribute(kSecDigestHMACKeyAttribute);
	if (key)
	{
		data = CFDataGetBytePtr(key);
		dataLength = CFDataGetLength(key);
	}
	
	CCHmacInit(&mHMACContext, mAlg, data, dataLength);
	
	// make room to hold the result
	mDigestBuffer = (UInt8*) malloc(mDigestLength);
	
	mInitialized = true;
}



void Hmac::Update(const void* buffer, size_t length)
{
	if (!mInitialized)
	{
		Initialize();
	}
	
	CCHmacUpdate(&mHMACContext, buffer, length);
}



size_t Hmac::DigestLength()
{
	return mDigestLength;
}



const void* Hmac::Finalize()
{
	CCHmacFinal(&mHMACContext, mDigestBuffer);
	return mDigestBuffer;
}

