//
//  EncryptionTest.mm
//  CommonCrypto
//
//  Created by Jim Murphy on 1/13/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import "EncryptionTest.h"

/* Copyright 2006 Apple Computer, Inc.
 *
 * ccSymTest.c - test CommonCrypto symmetric encrypt/decrypt.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "CommonCryptor.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "RandomNumberService.h"

int DoTesting(CCEncryptionTest* unitTest);

/*
 * Defaults.
 */
#define LOOPS_DEF		500
#define MIN_DATA_SIZE	8
#define MAX_DATA_SIZE	10000						/* bytes */
#define MAX_KEY_SIZE	kCCKeySizeMaxRC4			/* bytes */
#define MAX_BLOCK_SIZE	kCCBlockSizeAES128			/* bytes */
#define LOOP_NOTIFY		250

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_AES_128 = 1,	/* 128 bit block, 128 bit key */
	ALG_AES_192,		/* 128 bit block, 192 bit key */
	ALG_AES_256,		/* 128 bit block, 256 bit key */
	ALG_DES,
	ALG_3DES,
	ALG_CAST,
	ALG_RC4,
	/* these aren't in CommonCrypto (yet?) */
	ALG_RC2,
	ALG_RC5,
	ALG_BFISH,
	ALG_ASC,
	ALG_NULL					/* normally not used */
} SymAlg;
#define ALG_FIRST			ALG_AES_128
#define ALG_LAST			ALG_RC4


@implementation CCEncryptionTest

@synthesize encrAlg = _encrAlg;
@synthesize blockSize = _blockSize;
@synthesize minKeySizeInBytes = _minKeySizeInBytes;
@synthesize maxKeySizeInBytes = _maxKeySizeInBytes;
@synthesize ctxSize = _ctxSize;
@synthesize algName = _algName;
@synthesize testObject = _testObject;
@synthesize testPassed = _testPassed;


+ (NSArray *)setupEncryptionTests:(id<TestToolProtocol>)testObject
{
	NSMutableArray* result = [NSMutableArray array];
	
	// ============================= DES ==========================
	
	CCEncryptionTest* desTest = [[[CCEncryptionTest alloc]
		initWithEncryptionName:@"DES"
		withEncryptionAlgo:kCCAlgorithmDES
		withBlockSize:kCCBlockSizeDES 
		withMinKeySizeInBytes:kCCKeySizeDES
		withMaxKeySizeInBytes:kCCKeySizeDES
		withContextSize:kCCContextSizeDES
		withUnitTest:testObject
		] autorelease];
		
	[result addObject:desTest];
	
	// ============================= 3DES =========================
	
	CCEncryptionTest* des3Test = [[[CCEncryptionTest alloc]
		initWithEncryptionName:@"3DES"
		withEncryptionAlgo:kCCAlgorithm3DES 
		withBlockSize:kCCBlockSize3DES 
		withMinKeySizeInBytes:kCCKeySize3DES
		withMaxKeySizeInBytes:kCCKeySize3DES
		withContextSize:kCCContextSize3DES
		withUnitTest:testObject
		] autorelease];
		
	[result addObject:des3Test];
	
	// ============================ AES128 =========================
	
	CCEncryptionTest* aes128Test = [[[CCEncryptionTest alloc]
		initWithEncryptionName:@"AES128"
		withEncryptionAlgo:kCCAlgorithmAES128 
		withBlockSize:kCCBlockSizeAES128 
		withMinKeySizeInBytes:kCCKeySizeAES128
		withMaxKeySizeInBytes:kCCKeySizeAES128
		withContextSize:kCCContextSizeAES128
		withUnitTest:testObject
		] autorelease];
		
	[result addObject:aes128Test];
	
	// ============================ AES192 =========================
	
	CCEncryptionTest* aes192Test = [[[CCEncryptionTest alloc]
		initWithEncryptionName:@"AES192"
		withEncryptionAlgo:kCCAlgorithmAES128 
		withBlockSize:kCCBlockSizeAES128 
		withMinKeySizeInBytes:kCCKeySizeAES192
		withMaxKeySizeInBytes:kCCKeySizeAES192
		withContextSize:kCCContextSizeAES128
		withUnitTest:testObject
		] autorelease];
		
	[result addObject:aes192Test];
	
	// ============================ AES256 =========================
	
	CCEncryptionTest* aes256Test = [[[CCEncryptionTest alloc]
		initWithEncryptionName:@"AES256"
		withEncryptionAlgo:kCCAlgorithmAES128 
		withBlockSize:kCCBlockSizeAES128 
		withMinKeySizeInBytes:kCCKeySizeAES256
		withMaxKeySizeInBytes:kCCKeySizeAES256
		withContextSize:kCCContextSizeAES128
		withUnitTest:testObject
		] autorelease];
		
	[result addObject:aes256Test];
	
	// ============================= CAST ==========================
	
	CCEncryptionTest* castTest = [[[CCEncryptionTest alloc]
		initWithEncryptionName:@"CAST"
		withEncryptionAlgo:kCCAlgorithmCAST 
		withBlockSize:kCCBlockSizeCAST 
		withMinKeySizeInBytes:kCCKeySizeMinCAST
		withMaxKeySizeInBytes:kCCKeySizeMaxCAST
		withContextSize:kCCContextSizeCAST
		withUnitTest:testObject
		] autorelease];
		
	[result addObject:castTest];
	
	// ============================== RC4 ==========================
	
	CCEncryptionTest* rc4Test = [[[CCEncryptionTest alloc]
		initWithEncryptionName:@"RC4"
		withEncryptionAlgo:kCCAlgorithmRC4 
		withBlockSize:0 
		withMinKeySizeInBytes:kCCKeySizeMinRC4
		withMaxKeySizeInBytes:kCCKeySizeMaxRC4
		withContextSize:kCCContextSizeRC4
		withUnitTest:testObject
		] autorelease];
		
	[result addObject:rc4Test];
	
	return result;
}

