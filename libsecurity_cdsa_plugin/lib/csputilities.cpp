/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// csputilities - utility classes for CSP implementation
//
#include <security_cdsa_plugin/CSPsession.h>
#include <security_cdsa_plugin/cssmplugin.h>
#include <security_utilities/memutils.h>
#include <security_cdsa_utilities/cssmdates.h>

using LowLevelMemoryUtilities::increment;


//
// Writer objects
//
CSPFullPluginSession::Writer::Writer(CssmData *v, uint32 n, CssmData *rem)
: vec(v), firstVec(v), lastVec(v + n - 1), remData(rem)
{
    if (vec == NULL || n == 0)
        CssmError::throwMe(CSSMERR_CSP_INVALID_OUTPUT_VECTOR);	// CDSA p.253, amended
	useData(vec);
    written = 0;
}

void CSPFullPluginSession::Writer::allocate(size_t needed, Allocator &alloc)
{
	if (!needed)
		return; // No output buffer space needed so we're done.
    else if (vec == firstVec && !*vec) {	// initial null vector element, wants allocation there
        *vec = makeBuffer(needed, alloc);
        lastVec = vec;		// ignore all subsequent buffers in vector
		useData(vec);
    } else {
        // how much output space do we have left?
        size_t size = currentSize;
        for (CssmData *v = vec + 1; v <= lastVec; v++)
            size += v->length();
        if (size >= needed)
            return;	// we're fine
        if (remData) {
            if (!*remData) {	        // have overflow, can allocate
                *remData = makeBuffer(needed - size, alloc);
                return;	// got it
            }
            if (size + remData->length() >= needed)
                return;	// will fit into overflow
        }
        // not enough buffer space, and can't allocate
        CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
    }
}

void CSPFullPluginSession::Writer::nextBlock(void * &ptr, size_t &size)
{
    ptr = currentBuffer;
    size = currentSize;
}

void CSPFullPluginSession::Writer::use(size_t used)
{
    assert(used <= currentSize);
    written += used;
    if (used < currentSize) {
        currentBuffer = increment(currentBuffer, used);
        currentSize -= used;
    } else {
        if (vec < lastVec) {
            useData(vec++);				// use next vector buffer
        } else if (vec == lastVec && remData) {
            useData(remData);			// use remainder buffer
            vec++;						// mark used
#if !defined(NDEBUG) && 0
        } else if (vec == lastVec) {
            vec++;
        } else if (vec > lastVec) {
            assert(false);				// 2nd try to overflow end
#endif /* !NDEBUG */
        } else {
            currentBuffer = NULL;		// no more output buffer
            currentSize = 0;
        }
    }
}

void CSPFullPluginSession::Writer::put(void *addr, size_t size)
{
    while (size > 0) {
        void *p; size_t sz;
        nextBlock(p, sz);
        if (size < sz)
            sz = size;	// cap transfer
        memcpy(p, addr, sz);
        use(sz);
        addr = increment(addr, sz);
        size -= sz;
    }
}

size_t CSPFullPluginSession::Writer::close()
{
    return written;
}


//
// Common algorithm utilities
//
void CSPFullPluginSession::setKey(CssmKey &key,
								  const Context &context, CSSM_KEYCLASS keyClass,
								  CSSM_KEYATTR_FLAGS attrs, CSSM_KEYUSE use)
{
    // general setup
    memset(&key.KeyHeader, 0, sizeof(key.KeyHeader));
    key.KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
    key.KeyHeader.CspId = plugin.myGuid();
    key.KeyHeader.AlgorithmId = context.algorithm();
    key.KeyHeader.KeyClass = keyClass;
    key.KeyHeader.KeyUsage = use;
    key.KeyHeader.KeyAttr = attrs;

	CssmDate *theDate = context.get<CssmDate>(CSSM_ATTRIBUTE_START_DATE);
	if(theDate) {
		key.KeyHeader.StartDate = *theDate;
	}
	theDate = context.get<CssmDate>(CSSM_ATTRIBUTE_END_DATE);
	if(theDate) {
		key.KeyHeader.EndDate = *theDate;
	}

    // defaults (change as needed)
    key.KeyHeader.WrapAlgorithmId = CSSM_ALGID_NONE;
    
    // clear key data (standard says, "Always allocate this, ignore prior contents.")
    key = CssmData();
}
