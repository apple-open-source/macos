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
// sscommon - common definitions for all securityd MIG interfaces
//
// This is meant to go into both ssclient and tdclient (for tokend), so it
// needs to be fairly generic.
//
#ifndef _H_SSCOMMON
#define _H_SSCOMMON


#include <Security/cssm.h>
#include <security_utilities/alloc.h>
#include <security_utilities/mach++.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmkey.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/cssmdbname.h>
#include <security_cdsa_utilities/cssmdb.h>


namespace Security {
namespace SecurityServer {

using MachPlusPlus::Port;
using MachPlusPlus::ReceivePort;


//
// The default Mach bootstrap registration name for SecurityServer,
// and the environment variable to override it
//
#define SECURITYSERVER_BOOTSTRAP_NAME	"com.apple.SecurityServer"
#define SECURITYSERVER_BOOTSTRAP_ENV	"SECURITYSERVER"


//
// Handle types.
// These are all CSSM_HANDLEs behind the curtain, but we try to be
// explicit about which kind they are.
// By protocol, each of these are in a different address space - i.e.
// a KeyHandle and a DbHandle with the same value may or may not refer
// to the same thing - it's up to the handle provider.
// GenericHandle is for cases where a generic handle is further elaborated
// with a "kind code" - currently for ACL manipulations only.
//
typedef CSSM_HANDLE DbHandle;			// database handle
typedef CSSM_HANDLE KeyHandle;			// cryptographic key handle
typedef CSSM_HANDLE RecordHandle;		// data record identifier handle
typedef CSSM_HANDLE SearchHandle;		// search (query) handle
typedef CSSM_HANDLE GenericHandle;		// for polymorphic handle uses

static const DbHandle noDb = 0;
static const KeyHandle noKey = 0;
static const RecordHandle noRecord = 0;
static const SearchHandle noSearch = 0;


//
// Types of ACL bearers
//
enum AclKind { dbAcl, keyAcl, objectAcl, loginAcl };


//
// Common structure for IPC-client mediator objects
//
class ClientCommon {
	NOCOPY(ClientCommon)
public:
	ClientCommon(Allocator &standard = Allocator::standard(),
		Allocator &returning = Allocator::standard())
		: internalAllocator(standard), returnAllocator(returning) { }

	Allocator &internalAllocator;
	Allocator &returnAllocator;

public:
	typedef Security::Context Context;
};



} // end namespace SecurityServer
} // end namespace Security


#endif //_H_SSCOMMON
