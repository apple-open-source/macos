/*
 *  CCCryptorTestFuncs.c
 *  CCRegressions
 *
 *
 */


#include <stdio.h>
#include <assert.h>
#include "CCCryptorTestFuncs.h"
#include "testbyteBuffer.h"
#include "testmore.h"
#include "capabilities.h"
#include "../../lib/ccMemory.h"
#include <CommonCrypto/CommonCryptorSPI.h>

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

#define log(do_print, MSG, ARGS...) \
if(do_print){test_diag(test_directive, test_reason, __FILE__, __LINE__, MSG, ## ARGS);}

int
CCCryptTestCase(char *keyStr, char *ivStr, CCAlgorithm alg, CCOptions options, char *cipherText, char *plainText, bool log)
{
    byteBuffer key, iv;
    byteBuffer pt, ct;
    byteBuffer bb=NULL, bb2=NULL;
    int rc=1; //error

	CCCryptorStatus retval;
    char cipherDataOut[4096];
    char plainDataOut[4096];
    size_t dataOutMoved;

            
    key = hexStringToBytes(keyStr);        
    pt = ccConditionalTextBuffer(plainText);
    ct = ccConditionalTextBuffer(cipherText);
    iv = ccConditionalTextBuffer(ivStr);

    if (alg==kCCAlgorithmAES) {
        //feed a wrong key length
        retval = CCCrypt(kCCEncrypt, alg, options, key->bytes, key->len-2, iv->bytes, pt->bytes, pt->len, cipherDataOut, 4096, &dataOutMoved);
        if (retval!=kCCParamError)
            goto errOut;
    }
        
    if((retval = CCCrypt(kCCEncrypt, alg, options, key->bytes, key->len, iv->bytes, pt->bytes, pt->len, cipherDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	log(log, "Encrypt Failed %d\n", retval);
        goto errOut;
    }
    
    bb = bytesToBytes(cipherDataOut, dataOutMoved);

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(!ct->bytes) {
    	log(log, "Input Length %d Result: %s\n", (int) pt->len, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(ct, bb)) {
            log(log, "FAIL Encrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
        	goto errOut;
        }
    }

    
    if((retval = CCCrypt(kCCDecrypt, alg, options, key->bytes, key->len, iv->bytes, cipherDataOut, dataOutMoved, plainDataOut, 4096, &dataOutMoved)) != kCCSuccess) {
    	log(log, "Decrypt Failed\n");
        goto errOut;
    }
    
    bb2 = bytesToBytes(plainDataOut, dataOutMoved);
    
	if (!bytesAreEqual(pt, bb2)) {
        log(log, "FAIL Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(pt));
        goto errOut;
    }

    rc=0;

    // if(ct->bytes && iv->bytes) diag("PASS Test for length %d\n", (int) pt->len);
    // if(ct && (iv->bytes == NULL)) diag("PASS NULL IV Test for length %d\n", (int) pt->len);

errOut:
    free(bb2);
    free(bb);
    free(pt);
    free(ct);
    free(key);
    free(iv);
	return rc;
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

    free(bb);

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

    free(bb);

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

    free(bb);
    
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

//------------------------------------------------------------------------------
#ifdef CCSYMGCM

#define chk_result(msg) if(retval != kCCSuccess) {\
                           printf("Failed to: "msg"\n");\
                           goto out;\
                        }

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
    void 		    *tagOut,
	size_t 			*tagLength,
    bool callItAnyway)
{
    CCCryptorStatus retval;
    CCCryptorRef    cref;
    
    retval = CCCryptorCreateWithMode(op, kCCModeGCM, alg, ccNoPadding, NULL, key, keyLength, NULL, 0, 0, 0, &cref);
    if(retval != kCCSuccess) return retval;

    if(callItAnyway || ivLen!=0){
        retval = CCCryptorGCMAddIV(cref, iv, ivLen);
        chk_result("add IV");
    }

    if(callItAnyway || aDataLen!=0){
        retval = CCCryptorGCMaddAAD(cref, aData, aDataLen);
        chk_result("add AAD");
    }


    if(callItAnyway || dataInLength!=0){
       if(kCCEncrypt == op) {
            retval = CCCryptorGCMEncrypt(cref, dataIn, dataInLength, dataOut);
            chk_result("Encrypt");
       } else {
            retval = CCCryptorGCMDecrypt(cref, dataIn, dataInLength, dataOut);
           chk_result("Decrypt");
       }
    }


    retval = CCCryptorGCMFinal(cref, tagOut, tagLength);
    chk_result("Finalize and get tag");

    retval = CCCryptorGCMReset(cref);
    chk_result("Failed to Reset");
    
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
    byteBuffer tg=NULL;
    int rc=1; //fail
    
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
    bb = NULL;

    dataLen = pt->len;
    
    tagDataOutlen = tag->len;
    CC_XZEROMEM(tagDataOut, 16);
    if((retval = CCCryptorGCM(kCCEncrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, pt->bytes, dataLen, cipherDataOut, tagDataOut, &tagDataOutlen)) != kCCSuccess) {
    	diag("Encrypt Failed\n");
        goto errOut;
    }
        
    bb = bytesToBytes(cipherDataOut, dataLen);    	

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(!ct->bytes) {
    	diag("Input Length %d Result: %s\n", (int) dataLen, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(ct, bb)) {
            diag("FAIL Encrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
        	goto errOut;
        }
    }

    // Decrypt correctly
    tagDataOutlen = tag->len;
    CC_XZEROMEM(tagDataOut, 16);
    if((retval = CCCryptorGCM(kCCDecrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, cipherDataOut, dataLen, plainDataOut, tagDataOut, &tagDataOutlen)) != kCCSuccess) {
    	diag("Decrypt Failed\n");
        goto errOut;
    }

    free(bb);
    bb = bytesToBytes(plainDataOut, dataLen);
    tg = bytesToBytes(tagDataOut, tagDataOutlen);
    
	if (!bytesAreEqual(pt, bb)) {
        diag("FAIL Decrypt Output %s\nDecrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(pt));
        goto errOut;
    }

    if (timingsafe_bcmp(tagDataOut, tag->bytes, tag->len)) {
        diag("FAIL Tag on ciphertext is wrong\n       got %s\n  expected %s\n", bytesToHexString(tg), bytesToHexString(tag));
        goto errOut;
    }

    // Decrypt incorrectly (IV has been changed)
    tagDataOutlen = tag->len;
    CC_XZEROMEM(tagDataOut, 16);
    assert(iv->len>0);
    iv->bytes[0]^=1; // corrupt the IV
    if((retval = CCCryptorGCM(kCCDecrypt, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, cipherDataOut, dataLen, plainDataOut, tagDataOut, &tagDataOutlen)) != kCCSuccess) {
        diag("Decrypt Failed\n");
        goto errOut;
    }

    free(bb);
    bb = bytesToBytes(plainDataOut, dataLen);

    if (dataLen>0 && bytesAreEqual(pt, bb)) {
        diag("FAIL Output is expected to be wrong because IV was changed\n");
        goto errOut;
    }

    if (!timingsafe_bcmp(tagDataOut, tag->bytes, tag->len)) {
        diag("FAIL Tag on ciphertext is expected to be wrong here because IV was changed\n");
        goto errOut;
    }


    rc = 0;

errOut:
    free(tg);
    free(bb);
    free(pt);
    free(ct);
    free(key);
    free(iv);
    free(adata);
    free(tag);
    // diag("Pass One-Shot GCM Test\n");
	return rc;
}

static int
GCMDiscreetTestCase(CCOperation op, char *keyStr, char *ivStr, char *aDataStr, char *tagStr, CCAlgorithm alg, char *cipherText, char *plainText, bool CallItAnyway)
{
    byteBuffer key, iv;
    byteBuffer pt, ct;
    byteBuffer adata, tag;
    byteBuffer bb;
    byteBuffer tg=NULL;

    CCCryptorStatus retval;
    char DataOut[4096];
    char tagDataOut[4096];
    size_t tagDataOutlen;

    key = hexStringToBytes(keyStr);
    adata = ccConditionalTextBuffer(aDataStr);
    tag = hexStringToBytes(tagStr);
    pt = ccConditionalTextBuffer(plainText);
    ct = ccConditionalTextBuffer(cipherText);
    iv = ccConditionalTextBuffer(ivStr);


    const void 		*dataIn;
    size_t 			dataInLength;
    if(op == kCCEncrypt){
        dataInLength = pt->len;
        dataIn = pt->bytes;
    } else{
        dataInLength = ct->len;
        dataIn = ct->bytes;
    }


    tagDataOutlen = tag->len;
    CC_XZEROMEM(tagDataOut, 4096);
    if((retval = CCCryptorGCMDiscreet(op, alg, key->bytes, key->len, iv->bytes, iv->len, adata->bytes, adata->len, dataIn, dataInLength, DataOut, tagDataOut, &tagDataOutlen, CallItAnyway)) != kCCSuccess) {
        diag("Encrypt Failed\n");
        return 1;
    }

    bb = bytesToBytes(DataOut, dataInLength);
    tg = bytesToBytes(tagDataOut, tagDataOutlen);

    // If ct isn't defined we're gathering data - print the ciphertext result
    if(ct->bytes==NULL || pt->bytes==NULL) {
        diag("Input Length %d Result: %s\n", (int) dataInLength, bytesToHexString(bb));
    } else {
        if (!bytesAreEqual(op==kCCEncrypt?ct:pt, bb)) {
            diag("FAIL Encrypt Output %s\nEncrypt Expect %s\n", bytesToHexString(bb), bytesToHexString(ct));
            return 1;
        }
    }

    if(op ==kCCDecrypt){
        if (timingsafe_bcmp(tagDataOut, tag->bytes, tag->len)) {
            diag("FAIL Tag on ciphertext is wrong\n       got %s\n  expected %s\n", bytesToHexString(tg), bytesToHexString(tag));
            return 1;
        }
    }
    free(tg);
    free(tag);
    free(bb);
    free(pt);
    free(ct);
    free(key);
    free(iv);
    free(adata);
    // diag("Pass Discreet GCM Test\n");
    
    return 0;
}

int
CCCryptorGCMDiscreetTestCase(char *keyStr, char *ivStr, char *aDataStr, char *tagStr, CCAlgorithm alg, char *cipherText, char *plainText)
{

    CCCryptorStatus rc;
    rc = GCMDiscreetTestCase(kCCEncrypt, keyStr, ivStr, aDataStr, tagStr, alg, cipherText, plainText, true);
    if(rc){
        diag("GCM Encrypt Failed\n");
        return 1;
    }

    rc = GCMDiscreetTestCase(kCCDecrypt, keyStr, ivStr, aDataStr, tagStr, alg, cipherText, plainText, true);
    if(rc){
        diag("GCM Decrypt Failed\n");
        return 1;
     }


    rc = GCMDiscreetTestCase(kCCEncrypt, keyStr, ivStr, aDataStr, tagStr, alg, cipherText, plainText, false);
    if(rc){
        diag("GCM Encrypt (bypassing IV and AAD) Failed\n");
        return 1;
    }

    rc = GCMDiscreetTestCase(kCCDecrypt, keyStr, ivStr, aDataStr, tagStr, alg, cipherText, plainText, false);
    if(rc){
        diag("GCM decrypt (bypassing IV and AAD) Failed\n");
        return 1;
    }

    return 0;
}

#endif
