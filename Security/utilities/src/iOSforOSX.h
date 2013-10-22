//
//  iOSforOSX.h
//  utilities
//
//  Created by J Osborne on 11/13/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef utilities_iOSforOSX_h
#define utilities_iOSforOSX_h

#include <TargetConditionals.h>
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))

extern CFURLRef SecCopyKeychainDirectoryFile(CFStringRef file);

CFURLRef PortableCFCopyHomeDirectoryURL(void);

#ifndef _SECURITY_SECRANDOM_H_
extern const void *kSecRandomDefault;
#endif

#ifndef _SECURITY_SECBASE_H_
typedef struct OpaqueSecKeyRef *SecKeyRef;
#endif
OSStatus SecKeyCopyPersistentRef(SecKeyRef item, CFDataRef *newPersistantRef);
OSStatus SecKeyFindWithPersistentRef(CFDataRef persistantRef, SecKeyRef *key);


#endif

CFURLRef PortableCFCopyHomeDirectoryURL(void) asm("_CFCopyHomeDirectoryURL");

#endif
