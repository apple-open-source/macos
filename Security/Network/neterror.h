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
// neterror - exception objects for Security::Network
//
#ifndef _H_NETERROR
#define _H_NETERROR

#include <Security/utilities.h>


namespace Security {
namespace Network {


//
// We subordinate our error space to the CSSM exception model.
// Our primary error space is that of MacOS OSStatus codes.
//
class Error : public Security::MacOSError {
protected:
	Error(OSStatus err);
public:
	virtual ~Error();
    //@@@ -1 == internal error?!
    static void throwMe(OSStatus err = -1) __attribute__((noreturn));
};


}	// end namespace Network
}	// end namespace Security


#endif _H_NETERROR