- (id)initWithEncryptionName:(NSString *)name 
		withEncryptionAlgo:(CCAlgorithm)encrAlg
		withBlockSize:(uint32)blockSize 
		withMinKeySizeInBytes:(uint32)minKeySizeInBytes
		withMaxKeySizeInBytes:(uint32)maxKeySizeInBytes
		withContextSize:(size_t)ctxSize
		withUnitTest:(id<TestToolProtocol>)testObject
{
	if ((self = [super init]))
	{
		_encrAlg = encrAlg;
		_blockSize = blockSize;
		_minKeySizeInBytes = minKeySizeInBytes;
		_maxKeySizeInBytes = maxKeySizeInBytes;
		_ctxSize = ctxSize;
		_algName = [name copy];
		_testObject = [testObject retain];
		_testPassed = YES;
	}
	return self;
}

- (void)dealloc
{
	[_algName release];
	[_testObject release];
	[super dealloc];
}

- (void)doAssertTest:(BOOL)result errorString:(NSString *)errorStr
{
	if (nil != self.testObject)
	{
		[self.testObject doAssertTest:result errorString:errorStr];
		return;
	}
	
	if (_testPassed)
	{
		_testPassed = result;
	}
}



- (void)runTest
{
	int iResult = DoTesting(self);
	[self doAssertTest:(iResult == 0)  errorString:[NSString stringWithFormat:@"%@ test failed", _algName]];
	
	if (nil == _testObject)
	{
		printf("EncryptionTest: %s\n", (self.testPassed) ? "Passed" : "Failed");
	}
}

@end


static void appGetRandomBytes(void* dest, uint32 numBytes)
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	NSData* tempResult = [[CCRandomNumberService defaultRandomNumberService] generateRandomDataOfSize:numBytes];
	memcpy(dest, [tempResult bytes], numBytes);
	[pool drain];
}


static void printCCError(NSString* str, CCCryptorStatus crtn, id<TestToolProtocol> unitTest)
{
	NSString* errStr;
	
	switch(crtn) 
	{
		case kCCSuccess: errStr = @"kCCSuccess"; break;
		case kCCParamError: errStr = @"kCCParamError"; break;
		case kCCBufferTooSmall: errStr = @"kCCBufferTooSmall"; break;
		case kCCMemoryFailure: errStr = @"kCCMemoryFailure"; break;
		case kCCAlignmentError: errStr = @"kCCAlignmentError"; break;
		case kCCDecodeError: errStr = @"kCCDecodeError"; break;
		case kCCUnimplemented: errStr = @"kCCUnimplemented"; break;
		default:
			errStr = [NSString stringWithFormat:@"Unknown(%ld)", (long)crtn];
			break;
	}
	NSString* outputStr = nil;
	if (NULL != str)
	{
		outputStr = [NSString stringWithFormat:@"%@ %@", str, errStr];
	}
	else
	{
		outputStr = errStr;
	}
	
	[unitTest doAssertTest:NO errorString:outputStr];
}

