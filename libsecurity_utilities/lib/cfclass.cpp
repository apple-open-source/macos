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

#include <security_utilities/cfclass.h>
#include <security_utilities/seccfobject.h>
#include <security_utilities/threading.h>
#include <CoreFoundation/CFString.h>

//
// CFClass
//
CFClass::CFClass(const char *name)
{
	// initialize the CFRuntimeClass structure
	version = 0;
	className = name;
	init = NULL;
	copy = NULL;
	finalize = finalizeType;
	equal = equalType;
	hash = hashType;
	copyFormattingDesc = copyFormattingDescType;
	copyDebugDesc = copyDebugDescType;
	
	// register
	typeID = _CFRuntimeRegisterClass(this);
	assert(typeID != _kCFRuntimeNotATypeID);
}

void
CFClass::finalizeType(CFTypeRef cf) throw()
{
	SecCFObject *obj = SecCFObject::optional(cf);
	if (!obj->isNew())
	{
		try {
			// Call the destructor.
			obj->~SecCFObject();
		} catch (...) {}
	}
}

Boolean
CFClass::equalType(CFTypeRef cf1, CFTypeRef cf2) throw()
{
	// CF checks for pointer equality and ensures type equality already
	try {
		return SecCFObject::optional(cf1)->equal(*SecCFObject::optional(cf2));
	} catch (...) {
		return false;
	}
}

CFHashCode
CFClass::hashType(CFTypeRef cf) throw()
{
	try {
		return SecCFObject::optional(cf)->hash();
	} catch (...) {
		return 666; /* Beasty return for error */
	}
}

CFStringRef
CFClass::copyFormattingDescType(CFTypeRef cf, CFDictionaryRef dict) throw()
{
	try {
		return SecCFObject::optional(cf)->copyFormattingDesc(dict);
	} catch (...) {
		return CFSTR("Exception thrown trying to format object");
	}
}

CFStringRef
CFClass::copyDebugDescType(CFTypeRef cf) throw()
{
	try {
		return SecCFObject::optional(cf)->copyDebugDesc();
	} catch (...) {
		return CFSTR("Exception thrown trying to format object");
	}
}


