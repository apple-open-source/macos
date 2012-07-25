#include "SecDigestTransform.h"
#include "Digest.h"



const CFStringRef kSecDigestMD2 = CFSTR("MD2 Digest"),
				  kSecDigestMD4 = CFSTR("MD4 Digest"),
				  kSecDigestMD5 = CFSTR("MD5 Digest"),
				  kSecDigestSHA1 = CFSTR("SHA1 Digest"),
				  kSecDigestSHA2 = CFSTR("SHA2 Digest Family"),
				  kSecDigestHMACMD5 = CFSTR("HMAC-MD5"),
				  kSecDigestHMACSHA1 = CFSTR("HMAC-SHA1"),
				  kSecDigestHMACSHA2 = CFSTR("HMAC-SHA2 Digest Family");

const CFStringRef kSecDigestTypeAttribute = CFSTR("Digest Type"),
				  kSecDigestLengthAttribute = CFSTR("Digest Length"),
				  kSecDigestHMACKeyAttribute = CFSTR("HMAC Key");

SecTransformRef SecDigestTransformCreate(CFTypeRef digestType,
										 CFIndex digestLength,
										 CFErrorRef* error
										 )
{
	SecTransformRef tr = DigestTransform::Make();
	DigestTransform* dt = (DigestTransform*) CoreFoundationHolder::ObjectFromCFType(tr);
	
	CFErrorRef result = dt->Setup(digestType, digestLength);
	if (result != NULL)
	{
		// an error occurred
		CFRelease(tr);
		
		if (error)
		{
			*error = result;
		}
		
		return NULL;
	}
	
	return tr;
}



CFTypeID SecDigestTransformGetTypeID()
{
	return DigestTransform::GetCFTypeID();
}