/* max context size */
#define CC_MAX_CTX_SIZE	kCCContextSizeRC4

/* 
 * We write a marker at end of expected output and at end of caller-allocated 
 * CCCryptorRef, and check at the end to make sure they weren't written 
 */
#define MARKER_LENGTH	8
#define MARKER_BYTE		0x7e

/* 
 * Test harness for CCCryptor with lots of options. 
 */
CCCryptorStatus doCCCrypt(
	bool forEncrypt,
	CCAlgorithm encrAlg,			
	bool doCbc,
	bool doPadding,
	const void *keyBytes, size_t keyLen,
	const void *iv,
	bool randUpdates,
	bool inPlace,								/* !doPadding only */
	size_t ctxSize,								/* if nonzero, we allocate ctx */
	bool askOutSize,
	const uint8_t *inText, size_t inTextLen,
	uint8_t **outText, size_t *outTextLen, /* both returned, WE malloc */
	id<TestToolProtocol> unitTest)		
{
	CCCryptorRef	cryptor = NULL;
	CCCryptorStatus crtn;
	CCOperation		op = forEncrypt ? kCCEncrypt : kCCDecrypt;
	CCOptions		options = 0;
	uint8_t			*outBuf = NULL;			/* mallocd output buffer */
	uint8_t			*outp;					/* running ptr into outBuf */
	const uint8		*inp;					/* running ptr into inText */
	size_t			outLen;					/* bytes remaining in outBuf */
	size_t			toMove;					/* bytes remaining in inText */
	size_t			thisMoveOut;			/* output from CCCryptUpdate()/CCCryptFinal() */
	size_t			outBytes;				/* total bytes actually produced in outBuf */
	char			ctx[CC_MAX_CTX_SIZE];	/* for CCCryptorCreateFromData() */
	uint8_t			*textMarker = NULL;		/* 8 bytes of marker here after expected end of 
											 * output */
	char			*ctxMarker = NULL;		/* ditto for caller-provided context */
	unsigned		dex;
	size_t			askedOutSize;			/* from the lib */
	size_t			thisOutLen;				/* dataOutAvailable we use */
	
	
	if(!doCbc) 
	{
		options |= kCCOptionECBMode;
	}
	
	if(doPadding) 
	{
		options |= kCCOptionPKCS7Padding;
	}
	
	/* just hack this one */
	outLen = inTextLen;
	if(forEncrypt) 
	{
		outLen += MAX_BLOCK_SIZE;
	}
	
	outBuf = (uint8_t *)malloc(outLen + MARKER_LENGTH);
	
	/* library should not touch this memory */
	textMarker = outBuf + outLen;
	memset(textMarker, MARKER_BYTE, MARKER_LENGTH);
	
	/* subsequent errors to errOut: */

	if(inPlace) 
	{
		memmove(outBuf, inText, inTextLen);
		inp = outBuf;
	}
	else 
	{
		inp = inText;
	}

	if(!randUpdates) 
	{
		/* one shot */
		if(askOutSize) 
		{
			crtn = CCCrypt(op, encrAlg, options,
				keyBytes, keyLen, iv,
				inp, inTextLen,
				outBuf, 0, &askedOutSize);
			if(crtn != kCCBufferTooSmall) 
			{
				NSString* errStr = [NSString stringWithFormat:@"CCCrypt: ***Did not get kCCBufferTooSmall as expected\n  alg %d inTextLen %lu cbc %d padding %d keyLen %lu",
					(int)encrAlg, (unsigned long)inTextLen, (int)doCbc, (int)doPadding,
					(unsigned long)keyLen];
					
				printCCError(errStr, crtn, unitTest);
				crtn = -1;
				goto errOut;
			}
			outLen = askedOutSize;
		}
		crtn = CCCrypt(op, encrAlg, options,
			keyBytes, keyLen, iv,
			inp, inTextLen,
			outBuf, outLen, &outLen);
		if(crtn) 
		{
			printCCError(@"CCCrypt", crtn, unitTest);
			goto errOut;
		}
        
        [unitTest doAssertTest:(outLen != 0) errorString:@"output length should be non-zero for encryption operation"];
		*outText = outBuf;
		*outTextLen = outLen;
		goto errOut;
	}
	
	/* random multi updates */
	if(ctxSize) {
		size_t ctxSizeCreated;
		
		if(askOutSize) 
		{
			crtn = CCCryptorCreateFromData(op, encrAlg, options,
				keyBytes, keyLen, iv,
				ctx, 0 /* ctxSize */,
				&cryptor, &askedOutSize);
			if(crtn != kCCBufferTooSmall) 
			{
				printCCError(@"CCCryptorCreateFromData: ***Did not get kCCBufferTooSmall as expected", crtn, unitTest);
				crtn = -1;
				goto errOut;
			}
			ctxSize = askedOutSize;
		}
		crtn = CCCryptorCreateFromData(op, encrAlg, options,
			keyBytes, keyLen, iv,
			ctx, ctxSize, &cryptor, &ctxSizeCreated);
		if(crtn) 
		{
			printCCError(@"CCCryptorCreateFromData", crtn, unitTest);
			return crtn;
		}
		ctxMarker = ctx + ctxSizeCreated;
		memset(ctxMarker, MARKER_BYTE, MARKER_LENGTH);
	}
	else 
	{
		crtn = CCCryptorCreate(op, encrAlg, options,
			keyBytes, keyLen, iv,
			&cryptor);
		if(crtn) 
		{
			printCCError(@"CCCryptorCreate", crtn, unitTest);
			return crtn;
		}
	}
	
	toMove = inTextLen;		/* total to go */
	outp = outBuf;
	outBytes = 0;			/* bytes actually produced in outBuf */
	
	while(toMove) 
	{
		uint32 thisMoveIn;			/* input to CCryptUpdate() */
		
		thisMoveIn = [[CCRandomNumberService defaultRandomNumberService] 
			generateRandomNumberInRange:1 toMax:toMove];
		if(askOutSize) 
		{
			thisOutLen = CCCryptorGetOutputLength(cryptor, thisMoveIn, false);
		}
		else 
		{
			thisOutLen = outLen;
		}
		crtn = CCCryptorUpdate(cryptor, inp, thisMoveIn,
			outp, thisOutLen, &thisMoveOut);
		if(crtn) 
		{
			printCCError(@"CCCryptorUpdate", crtn, unitTest);
			goto errOut;
		}
		inp			+= thisMoveIn;
		toMove		-= thisMoveIn;
		outp		+= thisMoveOut;
		outLen   	-= thisMoveOut;
		outBytes	+= thisMoveOut;
	}
	
	if(doPadding) 
	{
		/* Final is not needed if padding is disabled */
		if(askOutSize) 
		{
			thisOutLen = CCCryptorGetOutputLength(cryptor, 0, true);
		}
		else 
		{
			thisOutLen = outLen;
		}
		crtn = CCCryptorFinal(cryptor, outp, thisOutLen, &thisMoveOut);
	}
	else 
	{
		thisMoveOut = 0;
		crtn = kCCSuccess;
	}
	
	if(crtn) 
	{
		printCCError(@"CCCryptorFinal", crtn, unitTest);
		goto errOut;
	}
	
	outBytes += thisMoveOut;
	*outText = outBuf;
	*outTextLen = outBytes;
	crtn = kCCSuccess;

	for(dex=0; dex<MARKER_LENGTH; dex++) 
	{
		if(textMarker[dex] != MARKER_BYTE) 
		{
			[unitTest doAssertTest:NO errorString:[NSString stringWithFormat:@"***lib scribbled on our textMarker memory (op=%s)!\n",
				forEncrypt ? "encrypt" : "decrypt"]];
			crtn = (CCCryptorStatus)-1;
		}
	}
	if(ctxSize) 
	{
		for(dex=0; dex<MARKER_LENGTH; dex++) 
		{
			if(ctxMarker[dex] != MARKER_BYTE) 
			{
				[unitTest doAssertTest:NO errorString:[NSString stringWithFormat:@"***lib scribbled on our ctxMarker memory (op=%s)!\n",
					forEncrypt ? "encrypt" : "decrypt"]];
				crtn = (CCCryptorStatus)-1;
			}
		}
	}
	
errOut:
	if(crtn) 
	{
		if(outBuf) 
		{
			free(outBuf);
		}
	}
	if(cryptor) 
	{
		CCCryptorRelease(cryptor);
	}
	return crtn;
}

