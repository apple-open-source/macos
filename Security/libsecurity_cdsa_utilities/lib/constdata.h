/*
 * Copyright (c) 2000-2001,2003-2004,2006 Apple Computer, Inc. All Rights Reserved.
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


//
// constdata - shared constant binary data objects
//
#ifndef _H_CONSTDATA
#define _H_CONSTDATA

#include <security_utilities/utilities.h>
#include <security_utilities/refcount.h>


namespace Security {


//
// ConstData represents a contiguous, binary blob of constant data.
// Assignment is by sharing (thus cheap).
// ConstData is a (constant) Dataoid type.
//
class ConstData {    
private:
    class Blob : public RefCount {
    public:
        Blob() : mData(NULL), mSize(0) { }
        Blob(const void *base, size_t size, bool takeOwnership = false);
        ~Blob()		{ delete[] reinterpret_cast<const char *>(mData); }
    
        const void *data() const	{ return mData; }
        size_t length() const		{ return mSize; }
    
    private:
        const void *mData;
        size_t mSize;
    };
    
public:
    ConstData() { }		//@@@ use a nullBlob?
    ConstData(const void *base, size_t size, bool takeOwnership = false)
        : mBlob(new Blob(base, size, takeOwnership)) { }
        
    template <class T>
    static ConstData wrap(const T &obj, bool takeOwnership)
    { return ConstData(&obj, sizeof(obj), takeOwnership); }
    
public:
    const void *data() const	{ return mBlob ? mBlob->data() : NULL; }
    size_t length() const		{ return mBlob ? mBlob->length() : 0; }
    
    operator bool() const		{ return mBlob; }
    bool operator !() const		{ return !mBlob; }

    template <class T> operator const T *() const
    { return reinterpret_cast<const T *>(data()); }
    
    template <class T> const T &as() const
    { return *static_cast<const T *>(reinterpret_cast<const T *>(data())); }
    
private:
    RefPointer<Blob> mBlob;
};


}	// end namespace Security


#endif //_H_CONSTDATA
