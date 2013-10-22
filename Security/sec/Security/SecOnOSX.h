//
//  SecOnOSX.h
//  messsageProtection
//
//  Created by Keith on 10/21/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#ifndef _SECONOSX_H_
#define _SECONOSX_H_

#include <TargetConditionals.h>

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))

#include <Security/SecKeyPriv.h>

#define SecKeyCreateFromPublicData _SecKeyCreateFromPublicData

#if 0
SecKeyRef SecKeyCreateFromPublicData(CFAllocatorRef allocator, CFIndex algorithmID, CFDataRef serialized);
#endif

#endif  /* TARGET_OS_MAC */

#endif /* _SECONOSX_H_ */


