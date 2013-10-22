//
//  iOSforOSX.c
//  utilities
//
//  Created by J Osborne on 11/13/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <TargetConditionals.h>

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
const void *kSecRandomDefault = (void*)0;
#endif
