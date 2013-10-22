//
//  SecXPCError.h
//  utilities
//
//  Created by John Hurley on 5/6/13.
//  Copyright (c) 2013 Apple Inc. All rights reserved.
//

#ifndef _UTILITIES_SECXPCERROR_H_
#define _UTILITIES_SECXPCERROR_H_

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFError.h>
#include <xpc/xpc.h>

__BEGIN_DECLS

extern CFStringRef sSecXPCErrorDomain;

enum {
    kSecXPCErrorSuccess = 0,
    kSecXPCErrorUnexpectedType = 1,
    kSecXPCErrorUnexpectedNull = 2,
    kSecXPCErrorConnectionFailed = 3,
    kSecXPCErrorUnknown = 4,
};

CFErrorRef SecCreateCFErrorWithXPCObject(xpc_object_t xpc_error);
xpc_object_t SecCreateXPCObjectWithCFError(CFErrorRef error);

__END_DECLS

#endif /* UTILITIES_SECXPCERROR_H */
