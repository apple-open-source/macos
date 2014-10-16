/*
 * CSP symmetric encryption performance measurement tool
 * Based on Michael Brouwer's cryptoPerformance.cpp
 *
 */
#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/wrapkey.h>
#include <string.h>
#include "common.h"
#include <iomanip>
#include <iostream>
#include <memory>
using namespace std;

/*
 * Default values
 */
#define ALG_DEFAULT			CSSM_ALGID_AES
#define ALG_STR_DEFAULT		"AES"
#define CHAIN_DEFAULT		CSSM_TRUE
#define KEY_SIZE_DEFAULT	128

#define BEGIN_FUNCTION try {

#define END_FUNCTION } \
    catch (const CssmError &e) \
    { \
        cssmPerror(__PRETTY_FUNCTION__, e.error); \
    } \
    catch (...) \
    { \
        fprintf(stderr, "%s: failed\n", __PRETTY_FUNCTION__); \
    } \

static void usage(char **argv)
{
	printf("usage: %s iterations bufsize [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (s=ASC; d=DES; 3=3DES; 2=RC2; 4=RC4; 5=RC5;\n");
	printf("     a=AES; b=Blowfish; c=CAST; n=NULL; default=AES)\n");
	printf("   k=keySizeInBits\n");
	printf("   b=blockSizeInBits\n");
	printf("   e (ECB mode; default is CBC)\n");
	printf("   i (re-set IV in each loop)\n");
	printf("   v(erbose)\n");
	printf("   h(elp)\n");
	exit(1);
}

static void
cdsaSetupContexts(int iterations,
	auto_ptr<Security::CssmClient::Encrypt> &encrypt,
	auto_ptr<Security::CssmClient::Decrypt> &decrypt,
	CSSM_ALGORITHMS keyAlg,
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	uint32 keySizeInBits,
	uint32 blockSizeInBits)		// optional
{
	BEGIN_FUNCTION
	Security::CssmClient::CSP csp(gGuidAppleCSP);
	//CssmData keyData((uint8 *)"1234567812345678", 16);
    Security::CssmClient::GenerateKey keyGenerator(csp, keyAlg, keySizeInBits);
    Security::CssmClient::Key key = keyGenerator(Security::CssmClient::KeySpec(
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
        CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE));
	for (int ix=0; ix < iterations; ++ix)
	{
		encrypt.reset(new Security::CssmClient::Encrypt(csp, encrAlg));
		encrypt->mode(encrMode);
		encrypt->key(key);
		if(blockSizeInBits) {
			encrypt->add(CSSM_ATTRIBUTE_BLOCK_SIZE, blockSizeInBits / 8);
		}
		//encrypt->activate();

		decrypt.reset(new Security::CssmClient::Decrypt(csp, encrAlg));
		decrypt->mode(encrMode);
		decrypt->key(key);
		if(blockSizeInBits) {
			decrypt->add(CSSM_ATTRIBUTE_BLOCK_SIZE, blockSizeInBits / 8);
		}
		//decrypt->activate();
	}
	END_FUNCTION
}

static void
cdsaEncrypt(int iterations, Security::CssmClient::Encrypt &encrypt,
	uint8 *inBuf, uint32 bufLen, uint8 *outBuf, bool useIv, uint32 blockSizeBytes,
	CSSM_BOOL resetIv)
{
	BEGIN_FUNCTION
	CssmData iv((uint8 *)"12345678123456781234567812345678", blockSizeBytes);
	CssmData inData(inBuf, bufLen);
	CssmData outData(outBuf, bufLen);
	CssmData nullData(reinterpret_cast<uint8 *>(NULL) + 1, 0);
	if(useIv) {
		encrypt.initVector(iv);
	}
	if(useIv && resetIv) {
		for (int ix=0; ix < iterations; ++ix)
		{
			encrypt.initVector(iv);
			encrypt.encrypt(inData, outData, nullData);
		}
	}
	else {
		for (int ix=0; ix < iterations; ++ix)
		{
			encrypt.encrypt(inData, outData, nullData);
		}
	}
	END_FUNCTION
}

