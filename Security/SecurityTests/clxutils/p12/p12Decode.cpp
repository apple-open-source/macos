/*
 * Decode P12 PFX using either C++ P12Coder or public
 * C API (both from SecurityNssPkcs12)
 */

#include <security_pkcs12/pkcs12Coder.h>
#include <stdlib.h>
#include <stdio.h>
#include <Security/cssmtype.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include "p12GetPassKey.h"
#include "p12.h"

/* use this option to debug the P12Coder class directly */
#define P12_DECODE_VIA_CPP		0

/*
 * Print CFString - stored as unicode, we get the C string,
 * print it plus newline
 */
static void printUcStr(
	CFStringRef cfstr)
{
	CFIndex len = CFStringGetLength(cfstr) + 1;
	char *outStr = (char *)malloc(len);
	if(CFStringGetCString(cfstr, outStr, len, kCFStringEncodingASCII)) {
		printf("%s\n", outStr);
	}
	else {
		printf("***Error converting from unicode to ASCII\n");
	}
	free(outStr);
}

static void printDataAsHex(
	CFDataRef d,
	unsigned maxToPrint = 0)		// optional, 0 means print it all
{
	unsigned i;
	bool more = false;
	uint32 len = CFDataGetLength(d);
	const uint8 *cp = CFDataGetBytePtr(d);
	
	if((maxToPrint != 0) && (len > maxToPrint)) {
		len = maxToPrint;
		more = true;
	}	
	for(i=0; i<len; i++) {
		printf("%02X ", ((unsigned char *)cp)[i]);
	}
	if(more) {
		printf("...\n");
	}
	else {
		printf("\n");
	}
}

static void printAlgAsString(
	CSSM_ALGORITHMS alg)
{
	char *s = "unknown alg";
	switch(alg) {
		case CSSM_ALGID_RSA:
			s = "RSA";
			break;
		case CSSM_ALGID_DSA:
			s = "DSA";
			break;
		case CSSM_ALGID_FEE:
			s = "FEE";
			break;
		default:
			break;
	}
	printf("%s\n", s);
}

#if		P12_DECODE_VIA_CPP

/* common bag attrs - friendlyName, localKeyId */
static void printBagAttrs(
	P12SafeBag &bag)
{
	CFStringRef friendlyName = bag.friendlyName();
	if(friendlyName) {
		printf("   friendlyName : ");
		printUcStr(friendlyName);
		CFRelease(friendlyName);
	}

	CFDataRef keyId = bag.localKeyId();
	if(keyId) {
		printf("   localKeyId   : ");
		printDataAsHex(keyId, 16);
		CFRelease(keyId);
	}
	if((friendlyName == NULL) && (keyId == NULL)) {
		printf("   <no attributes found>\n");
	}
}

