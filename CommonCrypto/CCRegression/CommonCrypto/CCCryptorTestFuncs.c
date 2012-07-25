/*
 *  CCCryptorTestFuncs.c
 *  CCRegressions
 *
 *
 */


#include <stdio.h>
#include "CCCryptorTestFuncs.h"
#include "testbyteBuffer.h"
#include "testmore.h"
#include "capabilities.h"


CCCryptorStatus
CCCryptWithMode(CCOperation op, CCMode mode, CCAlgorithm alg, CCPadding padding, const void *iv, 
				const void *key, size_t keyLength, const void *tweak, size_t tweakLength,
                int numRounds, CCModeOptions options,
                const void *dataIn, size_t dataInLength, 
                void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved)
#ifdef CRYPTORWITHMODE
{
    CCCryptorRef cref;
	CCCryptorStatus retval;
    size_t moved;

   	if((retval = CCCryptorCreateWithMode(op, mode, alg, padding, iv, key, keyLength, tweak, tweakLength, numRounds, options, &cref)) != kCCSuccess) {
    	return retval;
    }
    
    if((retval = CCCryptorUpdate(cref, dataIn, dataInLength, dataOut, dataOutAvailable, &moved)) != kCCSuccess) {
    	return retval;
    }
    
    dataOut += moved;
    dataOutAvailable -= moved;
    *dataOutMoved = moved;
    
    if((retval = CCCryptorFinal(cref, dataOut, dataOutAvailable, &moved)) != kCCSuccess) {
    	return retval;
    }
    
    *dataOutMoved += moved;

	CCCryptorRelease(cref);
    
    return kCCSuccess;
}
#else
{
    return kCCSuccess;
}
#endif



CCCryptorStatus 
CCMultiCrypt(CCOperation op, CCAlgorithm alg, CCOptions options, const void *key, size_t keyLength, const void *iv, const void *dataIn, size_t dataInLength,
	void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved)
{
	CCCryptorRef cref;
    CCCryptorStatus retval;
    size_t p1, p2;
    size_t newmoved;
    size_t finalSize;
    
    retval = CCCryptorCreate(op, alg, options, key, keyLength, iv, &cref);
    if(retval != kCCSuccess) {
    	diag("Cryptor Create Failed\n");
    	return retval;
    }
    p1 = ( dataInLength / 16 ) * 16 - 1;
    if(p1 > 16) p1 = dataInLength;
    p2 = dataInLength - p1;
    // diag("Processing length %d  in two parts %d and %d\n", (int) dataInLength, (int) p1, (int) p2);
    
    *dataOutMoved = 0;
    
    if(p1) {
    	retval = CCCryptorUpdate(cref, dataIn, p1, dataOut, dataOutAvailable, dataOutMoved);
        if(retval) {
        	diag("P1 - Tried to move %d - failed retval = %d\n", (int) p1, (int) retval);
            return retval;
        }
        dataIn += p1;
        dataOut += *dataOutMoved;
        dataOutAvailable -= *dataOutMoved;        
    }
    if(p2) {
        
    	retval = CCCryptorUpdate(cref, dataIn, p2, dataOut, dataOutAvailable, &newmoved);
        if(retval) {
        	diag("P2 - Tried to move %d - failed\n", (int) p2);
            return retval;
        }
        dataIn += p2;
        dataOut += newmoved;        
        dataOutAvailable -= newmoved;
        *dataOutMoved += newmoved;
    }
    
    /* We've had reports that Final fails on some platforms if it's only cipher blocksize.  */
    switch(alg) {
    case kCCAlgorithmDES: /* fallthrough */
    case kCCAlgorithm3DES: finalSize = kCCBlockSizeDES; break;
    case kCCAlgorithmAES128: finalSize = kCCBlockSizeAES128; break;
    case kCCAlgorithmCAST: finalSize = kCCBlockSizeCAST; break;
    case kCCAlgorithmRC2: finalSize = kCCBlockSizeRC2; break;
    default: finalSize = dataOutAvailable;
    }
    
    retval = CCCryptorFinal(cref, dataOut, finalSize, &newmoved);
    if(retval) {
        diag("Final - failed %d\n", (int) retval);
        return retval;
    }
    retval = CCCryptorRelease(cref);
    if(retval) {
        diag("Final - release failed %d\n", (int) retval);
        return retval;
    }
    *dataOutMoved += newmoved;
    return kCCSuccess;
    
    
}

