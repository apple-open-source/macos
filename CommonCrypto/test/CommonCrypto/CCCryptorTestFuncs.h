/*
 *  CCCryptorTestFuncs.h
 *  CCRegressions
 *
 *
 */

#include "capabilities.h"
#include <CommonCrypto/CommonCryptor.h>
#ifdef CRYPTORWITHMODE
#include <CommonCrypto/CommonCryptorSPI.h>
#else
typedef uint32_t CCMode;
typedef uint32_t CCPadding;
typedef uint32_t CCModeOptions;
#endif

/* This is a CCCrypt with the Updates split into two parts */

CCCryptorStatus 
CCMultiCrypt(CCOperation op, CCAlgorithm alg, CCOptions options, const void *key, size_t keyLength, const void *iv, const void *dataIn, size_t dataInLength,
	void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved);
    
    
/* This is a CCCrypt allowing mode specification */

CCCryptorStatus
CCCryptWithMode(CCOperation op, CCMode mode, CCAlgorithm alg, CCPadding padding, const void *iv, 
				const void *key, size_t keyLength, const void *tweak, size_t tweakLength,
                int numRounds, CCModeOptions options,
                const void *dataIn, size_t dataInLength, 
                void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved);

CCCryptorStatus 
CCMultiCryptWithMode(CCOperation op, CCMode mode, CCAlgorithm alg, CCPadding padding, const void *iv, 
	const void *key, size_t keyLength, const void *tweak, size_t tweakLength,
	int numRounds, CCModeOptions options,
    const void *dataIn, size_t dataInLength,
	void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved);

/* This is a Test Case "doer" using CCCrypt */
int
CCCryptTestCase(char *keyStr, char *ivStr, CCAlgorithm alg, CCOptions options, char *cipherText, char *plainText);

/* This is a Test Case "doer" using CCMultiCrypt */
int
CCMultiCryptTestCase(char *keyStr, char *ivStr, CCAlgorithm alg, CCOptions options, char *cipherText, char *plainText);

/* This is a Test Case "doer" using CCCryptWithMode */
int
CCModeTestCase(char *keyStr, char *ivStr, CCMode mode, CCAlgorithm alg, CCPadding padding, char *cipherText, char *plainText);

/* This is a Test Case "doer" using CCMultiCryptWithMode */
int
CCMultiModeTestCase(char *keyStr, char *ivStr, CCMode mode, CCAlgorithm alg, CCPadding padding, char *cipherText, char *plainText);
/* This is a Test Case "doer" using CCCryptorGCM */
int
CCCryptorGCMTestCase(char *keyStr, char *ivStr, char *aDataStr, char *tagStr, CCAlgorithm alg, char *cipherText, char *plainText);
int
CCCryptorGCMDiscreetTestCase(char *keyStr, char *ivStr, char *aDataStr, char *tagStr, CCAlgorithm alg, char *cipherText, char *plainText);