OSStatus p12Decode(
	const CSSM_DATA &pfx,
	CSSM_CSP_HANDLE cspHand,
	CFStringRef pwd,
	bool verbose,
	unsigned loops)
{
	OSStatus ourRtn;
	
	for(unsigned loop=0; loop<loops; loop++) {
		{
			/* localize scope of coder for malloc test */
			P12Coder coder;
			CFDataRef cfd = CFDataCreate(NULL, pfx.Data, pfx.Length);
			ourRtn = noErr;
			
			try { 
				coder.setCsp(cspHand);
				coder.setMacPassPhrase(pwd);
				coder.decode(cfd);
			}
			catch(...) {
				printf("***decode error\n");
				ourRtn = 1;
			}
			CFRelease(cfd);
			
			try {
				unsigned i;

				unsigned numCerts = coder.numCerts();
				printf("\n%u certs found\n", numCerts);
				for(i=0; i<numCerts; i++) {
					P12CertBag *cert = coder.getCert(i);
					printf("=== Cert %u ===\n", i);
					printBagAttrs(*cert);
					if(verbose) {
						CSSM_DATA &certData = cert->certData();
						printCert(certData.Data, certData.Length,
							CSSM_FALSE);

					}
					printf("\n");
				}
				
				unsigned numCrls = coder.numCrls();
				printf("%u crls  found\n", numCrls);
				for(i=0; i<numCrls; i++) {
					P12CrlBag *crl = coder.getCrl(i);
					printf("=== Crl %u ===\n", i);
					printBagAttrs(*crl);
					if(verbose) {
						CSSM_DATA &crlData = crl->crlData();
						printCrl(crlData.Data, crlData.Length, CSSM_FALSE);

					}
					printf("\n");
				}

				unsigned numKeys = coder.numKeys();
				printf("%u keys  found\n", numKeys);
				for(i=0; i<numKeys; i++) {
					P12KeyBag *key = coder.getKey(i);
					printf("\n=== Key %u ===\n", i);
					printBagAttrs(*key);
					/* fix this up */
					CSSM_KEY_PTR ckey = key->key();
					CSSM_KEYHEADER &hdr = ckey->KeyHeader;
					printf("   Key Alg      : ");
					printAlgAsString(hdr.AlgorithmId);
					printf("   Key Size     : %u bits\n", 
						(unsigned)hdr.LogicalKeySizeInBits);
					printf("\n");
				}

				unsigned numBlobs = coder.numOpaqueBlobs();
				printf("%u blobs found\n", numBlobs);
			} 
			catch(...) {
				printf("***exception extracting fields\n");
				ourRtn = 1;
			}
		}
		if(loops > 1) {
			fpurge(stdin);
			printf("CR to continue: ");
			getchar();
		}
		if(ourRtn) {
			return ourRtn;
		}
	}
	return ourRtn;
}

#else	/* P12_DECODE_VIA_CPP */

/* Normal decode using public API in SecPkcs12.h */

/* common bag attrs - friendlyName, localKeyId */
static void printBagAttrs(
	CFStringRef friendlyName,
	CFDataRef localKeyId)
{
	if(friendlyName) {
		printf("   friendlyName : ");
		printUcStr(friendlyName);
	}

	if(localKeyId) {
		printf("   localKeyId   : ");
		printDataAsHex(localKeyId, 20);
	}
	if((friendlyName == NULL) && (localKeyId == NULL)) {
		printf("   <no attributes found>\n");
	}
}

/* release attrs if present */
static void releaseAttrs(
	CFStringRef friendlyName,
	CFDataRef localKeyId,
	SecPkcs12AttrsRef attrs)
{
	if(friendlyName) {
		CFRelease(friendlyName);
	}
	if(localKeyId) {
		CFRelease(localKeyId);
	}
	if(attrs) {
		SecPkcs12AttrsRelease(attrs);
	}
}
	
static void printOsError(
	const char *op,
	OSStatus ortn)
{
	/* may want to parse out CSSM errors */
	cssmPerror(op, ortn);
	printf("\n");
}

/* Sec calls all return 1 - not the fault of SecNssPkcs12 */
#define GET_CERTS_WORKING	1

