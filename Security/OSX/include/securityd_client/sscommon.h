/*
 * Copyright (c) 2000-2004,2006-2008,2011 Apple Inc. All Rights Reserved.
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

//
// some handle types used to be defined here, so don't break anybody still 
// relying on that
//
#include <securityd_client/handletypes.h>

#ifdef __cplusplus

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

#endif //__cplusplus


//
// The default Mach bootstrap registration name for SecurityServer,
// and the environment variable to override it
//
#define SECURITYSERVER_BOOTSTRAP_NAME	"com.apple.SecurityServer"
#define SECURITYSERVER_BOOTSTRAP_ENV	"SECURITYSERVER"

//
// Types of ACL bearers
//
typedef enum { dbAcl, keyAcl, objectAcl, loginAcl } AclKind;


#ifdef __cplusplus


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

#endif //__cplusplus


#endif //_H_SSCOMMON
