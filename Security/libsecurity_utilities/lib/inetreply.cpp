/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// inetreply - manage Internet-standard reply strings
//
#include "inetreply.h"
#include <security_utilities/debugging.h>
#include <ctype.h>

namespace Security {
namespace IPPlusPlus {


//
// Construct an InetReply object from a WRITABLE buffer.
// The buffer will be alterered by this constructor, and needs to be left alone
// until the InetReply object is destroyed.
//
InetReply::InetReply(const char *buffer) : mBuffer(buffer)
{
    analyze();
}

void InetReply::analyze()
{
    // follow Internet rule #1: be lenient in what you accept
    /*const*/ char *p;				// (un-const is ANSI bogosity in strtol)
    mCode = (int) strtol(mBuffer, &p, 10);
    if (p == mBuffer) {			// conversion failed
        mCode = -1;				// error indicator
        mSeparator = ' ';
        mMessage = "?invalid?";
        return;
    }
    if (!*p) {					// just "nnn" (tolerate)
        mCode = atoi(p);
        mSeparator = ' ';
        mMessage = "";
        return;
    }
    mSeparator = *p++;
    while (isspace(*p)) p++;
    mMessage = p;
}


//
// Continuation handling
//
bool InetReply::Continuation::operator () (const char *input)
{
    if (mActive && !strncmp(input, mTestString, 4))
        mActive = false;
    return mActive;
}

bool InetReply::Continuation::operator () (const InetReply &reply)
{
    if (!mActive && reply.isContinued()) {
        mActive = true;
        snprintf(mTestString, 4, "%03d", reply.code());
        mTestString[3] = ' ';	// no \0 left in this string
    }
    return mActive;
}


}	// end namespace IPPlusPlus
}	// end namespace Security
