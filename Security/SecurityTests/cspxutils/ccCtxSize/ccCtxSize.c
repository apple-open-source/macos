/* 
 * measure and report min context sizes for all CCCryptor ops and algorithms. 
 */
 
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

static void report(
	const char *name,
	CCOperation op,
	CCAlgorithm alg,
	size_t definedSize)
{
	char key[4];
	size_t cryptorLength = 0;
	CCCryptorStatus crtn;
	CCCryptorRef cryptorRef;
	char buf[1];
	
	crtn = CCCryptorCreateFromData(op, alg, 0, key, 4, NULL,
		buf, 1, &cryptorRef, &cryptorLength);
	switch(crtn) {
		case kCCSuccess:
			printf("***Unuexpected success on CCCryptorCreate()\n");
			return;
		case kCCBufferTooSmall:
			break;
		default:
			printf("***Unexpected result on CCCryptorCreate: expect %d got %d\n",
				(int)kCCBufferTooSmall, (int)crtn);
			return;
	}
	printf("%s : %lu bytes\n", name, (unsigned long)cryptorLength);
	if(definedSize < cryptorLength) {
		printf("***Defined context size (%u) is less than reported!\n", 
			(unsigned)definedSize);
	}
}

int main(int argc, char **argv)
{
	report("kCCAlgorithmAES128  ", kCCEncrypt, kCCAlgorithmAES128,	kCCContextSizeAES128);
	report("kCCAlgorithmDES     ", kCCEncrypt, kCCAlgorithmDES,		kCCContextSizeDES);
	report("kCCAlgorithm3DES    ", kCCEncrypt, kCCAlgorithm3DES,	kCCContextSize3DES);
	report("kCCAlgorithmCAST    ", kCCEncrypt, kCCAlgorithmCAST,	kCCContextSizeCAST);
	report("kCCAlgorithmRC4     ", kCCEncrypt, kCCAlgorithmRC4,		kCCContextSizeRC4);
	return 0;
}

/*

sizeof(CCCryptor) = 24 including the spiCtx[] array
sizeof(struct _CCCryptContext) = 60 including algCtx[]
sizeof(DES_key_schedule) = 128
sizeof(DES3_Schedule) = 384
sizeof(GAesKey) = 516
sizeof(_ccHmacContext) = 180

*/
