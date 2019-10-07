/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * YarrowConnection.cpp - single, process-wide, thread-safe Yarrow client
 */
#include "YarrowConnection.h"
#include <security_utilities/globalizer.h>
#include <Security/cssmtype.h>
#include <Security/SecRandom.h>

/* and the exported functions */
void cspGetRandomBytes(void *buf, unsigned len)
{
    MacOSError::check(SecRandomCopyBytes(kSecRandomDefault, len, buf));
}

/* Unused, since SecRandomCopyBytes returns from a suitable CSPRNG seeded elsewhere */
void cspAddEntropy(const void *buf, unsigned len)
{
}
