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
// SecCFTypes.cpp - CF runtime interface
//

#include <Security/SecCFTypes.h>

using namespace KeychainCore;

ModuleNexus<SecCFTypes> Security::KeychainCore::gTypes;

SecCFTypes::SecCFTypes() :
	access("SecAccess"),
	acl("SecACL"),
	certificate("SecCertificate"),
	certificateRequest("SecCertificateRequest"),
	identity("SecIdentity"),
	identityCursor("SecIdentitySearch"),
	item("SecKeychainItem"),
	cursor("SecKeychainSearch"),
	keychain("SecKeychain"),
	keyItem("SecKey"),
	policy("SecPolicy"),
	policyCursor("SecPolicySearch"),
	trust("SecTrust"),
	trustedApplication("SecTrustedApplication")
{
}
