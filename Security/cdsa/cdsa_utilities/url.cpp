/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


//
// url - URL object with decomposition
//
#include "url.h"
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFString.h>
#include <errno.h>


namespace Security {
namespace Network {


//
// Turn a CFStringRef into an STL string and release the incoming CFStringRef
//
static string mkstr(CFStringRef str)
{
    if (!str)
        return "";
    char buffer[2048];
    if (CFStringGetCString(str, buffer, sizeof(buffer), kCFStringEncodingUTF8))
        return buffer;
    else
        UnixError::throwMe(EINVAL);
}


//
// Construction
//
URL::URL()
{
    ref = NULL;
}

URL::URL(const char *s)
{
    ref = CFURLCreateWithBytes(NULL, (const UInt8 *)s, strlen(s), kCFStringEncodingUTF8, NULL);
    if (!ref)
        UnixError::throwMe(EINVAL);
}

URL::URL(const char *s, const URL &base)
{
    ref = CFURLCreateWithBytes(NULL, (const UInt8 *)s, strlen(s), kCFStringEncodingUTF8, base.ref);
    if (!ref)
        UnixError::throwMe(EINVAL);
}

URL::~URL()
{
    if (ref)
        CFRelease(ref);
}


//
// Extraction: These methods produce UTF8 strings
//
URL::operator string() const
{
    return mkstr(CFURLGetString(ref));
}

string URL::scheme() const
{
    return mkstr(CFURLCopyScheme(ref));
}

string URL::host() const
{
    return mkstr(CFURLCopyHostName(ref));
}

IPPort URL::port(IPPort defaultPort) const
{
    SInt32 port = CFURLGetPortNumber(ref);
    return (port == -1) ? defaultPort : port;
}

string URL::username() const
{
    return mkstr(CFURLCopyUserName(ref));
}

string URL::password() const
{
    return mkstr(CFURLCopyPassword(ref));
}

string URL::path() const
{
    Boolean isAbsolute;
    return "/" + mkstr(CFURLCopyStrictPath(ref, &isAbsolute));
}

string URL::resourceSpec() const
{
    return mkstr(CFURLCopyResourceSpecifier(ref));
}

string URL::fullPath() const
{
    return path() + resourceSpec();
}

string URL::basename() const
{
    return mkstr(CFURLCopyLastPathComponent(ref));
}

string URL::extension() const
{
    return mkstr(CFURLCopyPathExtension(ref));
}

void URL::recreateURL(const char* url)
{
    if(ref)
        CFRelease(ref);
    ref = CFURLCreateWithBytes(NULL, (const UInt8 *)url, strlen(url), kCFStringEncodingUTF8, NULL);
}

}	// end namespace Network
}	// end namespace Security
