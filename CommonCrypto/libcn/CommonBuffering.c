//
//  CommonBuffering.c
//  CommonCrypto
//

#include <stdio.h>
#include "ccMemory.h"
#include "CommonBufferingPriv.h"
#include <AssertMacros.h>


CNBufferRef
CNBufferCreate(size_t chunksize)
{
    CNBufferRef retval = CC_XMALLOC(sizeof(CNBuffer));
    __Require_Quiet(NULL != retval, errOut);
    retval->chunksize = chunksize;
    retval->bufferPos = 0;
    retval->buf = CC_XMALLOC(chunksize);
    __Require_Quiet(NULL != retval->buf, errOut);
    return retval;
    
errOut:
    if(retval) {
        if(retval->buf) CC_XFREE(retval->buf, chunksize);
        CC_XFREE(retval, sizeof(CNBuffer));
    }
    return NULL;
}

CNStatus
CNBufferRelease(CNBufferRef *bufRef)
{
    CNBufferRef ref;
    
    __Require_Quiet(NULL != bufRef, out);
    ref = *bufRef;
    if(ref->buf) CC_XFREE(ref->buf, chunksize);
    if(ref) CC_XFREE(ref, sizeof(CNBuffer));
out:
    return kCNSuccess;
}



CNStatus
CNBufferProcessData(CNBufferRef bufRef, 
                    void *ctx, const void *in, const size_t inLen, void *out, size_t *outLen, 
                    cnProcessFunction pFunc, cnSizeFunction sizeFunc)
{
    size_t  blocksize = bufRef->chunksize;
    uint8_t *input = (uint8_t *) in, *output = out;
    size_t inputLen = inLen, outputLen, inputUsing, outputAvailable;
    
    outputAvailable = outputLen = *outLen;
    
    if(sizeFunc(ctx, bufRef->bufferPos + inLen) > outputAvailable) return  kCNBufferTooSmall;
    *outLen = 0;
    if(bufRef->bufferPos > 0) {
        inputUsing = CC_XMIN(blocksize - bufRef->bufferPos, inputLen);
        CC_XMEMCPY(&bufRef->buf[bufRef->bufferPos], in, inputUsing);
        bufRef->bufferPos += inputUsing;
        if(bufRef->bufferPos < blocksize) {
            return kCNSuccess;
        }
        pFunc(ctx, bufRef->buf, blocksize, output, &outputLen);
        inputLen -= inputUsing; input += inputUsing;
        output += outputLen; *outLen = outputLen; outputAvailable -= outputLen;
        bufRef->bufferPos = 0;
    }
    
    inputUsing = inputLen - inputLen % blocksize;
    if(inputUsing > 0) {
        outputLen = outputAvailable;
        pFunc(ctx, input, inputUsing, output, &outputLen);
        inputLen -= inputUsing; input += inputUsing;
        *outLen += outputLen;
    }
    
    if(inputLen > blocksize) {
        return kCNAlignmentError;
    } else if(inputLen > 0) {
        CC_XMEMCPY(bufRef->buf, input, inputLen);
        bufRef->bufferPos = inputLen;
    }
    return kCNSuccess;
    
}

CNStatus
CNBufferFlushData(CNBufferRef bufRef,
                  void *ctx, void *out, size_t *outLen,
                  cnProcessFunction pFunc, cnSizeFunction sizeFunc)
{
//    size_t outputLen, outputAvailable;
//    outputAvailable = outputLen = *outLen;

    if(bufRef->bufferPos > 0) {
        if(bufRef->bufferPos > bufRef->chunksize) return kCNAlignmentError;
        if(sizeFunc(ctx, bufRef->bufferPos) > *outLen) return kCNBufferTooSmall;
        pFunc(ctx, bufRef->buf, bufRef->bufferPos, out, outLen);
    } else {
        *outLen = 0;
    }
    return kCNSuccess;
}



bool
CNBufferEmpty(CNBufferRef bufRef)
{
    return bufRef->bufferPos == 0;
}