static void
cdsaDecrypt(int iterations, Security::CssmClient::Decrypt &decrypt,
	uint8 *inBuf, uint32 bufLen, uint8 *outBuf, bool useIv, uint32 blockSizeBytes,
	CSSM_BOOL resetIv)
{
	BEGIN_FUNCTION
	CssmData iv((uint8 *)"12345678123456781234567812345678", blockSizeBytes);
	CssmData inData(inBuf, bufLen);
	CssmData outData(outBuf, bufLen);
	CssmData nullData(reinterpret_cast<uint8 *>(NULL) + 1, 0);
	if(useIv) {
		decrypt.initVector(iv);
	}
	if(useIv && resetIv) {
		for (int ix=0; ix < iterations; ++ix)
		{
			decrypt.initVector(iv);
			decrypt.decrypt(inData, outData, nullData);
		}
	}
	else {
		for (int ix=0; ix < iterations; ++ix)
		{
			decrypt.decrypt(inData, outData, nullData);
		}
	}
	END_FUNCTION
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	CSSM_ENCRYPT_MODE	mode;
	uint32				blockSizeBytes = 8;
	
	/*
	 * User-spec'd params
	 */
	CSSM_BOOL			chainEnable = CHAIN_DEFAULT;
	uint32				keySizeInBits = KEY_SIZE_DEFAULT;
	uint32				blockSizeInBits = 0;
	const char			*algStr = ALG_STR_DEFAULT;
	uint32				keyAlg = ALG_DEFAULT;		// CSSM_ALGID_xxx of the key
	uint32				encrAlg = ALG_DEFAULT;		// CSSM_ALGID_xxx for encrypt
	int 				iterations;
	int 				bufSize;
	CSSM_BOOL			resetIv = CSSM_FALSE;
	CSSM_BOOL			verbose = false;
	
	if(argc < 3) {
		usage(argv);
	}
	iterations = atoi(argv[1]);
	bufSize = atoi(argv[2]);
	for(arg=3; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 's':
						encrAlg = keyAlg = CSSM_ALGID_ASC;
						algStr = "ASC";
						break;
					case 'd':
						encrAlg = keyAlg = CSSM_ALGID_DES;
						algStr = "DES";
						keySizeInBits = 64;
						break;
					case '3':
						keyAlg  = CSSM_ALGID_3DES_3KEY;
						encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
						algStr = "3DES";
						keySizeInBits = 64 * 3;
						break;
					case '2':
						encrAlg = keyAlg = CSSM_ALGID_RC2;
						algStr = "RC2";
						break;
					case '4':
						encrAlg = keyAlg = CSSM_ALGID_RC4;
						algStr = "RC4";
						/* not a block cipher */
						chainEnable = CSSM_FALSE;
						break;
					case '5':
						encrAlg = keyAlg = CSSM_ALGID_RC5;
						algStr = "RC5";
						break;
					case 'a':
						encrAlg = keyAlg = CSSM_ALGID_AES;
						algStr = "AES";
						break;
					case 'b':
						encrAlg = keyAlg = CSSM_ALGID_BLOWFISH;
						algStr = "Blowfish";
						break;
					case 'c':
						encrAlg = keyAlg = CSSM_ALGID_CAST;
						algStr = "CAST";
						break;
					case 'n':
						encrAlg = keyAlg = CSSM_ALGID_NONE;
						algStr = "NULL";
						break;
					default:
						usage(argv);
				}
				break;
		    case 'k':
		    	keySizeInBits = atoi(&argp[2]);
				break;
		    case 'b':
		    	blockSizeInBits = atoi(&argp[2]);
				break;
			case 'e':
				chainEnable = CSSM_FALSE;
				break;
			case 'i':
				resetIv = CSSM_TRUE;
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	switch(keyAlg) {
		case CSSM_ALGID_RC4:
			chainEnable = CSSM_FALSE;
			mode = CSSM_ALGMODE_NONE;
			if((iterations & 1) == 0) {
				printf("***WARNING: an even number of iterations with RC4 results in\n"
					"   identical plaintext and ciphertext!\n");
			}
			break;
		case CSSM_ALGID_AES:
		case CSSM_ALGID_NONE:
			blockSizeBytes = blockSizeInBits ? (blockSizeInBits / 8) : 16;
			break;
		default:
			break;
	}
	if(chainEnable) {
		mode = CSSM_ALGMODE_CBC_IV8;
	}
	else {
		mode = CSSM_ALGMODE_ECB;
	}
	
	if(blockSizeInBits) {
		printf("Algorithm: %s   keySize: %u  blockSize: %u  mode: %s"
			"  iterations: %d  bufSize %d\n",
			algStr, (unsigned)keySizeInBits, (unsigned)blockSizeInBits, 
			chainEnable ? "CBC" : "ECB", 
			iterations, bufSize);
	}
	else {
		printf("Algorithm: %s   keySize: %u  mode: %s  iterations: %d  "
			"bufSize %d\n",
			algStr, (unsigned)keySizeInBits, chainEnable ? "CBC" : "ECB", 
			iterations, bufSize);
	}
	CFAbsoluteTime start, end;
	auto_array<uint8> buffer(bufSize), plain(bufSize);
	auto_ptr<Security::CssmClient::Encrypt> encrypt(NULL);
	auto_ptr<Security::CssmClient::Decrypt> decrypt(NULL);

	uint8 *bp = buffer.get();
	for(int ix=0; ix<bufSize; ix++) {
		*bp++ = random();
	}
	memcpy(plain.get(), buffer.get(), bufSize);

	if(verbose) {
		printf("%d * cdsaSetupContexts", iterations);
	}
	fflush(stdout);
	start = CFAbsoluteTimeGetCurrent();
	cdsaSetupContexts(iterations, encrypt, decrypt,
		keyAlg, encrAlg, mode, keySizeInBits, blockSizeInBits);
	end = CFAbsoluteTimeGetCurrent();
	if(verbose) {
		printf(" took: %gs\n", end - start);
	}

	printf("  %d * cdsaEncrypt %d bytes", iterations, bufSize);
	fflush(stdout);
	start = CFAbsoluteTimeGetCurrent();
	cdsaEncrypt(iterations, *encrypt.get(), buffer.get(), bufSize, buffer.get(), 
		chainEnable, blockSizeBytes, resetIv);
	end = CFAbsoluteTimeGetCurrent();
	printf(" took: %gs %.1f Kbytes/s\n", end - start,
		(iterations * bufSize) / (end - start) / 1024.0);

	if (!memcmp(buffer.get(), plain.get(), bufSize))
		printf("*** ciphertext matches plaintext ***\n");

	printf("  %d * cdsaDecrypt %d bytes", iterations, bufSize);
	fflush(stdout);
	start = CFAbsoluteTimeGetCurrent();
	cdsaDecrypt(iterations, *decrypt.get(), buffer.get(), bufSize, buffer.get(), 
		chainEnable, blockSizeBytes, resetIv);
	end = CFAbsoluteTimeGetCurrent();
	printf(" took: %gs %.1f Kbytes/s\n", end - start,
		(iterations * bufSize) / (end - start) / 1024.0);

	if (memcmp(buffer.get(), plain.get(), bufSize))
		printf("*** plaintext not recovered correctly ***\n");
	/*
	else
		printf("plaintext recovered\n");
	*/
	return 0;
}
