/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// sstransit - Securityd client side transition support.
//
#ifndef _H_SSTRANSIT
#define _H_SSTRANSIT

#include <securityd_client/ssclient.h>
#include <security_cdsa_utilities/cssmwalkers.h>
#include <security_cdsa_utilities/AuthorizationWalkers.h>
#include <securityd_client/ucsp.h>
#include <securityd_client/ucspNotify.h>

namespace Security {
namespace SecurityServer {


// stock leading argument profile used by (almost) all calls
#define UCSP_ARGS	mGlobal().serverPort, mGlobal().thread().replyPort, &securitydCreds, &rcode

// common invocation profile (don't use directly)
#define IPCSTART(statement) \
	CSSM_RETURN rcode; security_token_t securitydCreds; check(statement)
#define IPCEND \
	if (securitydCreds.val[0] != 0   IFDEBUG( && !getenv("SECURITYSERVER_NONROOT"))) \
		CssmError::throwMe(CSSM_ERRCODE_VERIFICATION_FAILURE)
#define IPCEND_CHECK	IPCEND; if (rcode != CSSM_OK) CssmError::throwMe(rcode);
#define IPCN(statement) { \
	IPCSTART(statement); IPCEND_CHECK;  \
	}
#define IPC(statement)	{ activate(); IPCN(statement); }
#define IPCKEY(statement, key, tag) { \
	activate(); IPCSTART(statement); IPCEND; \
	switch (rcode) { \
	case CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT: \
		notifyAclChange(key, tag); \
	case CSSM_OK: \
		break; \
	default: \
		CssmError::throwMe(rcode); \
	} \
}

// pass mandatory or optional CssmData arguments into an IPC call
#define DATA(arg)			arg.data(), (mach_msg_type_number_t)(arg.length())
#define OPTIONALDATA(arg)	(arg ? arg->data() : NULL), (mach_msg_type_number_t)(arg ? arg->length() : 0)

// pass mandatory DataOutput argument into an IPC call
#define DATA_OUT(arg)                   arg.data(), arg.length()
    
// pass structured arguments in/out of IPC calls. See "data walkers" for details
#define COPY(copy)			copy, copy.length(), copy
#define COPY_OUT(copy)		&copy, &copy##Length, &copy##Base
#define COPY_OUT_DECL(type,name) type *name, *name##Base; mach_msg_type_number_t name##Length


//
// DataOutput manages an output CssmData argument.
//
class DataOutput {
public:
	DataOutput(CssmData &arg, Allocator &alloc)
		: allocator(alloc), mTarget(&arg) { mData = NULL; mLength = 0; }
	DataOutput(CssmData *arg, Allocator &alloc)
		: allocator(alloc), mTarget(arg) { mData = NULL; mLength = 0; }
	~DataOutput();
	
	void **data() { return &mData; }
	mach_msg_type_number_t *length() { return &mLength; }
	
	Allocator &allocator;

private:
	CssmData *mTarget;
	void *mData;
	mach_msg_type_number_t mLength;
};


//
// Bundle up an AccessCredentials meant for a database, parsing it for
// "special" samples that need extra evidence to be passed along.
//
class DatabaseAccessCredentials : public Copier<AccessCredentials> {
public:
	DatabaseAccessCredentials(const AccessCredentials *creds, Allocator &alloc);

private:
	void mapKeySample(CssmData &cspHandleData, CssmKey &key);
};


//
// Handle the standard CSSM data retrieval pattern (attribute vector+data)
//
class DataRetrieval : public Copier<CssmDbRecordAttributeData> {
public:
	DataRetrieval(CssmDbRecordAttributeData *&attrs, Allocator &alloc);
	~DataRetrieval();

	operator CssmDbRecordAttributeData **() { return &mAddr; }
	operator mach_msg_type_number_t *() { return &mLength; }
	CssmDbRecordAttributeData **base() { return &mBase; }

private:
	Allocator &mAllocator;
	CssmDbRecordAttributeData *&mAttributes;
	CssmDbRecordAttributeData *mAddr, *mBase;
	mach_msg_type_number_t mLength;
};


} // namespace SecurityServer
} // namespace Security

#endif //_H_SSTRANSIT