static int doTest(const uint8_t *ptext,
	size_t ptextLen,
	CCAlgorithm encrAlg,			
	bool doCbc,
	bool doPadding,
	bool nullIV,			/* if CBC, use NULL IV */
	uint32 keySizeInBytes,
	bool stagedEncr,
	bool stagedDecr,
	bool inPlace,	
	size_t ctxSize,		
	bool askOutSize,
	bool quiet,
	id<TestToolProtocol> unitTest)
{
	uint8_t			keyBytes[MAX_KEY_SIZE];
	uint8_t			iv[MAX_BLOCK_SIZE];
	uint8_t			*ivPtrEncrypt;
	uint8_t			*ivPtrDecrypt;
	uint8_t			*ctext = NULL;		/* mallocd by doCCCrypt */
	size_t			ctextLen = 0;
	uint8_t			*rptext = NULL;		/* mallocd by doCCCrypt */
	size_t			rptextLen;
	CCCryptorStatus	crtn;
	int				rtn = 0;
	
	/* random key */
	appGetRandomBytes(keyBytes, keySizeInBytes);
	
	/* random IV if needed */
	if(doCbc) 
	{
		if(nullIV) 
		{
			memset(iv, 0, MAX_BLOCK_SIZE);
			
			/* flip a coin, give one side NULL, the other size zeroes */
			
			if ([[CCRandomNumberService defaultRandomNumberService] 
						generateRandomNumberInRange:1 toMax:2] == 1)
			{
				ivPtrEncrypt = NULL;
				ivPtrDecrypt = iv;
			}
			else 
			{
				ivPtrEncrypt = iv;
				ivPtrDecrypt = NULL;
			}
		}
		else 
		{
			appGetRandomBytes(iv, MAX_BLOCK_SIZE);
			ivPtrEncrypt = iv;
			ivPtrDecrypt = iv;
		}
	}	
	else 
	{
		ivPtrEncrypt = NULL;
		ivPtrDecrypt = NULL;
	}

	crtn = doCCCrypt(true, encrAlg, doCbc, doPadding,
		keyBytes, keySizeInBytes, ivPtrEncrypt,
		stagedEncr, inPlace, ctxSize, askOutSize,
		ptext, ptextLen,
		&ctext, &ctextLen, unitTest);
	if(crtn) 
	{
		rtn = 1;
		goto abort;
	}
		
	crtn = doCCCrypt(false, encrAlg, doCbc, doPadding,
		keyBytes, keySizeInBytes, ivPtrDecrypt,
		stagedDecr, inPlace, ctxSize, askOutSize,
		ctext, ctextLen,
		&rptext, &rptextLen, unitTest);
	if(crtn) 
	{
		rtn = 1;
		goto abort;
	}
	
	/* compare ptext, rptext */
	if(ptextLen != rptextLen) 
	{
		NSString* errStr = [NSString stringWithFormat:@"Ptext length mismatch: expect %lu, got %lu\n", ptextLen, rptextLen];
		[unitTest doAssertTest:NO errorString:errStr];
		rtn = 1;
		goto abort;
		
	}
	
	if(memcmp(ptext, rptext, ptextLen)) 
	{
		[unitTest doAssertTest:NO errorString:@"***data miscompare"];
		rtn = 1;
	}
	
abort:
	if(ctext) 
	{
		free(ctext);
	}
	if(rptext) 
	{
		free(rptext);
	}
	return rtn;
}

