//
//  SecDispatchRelease.h
//  utilities
//
//  Created by Mitch Adler on 11/26/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//


#ifndef _SECDISPATCHRELEASE_H_
#define _SECDISPATCHRELEASE_H_

#include <dispatch/dispatch.h>
#include <xpc/xpc.h>

#define dispatch_retain_safe(DO) {  \
    __typeof__(DO) _do = (DO);      \
    if (_do)                        \
        dispatch_retain(_do);       \
}

#define dispatch_release_safe(DO) { \
    __typeof__(DO) _do = (DO);      \
    if (_do)                        \
        dispatch_release(_do);      \
}

#define dispatch_release_null(DO) { \
    __typeof__(DO) _do = (DO);      \
    if (_do) {                      \
        (DO) = NULL;                \
        dispatch_release(_do);      \
    }                               \
}


#define xpc_retain_safe(XO) {  \
    __typeof__(XO) _xo = (XO); \
    if (_xo)                   \
        xpc_retain(_xo);       \
}

#define xpc_release_safe(XO) { \
    __typeof__(XO) _xo = (XO); \
    if (_xo)                   \
        xpc_release(_xo);      \
}

#define xpc_release_null(XO) {  \
    __typeof__(XO) _xo = (XO); \
    if (_xo) {                  \
        (XO) = NULL;            \
        xpc_release(_xo);       \
    }                           \
}

#endif