OSStatus p12Decode(
	const CSSM_DATA &pfx,
	CSSM_CSP_HANDLE cspHand,
	CFStringRef pwd,			// explicit passphrase, mutually exclusive with...
	bool usePassKey,			// use SECURE_PASSPHRASE key
	bool verbose,
	unsigned loops)
{
	OSStatus ortn;
	CSSM_KEY		passKey;
	CSSM_KEY_PTR	passKeyPtr = NULL;
	
	CFDataRef cfd = CFDataCreate(NULL, pfx.Data, pfx.Length);
	if(usePassKey) {
		ortn = p12GetPassKey(cspHand, GPK_Decode, true, &passKey);
		if(ortn) {
			return ortn;
		}
		passKeyPtr = &passKey;
	}
	for(unsigned loop=0; loop<loops; loop++) {
		SecPkcs12CoderRef coder;
		ortn = SecPkcs12CoderCreate(&coder);
		if(ortn) {
			printOsError("SecPkcs12CoderCreate", ortn);
			return ortn;
		}

		ortn = SecPkcs12SetCspHandle(coder, cspHand);
		if(ortn) {
			printOsError("SecPkcs12SetCspHandle", ortn);
			return ortn;
		}
		
		if(usePassKey) {
			ortn = SecPkcs12SetMACPassKey(coder, passKeyPtr);
			if(ortn) {
				printOsError("SecPkcs12SetMACPassKey", ortn);
				return ortn;
			}
		}
		else {
			ortn = SecPkcs12SetMACPassphrase(coder, pwd);
			if(ortn) {
				printOsError("SecPkcs12SetMACPassphrase", ortn);
				return ortn;
			}
		}
		ortn = SecPkcs12Decode(coder, cfd);
		if(ortn) {
			printOsError("SecPkcs12Decode", ortn);
			return ortn;
		}
		
		CFIndex i;
		CFStringRef fname;
		CFDataRef keyId;

		CFIndex numCerts;
		SecPkcs12CertificateCount(coder, &numCerts);
		printf("\n=== %ld certs found ===\n", numCerts);
		#if GET_CERTS_WORKING
		for(i=0; i<numCerts; i++) {
			SecCertificateRef secCert;
			ortn = SecPkcs12CopyCertificate(coder,
				i,
				&secCert,
				&fname,
				&keyId,
				NULL);		// attrs
			if(ortn) {
				printOsError("SecPkcs12CopyCertificate", ortn);
				return ortn;
			}
			printf("Cert %ld:\n", i);
			printBagAttrs(fname, keyId);
			if(verbose) {
				CSSM_DATA certData;
				ortn = SecCertificateGetData(secCert, &certData);
				if(ortn) {
					printOsError("SecCertificateGetData", ortn);
					return ortn;
				}
				printCert(certData.Data, certData.Length,
					CSSM_FALSE);

			}
			releaseAttrs(fname, keyId, NULL);
			CFRelease(secCert);
			printf("\n");
		}
		#endif	/* GET_CERTS_WORKING */
		
		CFIndex numCrls;
		SecPkcs12CrlCount(coder, &numCrls);
		printf("=== %ld crls  found ===\n", numCrls);
		for(i=0; i<numCrls; i++) {
			CFDataRef crl;
			ortn = SecPkcs12CopyCrl(coder,
				i,
				&crl,
				&fname,
				&keyId,
				NULL);		// attrs
			if(ortn) {
				printOsError("SecPkcs12CopyCrl", ortn);
				return ortn;
			}
			printf("Crl %ld:\n", i);
			printBagAttrs(fname, keyId);
			if(verbose) {
				const CSSM_DATA crlData = {
					CFDataGetLength(crl),
					(uint8 *)CFDataGetBytePtr(crl) };
				printCrl(crlData.Data, crlData.Length, CSSM_FALSE);

			}
			releaseAttrs(fname, keyId, NULL);
			CFRelease(crl);
			printf("\n");
		}

		CFIndex numKeys;
		SecPkcs12PrivateKeyCount(coder, &numKeys);
		printf("=== %ld keys  found ===\n", numKeys);
		for(i=0; i<numKeys; i++) {
			CSSM_KEY_PTR key;
			ortn = SecPkcs12GetCssmPrivateKey(coder,
				i,
				&key,
				&fname,
				&keyId,
				NULL);
				
			printf("Key %ld:\n", i);
			printBagAttrs(fname, keyId);
			/* fix this up */
			CSSM_KEYHEADER &hdr = key->KeyHeader;
			printf("   Key Alg      : ");
			printAlgAsString(hdr.AlgorithmId);
			printf("   Key Size     : %u bits\n", 
				(unsigned)hdr.LogicalKeySizeInBits);
			printf("\n");
			releaseAttrs(fname, keyId, NULL);
		}

		CFIndex numBlobs;
		SecPkcs12OpaqueBlobCount(coder, &numBlobs);
		if(numBlobs != 0) {
			printf("%ld blobs found\n", numBlobs);
		}

		/* this should free all memory allocated in the decode */
		SecPkcs12CoderRelease(coder);
		
		if(loops > 1) {
			fpurge(stdin);
			printf("CR to continue: ");
			getchar();
		}
	}
	return 0;
}


#endif	/* P12_DECODE_VIA_CPP */
