/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
// osxcodewrap - wrap an OSXCode around a SecCodeRef
//
#ifndef _H_OSXCODEWRAP
#define _H_OSXCODEWRAP

#include <security_utilities/osxcode.h>
#include <Security/SecCode.h>
#include <string>
#include <map>


//
// OSXCodeWrap is a partial OSXCode implementation that gets all its information
// from a SecStaticCodeRef API object. OSXCode and SecStaticCode are in many ways
// twin brothers, and this class allows the use of a SecStaticCode in places where
// an OSXCode is required.
// Note that OSXCodeWrap will not provide the capabilities of the canonical
// OSXCode subclasses (such as Bundle). its encodings will always specify a type
// code of '?' (unknown).
//
class OSXCodeWrap : public OSXCode {
public:
	OSXCodeWrap(SecStaticCodeRef code) : mCode(code) { }

	string encode() const;
	
	string canonicalPath() const;
	string executablePath() const;

private:
	CFCopyRef<SecStaticCodeRef> mCode;
};


#endif //_H_OSXCODEWRAP
