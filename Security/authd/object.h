/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#ifndef _SECURITY_AUTH_OBJECT_H_
#define _SECURITY_AUTH_OBJECT_H_

#include "authd_private.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#if defined(__cplusplus)
extern "C" {
#endif
    
#define __AUTH_BASE_STRUCT_HEADER__ \
    CFRuntimeBase _base;
    
struct _auth_base_s {
    __AUTH_BASE_STRUCT_HEADER__;
};

#define AUTH_TYPE(type) const CFRuntimeClass type
    
#define AUTH_TYPE_INSTANCE(name, ...) \
    AUTH_TYPE(_auth_type_##name) = { \
        .version = 0, \
        .className = #name "_t", \
        __VA_ARGS__ \
    }

#define AUTH_CLASS_SIZE(name) (sizeof(struct _##name##_s) - sizeof(CFRuntimeBase))
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_OBJECT_H_ */