CCCryptorStatus 
CCMultiCryptWithMode(CCOperation op, CCMode mode, CCAlgorithm alg, CCPadding padding, const void *iv, 
	const void *key, size_t keyLength, const void *tweak, size_t tweakLength,
	int numRounds, CCModeOptions options,
    const void *dataIn, size_t dataInLength,
	void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved)
#ifdef CRYPTORWITHMODE
{
	CCCryptorRef cref;
    CCCryptorStatus retval;
    size_t p1, p2;
    size_t newmoved;
    
   	if((retval = CCCryptorCreateWithMode(op, mode, alg, padding, iv, key, keyLength, tweak, tweakLength, numRounds, options, &cref)) != kCCSuccess) {
    	return retval;
    }
    p1 = ( dataInLength / 16 ) * 16 - 1;
    if(p1 > 16) p1 = dataInLength;
    p2 = dataInLength - p1;
    // diag("Processing length %d  in two parts %d and %d\n", (int) dataInLength, (int) p1, (int) p2);
    
    *dataOutMoved = 0;
    
    if(p1) {
    	retval = CCCryptorUpdate(cref, dataIn, p1, dataOut, dataOutAvailable, dataOutMoved);
        if(retval) {
        	diag("P1 - Tried to move %d - failed retval = %d\n", (int) p1, (int) retval);
            return retval;
        }
        dataIn += p1;
        dataOut += *dataOutMoved;
        dataOutAvailable -= *dataOutMoved;        
    }
    if(p2) {
        
    	retval = CCCryptorUpdate(cref, dataIn, p2, dataOut, dataOutAvailable, &newmoved);
        if(retval) {
        	diag("P2 - Tried to move %d - failed\n", (int) p2);
            return retval;
        }
        dataIn += p2;
        dataOut += newmoved;        
        dataOutAvailable -= newmoved;
        *dataOutMoved += newmoved;
    }
    retval = CCCryptorFinal(cref, dataOut, dataOutAvailable, &newmoved);
    if(retval) {
        diag("Final - failed %d\n", (int) retval);
        return retval;
    }
    retval = CCCryptorRelease(cref);
    if(retval) {
        diag("Final - release failed %d\n", (int) retval);
        return retval;
    }
    *dataOutMoved += newmoved;
    return kCCSuccess;
}
#else
{
    return kCCSuccess;
}
#endif


static byteBuffer
ccConditionalTextBuffer(char *inputText)
{
	byteBuffer ret;
    
    if(inputText) ret = hexStringToBytes(inputText);
    else {
    	ret = hexStringToBytes("");
        ret->bytes = NULL;
    }
    return ret;
}

