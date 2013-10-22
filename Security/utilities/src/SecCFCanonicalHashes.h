//
//  SecCFCanonicalHashes.h
//  Security
//
//  Created by Mitch Adler on 2/8/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef _SECCFCANONICALHASHES_H_
#define _SECCFCANONICALHASHES_H_

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFData.h>

#include <corecrypto/ccdigest.h>

bool SecCFAppendCFDictionaryHash(CFDictionaryRef thisDictionary, const struct ccdigest_info* di, CFMutableDataRef toThis);


#endif
