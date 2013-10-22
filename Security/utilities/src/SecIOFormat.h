//
//  SecIOFormat.h
//  utilities
//
//  Created by Mitch Adler on 10/1/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef _SECIOFORMAT_H_
#define _SECIOFORMAT_H_

#include <inttypes.h>

// MARK: CFIndex printing support

#ifdef __LLP64__
#  define PRIdCFIndex    "lld"
#  define PRIiCFIndex    "lli"
#  define PRIoCFIndex    "llo"
#  define PRIuCFIndex    "llu"
#  define PRIxCFIndex    "llx"
#  define PRIXCFIndex    "llX"
#else
#  define PRIdCFIndex    "ld"
#  define PRIiCFIndex    "li"
#  define PRIoCFIndex    "lo"
#  define PRIuCFIndex    "lu"
#  define PRIxCFIndex    "lx"
#  define PRIXCFIndex    "lX"
#endif

// MARK: OSStatus printing support

#ifdef __LP64__
#  define PRIdOSStatus    "d"
#  define PRIiOSStatus    "i"
#  define PRIoOSStatus    "o"
#  define PRIuOSStatus    "u"
#  define PRIxOSStatus    "x"
#  define PRIXOSStatus    "X"
#else
#  define PRIdOSStatus    "ld"
#  define PRIiOSStatus    "li"
#  define PRIoOSStatus    "lo"
#  define PRIuOSStatus    "lu"
#  define PRIxOSStatus    "lx"
#  define PRIXOSStatus    "lX"
#endif

#endif
