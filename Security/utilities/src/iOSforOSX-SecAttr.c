//
//  iOSforOSX.c
//  utilities
//
//  Created by J Osborne on 11/13/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <TargetConditionals.h>

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include "iOSforOSX.h"
#include <pwd.h>
#include <unistd.h>

// Was in SOSAccount.c
#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));
// We may not have all of these we need
SEC_CONST_DECL (kSecAttrAccessible, "pdmn");
SEC_CONST_DECL (kSecAttrAccessibleAlwaysThisDeviceOnly, "dku");

#endif