bool isBitSet(unsigned bit, unsigned word) 
{
	if(bit > 31) 
	{
		NSLog(@"We don't have that many bits");
		exit(1);
	}
	unsigned mask = 1 << bit;
	return (word & mask) ? true : false;
}

int DoTesting(CCEncryptionTest* unitTest)
{
	unsigned			loop;
	uint8				*ptext;
	size_t				ptextLen;
	bool				stagedEncr;
	bool				stagedDecr;
	bool				doPadding;
	bool				doCbc;
	bool				nullIV;
	const char			*algStr;
	CCAlgorithm			encrAlg;	
	uint32				minKeySizeInBytes;
	uint32				maxKeySizeInBytes;
	uint32				keySizeInBytes;
	int					rtn = 0;
	uint32				blockSize;		// for noPadding case
	size_t				ctxSize;		// always set per alg
	size_t				ctxSizeUsed;	// passed to doTest
	bool				askOutSize;		// inquire output size each op
	
	/*
	 * User-spec'd params
	 */
	bool		keySizeSpec = false;		// false: use rand key size
	unsigned	loops = LOOPS_DEF;
	size_t		minPtextSize = MIN_DATA_SIZE;
	size_t		maxPtextSize = MAX_DATA_SIZE;
	bool		quiet = false;
	bool		paddingSpec = false;		// true: user calls doPadding, const
	bool		cbcSpec = false;			// ditto for doCbc
	bool		stagedSpec = false;			// ditto for stagedEncr and stagedDecr
	bool		inPlace = false;			// en/decrypt in place for ECB
	bool		allocCtxSpec = false;		// use allocCtx
	bool		allocCtx = false;			// allocate context ourself
	

	ptext = (uint8 *)malloc(maxPtextSize);
	if(ptext == NULL) 
	{
		[unitTest doAssertTest:NO errorString:@"Insufficient heap space"];
		exit(1);
	}
	/* ptext length set in test loop */
	
	/* Set up the values for this test from the object */	
	encrAlg = unitTest.encrAlg;
	blockSize = unitTest.blockSize;
	minKeySizeInBytes = unitTest.minKeySizeInBytes;
	maxKeySizeInBytes = unitTest.maxKeySizeInBytes;
	ctxSize = unitTest.ctxSize;
	algStr = [unitTest.algName UTF8String];		
		
	for(loop=1; ; loop++) 
	{
		ptextLen = [[CCRandomNumberService defaultRandomNumberService] 
			generateRandomNumberInRange:minPtextSize toMax:maxPtextSize];
		appGetRandomBytes(ptext, ptextLen);
		
		/* per-loop settings */
		if(!keySizeSpec) 
		{
			if(minKeySizeInBytes == maxKeySizeInBytes) 
			{
				keySizeInBytes = minKeySizeInBytes;
			}
			else 
			{
				keySizeInBytes = [[CCRandomNumberService defaultRandomNumberService] 
					generateRandomNumberInRange:minKeySizeInBytes toMax:maxKeySizeInBytes];
			}
		}
		if(blockSize == 0) 
		{
			/* stream cipher */
			doCbc = false;
			doPadding = false;
		}
		else 
		{
			if(!cbcSpec) 
			{
				doCbc = isBitSet(0, loop);
			}
			if(!paddingSpec) 
			{
				doPadding = isBitSet(1, loop);
			}
		}
		if(!doPadding && (blockSize != 0)) 
		{
			/* align plaintext */
			ptextLen = (ptextLen / blockSize) * blockSize;
			if(ptextLen == 0) 
			{
				ptextLen = blockSize;
			}
		}
		if(!stagedSpec) 
		{
			stagedEncr = isBitSet(2, loop);
			stagedDecr = isBitSet(3, loop);
		}
		if(doCbc) 
		{
			nullIV = isBitSet(4, loop);
		}
		else 
		{
			nullIV = false;
		}
		inPlace = isBitSet(5, loop);
		if(allocCtxSpec) 
		{
			ctxSizeUsed = allocCtx ? ctxSize : 0;
		}
		else if(isBitSet(6, loop)) 
		{
			ctxSizeUsed = ctxSize;
		}
		else 
		{
			ctxSizeUsed = 0;
		}
		askOutSize = isBitSet(7, loop);
		
		
		if(doTest(ptext, ptextLen,
				encrAlg, doCbc, doPadding, nullIV,
				keySizeInBytes,
				stagedEncr,	stagedDecr, inPlace, ctxSizeUsed, askOutSize,
				quiet, unitTest)) 
		{
			rtn = 1;
			break;
		}
		
		if(loops && (loop == loops)) 
		{
			break;
		}
		
		if(rtn) 
		{
			break;
		}
		
	}	/* for algs */
	
	free(ptext);
	return rtn;
}
