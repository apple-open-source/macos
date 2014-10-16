/*
 * Copyright (c) 2002-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// SecCFTypes.h - CF runtime interface
//
#ifndef _SECURITY_SECCFTYPES_H_
#define _SECURITY_SECCFTYPES_H_

#include <CoreFoundation/CFRuntime.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/cfclass.h>

namespace Security
{

namespace KeychainCore
{

/* Singleton that registers all the CFClass instances with the CFRuntime.

   To make something a CFTypeRef you need to make the actual object inheirit from SecCFObject and provide implementation of the virtual functions in that class.
   
   In addition to that you need to define an opque type for the C API like:
   typedef struct __OpaqueYourObject *YourObjectRef;

   Add an instance of CFClass to the public section of SecCFTypes below to get it registered with the CFRuntime.
   CFClass yourObject;

   XXX
   In your C++ code you should use SecPointer<YourObject> to refer to instances of your class.  SecPointers are just like autopointers and implement * and -> semantics.  They refcount the underlying object.  So to create an instance or your new object you would do something like:
   
       SecPointer<YourObject> instance(new YourObject());

   SecPointers have copy semantics and if you subclass SecPointer and define a operator < on the subclass you can even safely store instances of your class in stl containers.

	Use then like this:
		instance->somemethod();
	or if you want a reference to the underlying object:
		YourObject &object = *instance;
	if you want a pointer to the underlying object:
		YourObject *object = instance.get();

	In the API glue you will need to use:
		SecPointer<YourObject> instance;
		[...] get the instance somehow
		return instance->handle();
		to return an opaque handle (the is a CFTypeRef) to your object.
		
	when you obtain an object as input use:
		SecYourObjectRef ref;
		SecPointer<YourObject> instance = YourObject::required(ref);
		to get a SecPointer to an instance of your object from the external CFTypeRef.
*/
class SecCFTypes
{
public:
    SecCFTypes();

public:
	/* Add new instances of CFClass here that you want registered with the CF runtime. */
	CFClass Access;
	CFClass ACL;
	CFClass Certificate;
	CFClass CertificateRequest;
	CFClass Identity;
	CFClass IdentityCursor;
	CFClass ItemImpl;
	CFClass KCCursorImpl;
	CFClass KeychainImpl;
	CFClass KeyItem;
    CFClass PasswordImpl;
	CFClass Policy;
	CFClass PolicyCursor;
	CFClass Trust;
	CFClass TrustedApplication;
	CFClass ExtendedAttribute;
};

extern SecCFTypes &gTypes();

} // end namespace KeychainCore

} // end namespace Security


#endif // !_SECURITY_SECCFTYPES_H_
