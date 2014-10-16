/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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


#ifndef _SECCFRELEASE_H_
#define _SECCFRELEASE_H_

#include <CoreFoundation/CFBase.h>

// Retains its argument unless it's NULL.  Always returns its argument.
#define CFRetainSafe(CF) ({ __typeof__(CF) _cf = (CF); _cf ? (CFRetain(_cf), _cf) : _cf; })

// Assume CF is NULL or an already retained object and CFReleaseSafe VAR and assigns CF to VAR. Returns CF.
#define CFAssignRetained(VAR,CF) ({ \
    __typeof__(VAR) *const _pvar = &(VAR); \
    __typeof__(CF) _cf = (CF); \
    (*_pvar) = *_pvar ? (CFRelease(*_pvar), _cf) : _cf; \
})

// CFRetainSafe CF and CFReleaseSafe VAR and assigns CF to VAR. Returns CF.
#define CFRetainAssign(VAR,CF) ({ \
    __typeof__(VAR) *const _pvar = &(VAR); \
    __typeof__(CF) _cf = (CF); \
    (((*_pvar) == _cf) ? _cf \
    : ((*_pvar) = (_cf ? (CFRetain(_cf), (*_pvar ? (CFRelease(*_pvar), _cf) : _cf)) \
                   : (CFRelease(*_pvar), ((__typeof__(_cf))0))))); \
})

// Releases CF unless it's NULL.  Always returns NULL, for your convenience and constructs like:
// return CFReleaseSafe(foo);
#define CFReleaseSafe(CF) ({ __typeof__(CF) _cf = (CF); (_cf ? (CFRelease(_cf), ((__typeof__(CF))0)) : _cf); })

// Assigns NULL to CF. Releases the value stored at CF unless it was NULL.  Always returns NULL, for your convenience
#define CFReleaseNull(CF) ({ __typeof__(CF) *const _pcf = &(CF), _cf = *_pcf; (_cf ? (*_pcf) = ((__typeof__(CF))0), (CFRelease(_cf), ((__typeof__(CF))0)) : _cf); })


#endif /* _SECCFRELEASE_H_ */
