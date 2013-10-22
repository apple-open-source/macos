/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _XDR_DLDB_H
#define _XDR_DLDB_H

#include "sec_xdr.h"
#include "xdr_cssm.h"

#include <security_cdsa_utilities/cssmdb.h>

bool_t xdr_DLDbFlatIdentifier(XDR *xdrs, DataWalkers::DLDbFlatIdentifier *objp);
bool_t xdr_DLDbFlatIdentifierRef(XDR *xdrs, DataWalkers::DLDbFlatIdentifier **objp);

class CopyIn {
    public:
        CopyIn(const void *data, xdrproc_t proc) : mLength(0), mData(0) {
            if (data && !::copyin(static_cast<uint8_t*>(const_cast<void *>(data)), proc, &mData, &mLength))
                CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR);  
        }
        ~CopyIn() { if (mData) free(mData);     }
        u_int length() { return mLength; }
        void *data() { return mData; }
    protected:
        u_int mLength;
        void *mData;
};

// split out CSSM_DATA variant
class CopyOut {
    public:
		// CSSM_DATA can be output only if empty, but also specify preallocated memory to use
        CopyOut(void *copy, size_t size, xdrproc_t proc, bool dealloc = false, CSSM_DATA *in_out_data = NULL) : mLength(in_out_data?(u_int)in_out_data->Length:0), mData(NULL), mInOutData(in_out_data), mDealloc(dealloc), mSource(copy), mSourceLen(size) {
            if (copy && size && !::copyout(copy, (u_int)size, proc, mInOutData ? reinterpret_cast<void**>(&mInOutData) : &mData, &mLength)) {
                if (mInOutData && mInOutData->Length) // DataOut behaviour: error back to user if likely related to amount of space passed in
                    CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
                else
                    CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR);
            }
        }
        ~CopyOut();
        u_int length() { return mLength; } 
        void* data() { return mData; }
        void* get() { void *tmp = mData; mData = NULL; return tmp; }
    protected:
        u_int mLength;
        void *mData;
		CSSM_DATA *mInOutData;
		bool mDealloc;
		void *mSource;
		size_t mSourceLen;
};

#endif /* !_XDR_AUTH_H */