int
CCCryptTestCase(char *keyStr, char *ivStr, CCAlgorithm alg, CCOptions options, char *cipherText, char *plainText)
{
    byteBuffer key, iv;
    byteBuffer pt, ct;
    
    
	CCCryptorStatus retval;
    char cipherDataOut[4096];
    char plainDataOut[4096];
    size_t dataOutMoved;
    byteBuffer bb;
            
    key = hexStringToBytes(keyStr);        
    pt = ccConditionalTextBuffer(plainText);
    ct = ccConditionalTextBuffer(cipherText);
    iv = ccConditionalTextBuffer(ivStr);
    
        
    if((retval = CCCrypt(kCCEncrypt, alg, options, key->bytes, key->len, iv->bytes, pt->bytes, pt->len, cipherDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	diag("Encrypt Failed %d\n", retval);
        return 1;
    }
    
    bb = bytesToBytes(cipherDataOut, dataOutMoved);    	

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(!ct->bytes) {
    	diag("Input Length %d Result: %s\n", (int) pt->len, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(ct, bb)) {
            diag("FAIL Encrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
        	return 1;
        }
    }
    
    free(bb);
    
    if((retval = CCCrypt(kCCDecrypt, alg, options, key->bytes, key->len, iv->bytes, cipherDataOut, dataOutMoved, plainDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	diag("Decrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(plainDataOut, dataOutMoved);
    
	if (!bytesAreEqual(pt, bb)) {
        diag("FAIL Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(pt));
        return 1;
    }
    
    // if(ct->bytes && iv->bytes) diag("PASS Test for length %d\n", (int) pt->len);
    // if(ct && (iv->bytes == NULL)) diag("PASS NULL IV Test for length %d\n", (int) pt->len);

    free(pt);
    free(ct);
    free(key);
    free(iv);
	return 0;
}




int
CCMultiCryptTestCase(char *keyStr, char *ivStr, CCAlgorithm alg, CCOptions options, char *cipherText, char *plainText)
{
    byteBuffer key, iv;
    byteBuffer pt, ct;
    
    
	CCCryptorStatus retval;
    char cipherDataOut[4096];
    char plainDataOut[4096];
    size_t dataOutMoved;
    byteBuffer bb;
            
    key = hexStringToBytes(keyStr);        
    pt = ccConditionalTextBuffer(plainText);
    ct = ccConditionalTextBuffer(cipherText);
    iv = ccConditionalTextBuffer(ivStr);
    
        
    if((retval = CCMultiCrypt(kCCEncrypt, alg, options, key->bytes, key->len, iv->bytes, pt->bytes, pt->len, cipherDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	diag("Encrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(cipherDataOut, dataOutMoved);    	

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(!ct->bytes) {
    	diag("Input Length %d Result: %s\n", (int) pt->len, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(ct, bb)) {
            diag("FAIL Encrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
        	return 1;
        }
    }
    
    free(bb);
    
    if((retval = CCMultiCrypt(kCCDecrypt, alg, options, key->bytes, key->len, iv->bytes, cipherDataOut, dataOutMoved, plainDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	diag("Decrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(plainDataOut, dataOutMoved);
    
	if (!bytesAreEqual(pt, bb)) {
        diag("FAIL Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(pt));
        return 1;
    }
    
    // if(ct && iv->bytes) diag("PASS Test for length %d\n", (int) pt->len);
    // if(ct && (iv->bytes == NULL)) diag("PASS NULL IV Test for length %d\n", (int) pt->len);

    free(pt);
    free(ct);
    free(key);
    free(iv);
	return 0;
}




int
CCModeTestCase(char *keyStr, char *ivStr, CCMode mode, CCAlgorithm alg, CCPadding padding, char *cipherText, char *plainText)
#ifdef CRYPTORWITHMODE
{
    byteBuffer key, iv;
    byteBuffer pt, ct;
    
	CCCryptorStatus retval;
    char cipherDataOut[4096];
    char plainDataOut[4096];
    size_t dataOutMoved;
    byteBuffer bb;
            
    key = hexStringToBytes(keyStr);        
    pt = ccConditionalTextBuffer(plainText);
    ct = ccConditionalTextBuffer(cipherText);
    iv = ccConditionalTextBuffer(ivStr);
    
   	if((retval = CCCryptWithMode(kCCEncrypt, mode, alg, padding, iv->bytes, key->bytes, key->len, NULL, 0, 0, 0,  pt->bytes, pt->len, 
            cipherDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	diag("Encrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(cipherDataOut, dataOutMoved);    	

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(!ct->bytes) {
    	diag("Input Length %d Result: %s\n", (int) pt->len, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(ct, bb)) {
            diag("FAIL\nEncrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
        	return 1;
        }
    }
    
    free(bb);
    
   	if((retval = CCCryptWithMode(kCCDecrypt, mode, alg, padding, iv->bytes, key->bytes, key->len, NULL, 0, 0, 0,  cipherDataOut, dataOutMoved, 
        plainDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	diag("Decrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(plainDataOut, dataOutMoved);
    
	if (!bytesAreEqual(pt, bb)) {
        diag("FAIL Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(pt));
        return 1;
    }
    
    // if(ct->bytes && iv->bytes) diag("PASS Test for length %d\n", (int) pt->len);
    // if(ct->bytes && (iv->bytes == NULL)) diag("PASS NULL IV Test for length %d\n", (int) pt->len);

    free(pt);
    free(ct);
    free(key);
    free(iv);
	return 0;
}
#else
{
    return 0;
}
#endif




int
CCMultiModeTestCase(char *keyStr, char *ivStr, CCMode mode, CCAlgorithm alg, CCPadding padding, char *cipherText, char *plainText)
#ifdef CRYPTORWITHMODE
{
    byteBuffer key, iv;
    byteBuffer pt, ct;    
	CCCryptorStatus retval;
    char cipherDataOut[4096];
    char plainDataOut[4096];
    size_t dataOutMoved;
    byteBuffer bb;
            
    key = hexStringToBytes(keyStr);        
    pt = ccConditionalTextBuffer(plainText);
    ct = ccConditionalTextBuffer(cipherText);
    iv = ccConditionalTextBuffer(ivStr);
    
   	if((retval = CCMultiCryptWithMode(kCCEncrypt, mode, alg, padding, iv->bytes,key->bytes, key->len, NULL, 0,0, 0, pt->bytes, pt->len, 
            cipherDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	diag("Encrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(cipherDataOut, dataOutMoved);    	

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(!ct->bytes) {
    	diag("Input Length %d Result: %s\n", (int) pt->len, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(ct, bb)) {
            diag("FAIL\nEncrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
        	return 1;
        }
    }
    
    free(bb);
    
   	if((retval = CCMultiCryptWithMode(kCCEncrypt, mode, alg, padding, iv->bytes, key->bytes, key->len, NULL, 0, 0, 0, 
        cipherDataOut, dataOutMoved, plainDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	diag("Decrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(plainDataOut, dataOutMoved);
    
	if (!bytesAreEqual(pt, bb)) {
        diag("FAIL Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(pt));
        return 1;
    }
    
    // if(ct && iv->bytes) diag("PASS Test for length %d\n", (int) pt->len);
    // if(ct && (iv->bytes == NULL)) diag("PASS NULL IV Test for length %d\n", (int) pt->len);

    free(pt);
    free(ct);
    free(key);
    free(iv);
	return 0;
}
#else
{
    return kCCSuccess;
}
#endif

#ifdef CCSYMGCM

static CCCryptorStatus 
CCCryptorGCMDiscreet(
	CCOperation 	op,				/* kCCEncrypt, kCCDecrypt */
	CCAlgorithm		alg,
	const void 		*key,			/* raw key material */
	size_t 			keyLength,	
	const void 		*iv,
	size_t 			ivLen,
	const void 		*aData,
	size_t 			aDataLen,
	const void 		*dataIn,
	size_t 			dataInLength,
  	void 			*dataOut,
	const void 		*tag,
	size_t 			*tagLength)
{
    CCCryptorStatus retval;
    CCCryptorRef    cref;
    
    retval = CCCryptorCreateWithMode(op, kCCModeGCM, alg, ccNoPadding, NULL, key, keyLength, NULL, 0, 0, 0, &cref);
    if(retval != kCCSuccess) return retval;
    
    retval = CCCryptorGCMAddIV(cref, iv, ivLen);
    if(retval != kCCSuccess) {
        printf("Failed to add IV\n");
        goto out;
    }
    
    retval = CCCryptorGCMaddAAD(cref, aData, aDataLen);
    if(retval != kCCSuccess) {
        printf("Failed to add ADD\n");
        goto out;
    }


    if(kCCEncrypt == op) {
        retval = CCCryptorGCMEncrypt(cref, dataIn, dataInLength, dataOut);
        if(retval != kCCSuccess) {
            printf("Failed to Encrypt\n");
            goto out;
        }
    } else {
        retval = CCCryptorGCMDecrypt(cref, dataIn, dataInLength, dataOut);
        if(retval != kCCSuccess) {
            printf("Failed to Decrypt\n");
            goto out;
        }
    }


    retval = CCCryptorGCMFinal(cref, tag, tagLength);
    if(retval != kCCSuccess) {
        printf("Failed to Finalize and get tag\n");
        goto out;
    }
    retval = CCCryptorGCMReset(cref);
    if(retval != kCCSuccess) {
        printf("Failed to Reset\n");
    }
    
    
out:

    CCCryptorRelease(cref);
    return retval;
}


int
CCCryptorGCMTestCase(char *keyStr, char *ivStr, char *aDataStr, char *tagStr, CCAlgorithm alg, char *cipherText, char *plainText)
{
    byteBuffer key, iv;
    byteBuffer pt, ct;
    byteBuffer adata, tag;
    byteBuffer bb;
    
    
	CCCryptorStatus retval;
    char cipherDataOut[4096];
    char plainDataOut[4096];
    char tagDataOut[4096];
    size_t tagDataOutlen;
    size_t  dataLen;
    

    key = hexStringToBytes(keyStr);        
    adata = ccConditionalTextBuffer(aDataStr);        
    tag = hexStringToBytes(tagStr);        
    pt = ccConditionalTextBuffer(plainText);
    ct = ccConditionalTextBuffer(cipherText);
    iv = ccConditionalTextBuffer(ivStr);
    
    dataLen = pt->len;
    
    tagDataOutlen = tag->len;
    memset(tagDataOut, 0, 16);
    if((retval = CCCryptorGCM(kCCEncrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, pt->bytes, dataLen, cipherDataOut, tagDataOut, &tagDataOutlen)) != kCCSuccess) {
    	diag("Encrypt Failed\n");
        return 1;
    }
        
    bb = bytesToBytes(cipherDataOut, dataLen);    	

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(!ct->bytes) {
    	diag("Input Length %d Result: %s\n", (int) dataLen, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(ct, bb)) {
            diag("FAIL Encrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
        	return 1;
        }
    }
    
    free(bb);
#if NEVER    
    bb = bytesToBytes(tagDataOut, tagDataOutlen);
    if (!bytesAreEqual(tag, bb)) {
        diag("FAIL Tag on plaintext is wrong\n       got %s\n  expected %s\n", bytesToHexString(bb), bytesToHexString(tag));
        return 1;
    }
#endif
    
    tagDataOutlen = tag->len;
    memset(tagDataOut, 0, 16);
    if((retval = CCCryptorGCM(kCCDecrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, cipherDataOut, dataLen, plainDataOut, tagDataOut, &tagDataOutlen)) != kCCSuccess) {
    	diag("Decrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(plainDataOut, dataLen);
    
	if (!bytesAreEqual(pt, bb)) {
        diag("FAIL Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(pt));
        return 1;
    }
    
    free(bb);
    
    bb = bytesToBytes(tagDataOut, tagDataOutlen);
    if (!bytesAreEqual(tag, bb)) {
        diag("FAIL Tag on ciphertext is wrong\n       got %s\n  expected %s\n", bytesToHexString(bb), bytesToHexString(tag));
        return 1;
    }
    
    free(bb);
    free(pt);
    free(ct);
    free(key);
    free(iv);
    // diag("Pass One-Shot GCM Test\n");
	return 0;
}

int
CCCryptorGCMDiscreetTestCase(char *keyStr, char *ivStr, char *aDataStr, char *tagStr, CCAlgorithm alg, char *cipherText, char *plainText)
{
    byteBuffer key, iv;
    byteBuffer pt, ct;
    byteBuffer adata, tag;
    byteBuffer bb;
    
    
	CCCryptorStatus retval;
    char cipherDataOut[4096];
    char plainDataOut[4096];
    char tagDataOut[4096];
    size_t tagDataOutlen;
    size_t  dataLen;
    

    key = hexStringToBytes(keyStr);        
    adata = ccConditionalTextBuffer(aDataStr);        
    tag = hexStringToBytes(tagStr);        
    pt = ccConditionalTextBuffer(plainText);
    ct = ccConditionalTextBuffer(cipherText);
    iv = ccConditionalTextBuffer(ivStr);
    
    dataLen = pt->len;
    
    tagDataOutlen = tag->len;
    memset(tagDataOut, 0, 4096);
    if((retval = CCCryptorGCMDiscreet(kCCEncrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, pt->bytes, dataLen, cipherDataOut, tagDataOut, &tagDataOutlen)) != kCCSuccess) {
    	diag("Encrypt Failed\n");
        return 1;
    }
        
    bb = bytesToBytes(cipherDataOut, dataLen);    	

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(!ct->bytes) {
    	diag("Input Length %d Result: %s\n", (int) dataLen, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(ct, bb)) {
            diag("FAIL Encrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
        	return 1;
        }
    }
    
    free(bb);

#ifdef NEVER
    bb = bytesToBytes(tagDataOut, tagDataOutlen);
    if (!bytesAreEqual(tag, bb)) {
        diag("FAIL Tag on plaintext is wrong\n       got %s\n  expected %s\n", bytesToHexString(bb), bytesToHexString(tag));
        return 1;
    }
#endif
    
    tagDataOutlen = tag->len;
    memset(tagDataOut, 0, 4096);
    if((retval = CCCryptorGCMDiscreet(kCCDecrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, cipherDataOut, dataLen, plainDataOut, tagDataOut, &tagDataOutlen)) != kCCSuccess) {
    	diag("Decrypt Failed\n");
        return 1;
    }
    
    bb = bytesToBytes(plainDataOut, dataLen);
    
	if (!bytesAreEqual(pt, bb)) {
        diag("FAIL Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(pt));
        return 1;
    }
    
    free(bb);
    
    bb = bytesToBytes(tagDataOut, tagDataOutlen);
    if (!bytesAreEqual(tag, bb)) {
        diag("FAIL Tag on ciphertext is wrong\n       got %s\n  expected %s\n", bytesToHexString(bb), bytesToHexString(tag));
        return 1;
    }
    
    free(bb);
    free(pt);
    free(ct);
    free(key);
    free(iv);
    // diag("Pass Discreet GCM Test\n");

	return 0;
}

#endif
