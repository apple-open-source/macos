/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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

#ifdef	BSAFE_CSP_ENABLE


//
// bsobjects - C++ adaptations of popular BSafe 4 object types
//
#ifndef _H_BSOBJECTS
#define _H_BSOBJECTS

#include <security_cdsa_utilities/cssmutilities.h>
#include <security_utilities/alloc.h>
#include <aglobal.h>
#include <bsafe.h>

//
// A PodWrapper for BSafe's ITEM objects
//
class BSafeItem : public PodWrapper<BSafeItem, ITEM> {
public:
    BSafeItem() { ((ITEM *)this)->data = NULL; len = 0; }
    BSafeItem(void *addr, size_t sz)
    { ((ITEM *)this)->data = (unsigned char *)addr; len = sz; }
    BSafeItem(const CSSM_DATA &cData)
    { ((ITEM *)this)->data = cData.Data; len = cData.Length; }
    BSafeItem(const ITEM &cData)
    { *(ITEM *)this = cData; }

    void operator = (const CssmData &cData)
    { ((ITEM *)this)->data = (unsigned char *)cData.data(); len = cData.length(); }

    void *data() const { return ((ITEM *)this)->data; }
    size_t length() const { return len; }

    template <class T>
    T copy(Allocator &alloc)
    { return T(memcpy(alloc.malloc(length()), data(), length()), length()); }

    template <class T>
    T *copyp(Allocator &alloc)
    { return new(alloc) T(copy<T>(alloc)); }

    void *operator new (size_t size, Allocator &alloc)
    { return alloc.malloc(size); }
};

#endif //_H_BSOBJECTS
#endif	/* BSAFE_CSP_ENABLE */
