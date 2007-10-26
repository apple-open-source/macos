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
#include "osxcodewrap.h"
#include <Security/SecCode.h>


//
// We don't really HAVE a canonical encoding, in the sense that
// the matching OSXCode::decode function won't recognize us.
// That's not the point; if you want use the old transmission logic,
// use the canonical OSXCode subclasses.
//
string OSXCodeWrap::encode() const
{
	return "?:unsupported";
}


//
// Canonical path directly from the SecCode's mouth
//	
string OSXCodeWrap::canonicalPath() const
{
	CFURLRef path;
	MacOSError::check(SecCodeCopyPath(mCode, kSecCSDefaultFlags, &path));
	return cfString(path, true);
}


//
// The executable path is a bit annoying to get, but not quite
// annoying enough to cache the result.
//
string OSXCodeWrap::executablePath() const
{
	CFRef<CFDictionaryRef> info;
	MacOSError::check(SecCodeCopySigningInformation(mCode, kSecCSDefaultFlags, &info.aref()));
	return cfString(CFURLRef(CFDictionaryGetValue(info, kSecCodeInfoMainExecutable)));
}
