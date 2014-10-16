/*
 * p12GetPassKey.h - get a CSSM_ALGID_SECURE_PASSPHRASE key for encode/decode
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include "p12GetPassKey.h"
#include <CoreServices.framework/Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/cssmapple.h>
#include <utilLib/cspwrap.h>

/* when true, simulate secure passphrase in CSPDL */
#define SIMULATE_PASSPHRASE		1

/* 
 * Safe gets().
 * -- guaranteed no buffer overflow
 * -- guaranteed NULL-terminated string
 * -- handles empty string (i.e., response is just CR) properly
 */
void getString(
	char *buf,
	unsigned bufSize)
{
	unsigned dex;
	char c;
	char *cp = buf;
	
	for(dex=0; dex<bufSize-1; dex++) {
		c = getchar();
		if (c == EOF) {
			break;
		}
		
		if(!isprint(c)) {
			break;
		}
		switch(c) {
			case '\n':
			case '\r':
				goto done;
			default:
				*cp++ = c;
		}
	}
done:
	*cp = '\0';
}

OSStatus p12GetPassKey(
	CSSM_CSP_HANDLE	cspHand,
	GPK_Type 		gpkType,
	bool			isRawCsp,
	CSSM_KEY		*passKey)		// RETURNED
{
	if(isRawCsp || SIMULATE_PASSPHRASE) {
		char passphrase[512];
		
		if(gpkType == GPK_Decode) {
			printf("Enter passphrase for PKCS12 Decode: ");
		}
		else {
			printf("Enter passphrase for PKCS12 Encode: ");
		}
		getString(passphrase, 512);
		
		/* cook up a raw key with passphrase as data */
		unsigned phraseLen = strlen(passphrase);
		CSSM_KEY		rawKey;
		memset(&rawKey, 0, sizeof(CSSM_KEY));
		CSSM_KEYHEADER	&hdr = rawKey.KeyHeader;
		hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
		hdr.BlobType = CSSM_KEYBLOB_RAW;
		hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
		hdr.AlgorithmId = CSSM_ALGID_SECURE_PASSPHRASE;
		hdr.KeyClass = CSSM_KEYCLASS_SESSION_KEY;
		hdr.LogicalKeySizeInBits = phraseLen * 2 * 8;
		hdr.KeyAttr = CSSM_KEYATTR_MODIFIABLE | CSSM_KEYATTR_EXTRACTABLE;
		hdr.KeyUsage = CSSM_KEYUSE_DERIVE;
		
		#if 0
		/* data = Unicode version of C string passphrase, bigendian */
		rawKey.KeyData.Length = phraseLen * 2;
		rawKey.KeyData.Data = (uint8 *)malloc(phraseLen * 2);
		const char *cpIn = passphrase;
		char *cpOut = (char *)rawKey.KeyData.Data;
		
		for(unsigned dex=0; dex<phraseLen; dex++) {
			*cpOut++ = 0;
			*cpOut++ = *cpIn++;
		}
		#else
		
		/* data = external representation of CFString */
		CFStringRef cfStr = CFStringCreateWithCString(NULL, passphrase,
			kCFStringEncodingASCII);
		CFDataRef cfData = CFStringCreateExternalRepresentation(NULL,
			cfStr, kCFStringEncodingUnicode, 0);
		unsigned keyLen = CFDataGetLength(cfData);
		rawKey.KeyData.Length = keyLen;
		rawKey.KeyData.Data = (uint8 *)malloc(keyLen);
		memmove(rawKey.KeyData.Data, CFDataGetBytePtr(cfData), keyLen);
		CFRelease(cfData);
		CFRelease(cfStr);
		hdr.LogicalKeySizeInBits = keyLen * 8;
		#endif
		CSSM_DATA descrData = {0, NULL};
		
		/* NULL unwrap to make a ref key */
		CSSM_RETURN crtn = cspUnwrapKey(cspHand,
			&rawKey,
			NULL,			// wrappingKey
			CSSM_ALGID_NONE,
			0, 0, 0, 		// mode, pad, vector
			passKey,
			&descrData,
			"someLabel",
			9);				// labelLen 
		if(crtn) {
			printf("***Error doing NULL wrap of passKey.\n");
			return crtn;
		}
		return crtn;
	}
	else {
		printf("SS does not support secure passphrase yet.");
		/*
		 * TBD: do a DeriveKey
		 */
		return unimpErr;
	}
}
