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
// os9utils - MacOS 9 specific utilities
//
#ifndef _H_OS9UTILS
#define _H_OS9UTILS

#include <Security/utility_config.h>
#if TARGET_API_MAC_OS8

#include <Security/utilities.h>
#include <string>


namespace Security
{

namespace MacOS9Utilities
{


//
// A temporary or in-place Str255 constructed from other string forms
//
class PString {
public:
        PString(const char *s) { set(s, strlen(s)); }
        PString(string s) { set(s.data(), s.size()); }
        operator const unsigned char * () const { return mString; }

private:
        Str255 mString;
        void set(const char *str, int size)
        {
	        if (size > 255)
	                CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	        mString[0] = size;
	        memcpy(mString+1, str, size);
        }
};


//
// Make an STL string from a Pascal string
//
inline string p2cString(StringPtr s)
{
        return string(reinterpret_cast<const char *>(s + 1), s[0]);
}


} // end namespace MacOS9Utilities

} // end namespace Security

#endif // OS 8/9
#endif //_H_OS9UTILS
