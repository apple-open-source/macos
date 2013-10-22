//
//  testpolicy.h
//  regressions
//
//  Created by Mitch Adler on 7/21/11.
//  Copyright (c) 2011 Apple Inc. All rights reserved.
//

#ifndef regressions_testpolicy_h
#define regressions_testpolicy_h

#include <Security/SecPolicy.h>
#include <CoreFoundation/CoreFoundation.h>


void runCertificateTestForDirectory(SecPolicyRef policy, CFStringRef resourceSubDirectory, CFGregorianDate *date);


#endif
