/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
// SecCFTypes.h - CF runtime interface
//
#ifndef _SECURITY_SECCFTYPES_H_
#define _SECURITY_SECCFTYPES_H_

#include <Security/Access.h>
#include <Security/ACL.h>
#include <Security/Certificate.h>
#include <Security/CertificateRequest.h>
#include <Security/Identity.h>
#include <Security/IdentityCursor.h>
#include <Security/Item.h>
#include <Security/KCCursor.h>
#include <Security/Keychains.h>
#include <Security/KeyItem.h>
#include <Security/Policies.h>
#include <Security/PolicyCursor.h>
#include <Security/Trust.h>
#include <Security/TrustedApplication.h>

//#include <Security/SecAccess.h>
//#include <Security/SecCertificate.h>
#include <Security/SecCertificateRequest.h>
//#include <Security/SecIdentity.h>
#include <Security/SecIdentitySearch.h>
//#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
//#include <Security/SecKeychain.h>
//#include <Security/SecKey.h>
//#include <Security/SecPolicy.h>
//#include <Security/SecACL.h>
#include <Security/SecPolicySearch.h>
#include <Security/SecTrust.h>
//#include <Security/SecTrustedApplication.h>

#include <Security/utilities.h>
#include <map>

namespace Security
{

namespace KeychainCore
{

/* Singleton that registers all the CFClass<> instances with the CFRuntime.

   To make something a CFTypeRef you need to make the actual object inheirit from SecCFObject and provide implementation of the virtual functions in that class.
   
   In addition to that you need to define an opque type for the C API like:
   typedef struct __OpaqueYourObject *YourObjectRef;
   and in the C++ headers you define something like:
   typedef CFClass<YourObject, YourObjectRef> YourObjectClass;

   Add an instance of the YourObjectClass to the public section of SecCFTypes below to get it registered with the CFRuntime.
   YourObjectClass yourObject;


   In your C++ code you should use RefPointer<YourObject> to refer to instances of your class.  RefPointers are just like autopointers and implement * and -> semantics.  They refcount the underlying object.  So to create an instance or your new object you would do something like:
   
       RefPointer<YourObject> instance(new YourObject());

   RefPointers have copy semantics and if you subclass RefPointer and define a operator < on the subclass you can even safely store instances of your class in stl containers.

	Use then like this:
		instance->somemethod();
	or if you want a reference to the underlying object:
		YourObject &object = *instance;
	if you want a pointer to the underlying object:
		YourObject *object = instance.get();

	In the API glue you will need to use:
		RefPointer<YourObject> instance;
		[...] get the instance somehow
		return gTypes().yourObject.handle(*instance);
		to return an opaque handle (the is a CFTypeRef) to your object.
		
	when you obtain an object as input use:
		SecYourObjectRef ref;
		RefPointer<YourObject> instance = gTypes().yourObject.required(ref);
		to get a RefPointer to an instance of your object fro the external CFTypeRef.
*/
class SecCFTypes
{
public:
    SecCFTypes();

public:
	/* Add new instances of CFClass<> here that you want registered with the CF runtime. */

	/* @@@ Error should be errSecInvalidAccessRef */
	CFClass<Access, SecAccessRef, errSecInvalidItemRef> access;
	/* @@@ Error should be errSecInvalidTrustedApplicationRef */
	CFClass<ACL, SecACLRef, errSecInvalidItemRef> acl;
	/* @@@ Error should be errSecInvalidCertificateRef */
	CFClass<Certificate, SecCertificateRef, errSecInvalidItemRef> certificate;
	/* @@@ Error should be errSecInvalidCertificateRequestRef */
	CFClass<CertificateRequest, SecCertificateRequestRef, errSecInvalidItemRef> certificateRequest;
	/* @@@ Error should be errSecInvalidIdentityRef */
	CFClass<Identity, SecIdentityRef, errSecInvalidItemRef> identity;
	CFClass<IdentityCursor, SecIdentitySearchRef, errSecInvalidSearchRef> identityCursor;
	CFClass<ItemImpl, SecKeychainItemRef, errSecInvalidItemRef> item;
	CFClass<KCCursorImpl, SecKeychainSearchRef, errSecInvalidSearchRef> cursor;
	CFClass<KeychainImpl, SecKeychainRef, errSecInvalidKeychain> keychain;
	/* @@@ Error should be errSecInvalidKeyRef */
	CFClass<KeyItem, SecKeyRef, errSecInvalidItemRef> keyItem;
	/* @@@ Error should be errSecInvalidPolicyRef */
	CFClass<Policy, SecPolicyRef, errSecInvalidItemRef> policy;
	/* @@@ Error should be errSecInvalidPolicySearchRef */
	CFClass<PolicyCursor, SecPolicySearchRef, errSecInvalidSearchRef> policyCursor;
	/* @@@ Error should be errSecInvalidTrustRef */
	CFClass<Trust, SecTrustRef, errSecInvalidItemRef> trust;
	/* @@@ Error should be errSecInvalidTrustedApplicationRef */
	CFClass<TrustedApplication, SecTrustedApplicationRef, errSecInvalidItemRef> trustedApplication;

public:
    Mutex mapLock;
    typedef std::map<SecCFObject *, const SecCFType *> Map;
    Map map;
};


extern ModuleNexus<SecCFTypes> gTypes;

} // end namespace KeychainCore

} // end namespace Security


#endif // !_SECURITY_SECCFTYPES_H_
