/*
 * Copyright (c) 2006-2008,2010 Apple Inc. All Rights Reserved.
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
// csproxy - Code Signing Hosting Proxy
//
#ifndef _H_CSPROXY
#define _H_CSPROXY

#include <security_utilities/casts.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/debugging_internal.h>
#include <security_cdsa_utilities/handleobject.h>
#include <security_utilities/mach++.h>
#include <security_utilities/machserver.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <securityd_client/cshosting.h>
#include <Security/SecCodeHost.h>
#include <string>
#include <map>

using MachPlusPlus::Port;
using MachPlusPlus::MachServer;


//
// CodeSigningHost is a mix-in for an object representing a primary
// Code Signing host object. It performs two notionally separate functions:
//  (1) Register a hosting port.
//  (2) Optionally, maintain a guest registry to offload the host's work.
//
class CodeSigningHost : private MachServer::Handler {
public:
	CodeSigningHost();
	~CodeSigningHost();
	void reset();
	
	enum HostingState {
		noHosting,					// is not a host (yet), could go either way
		dynamicHosting,				// gave us its own hosting port to keep
		proxyHosting				// we act as a proxy for it
	};
	
	enum GuestCheck {
		strict,						// direct guest relationship required
		loose						// indirect or identity is okay (prefix check)
	};
	
	struct Guest : public RefCount, public HandleObject {
	public:
		~Guest();
		std::vector<SecGuestRef> guestPath; // guest chain to this guest
		uint32_t status;			// dynamic status
		std::string path;			// canonical code path
		CFRef<CFDictionaryRef> attributes; // matching attributes set
		CFRef<CFDataRef> cdhash;	// hash of CodeDirectory as specified by host
		bool dedicated;				// host is dedicated (and this is the only guest)
		
		operator bool() const { return attributes; }  // exists
		SecGuestRef guestRef() const { return int_cast<long, SecGuestRef>(handle()); }
		void setAttributes(const CssmData &attrData);
		CFDataRef attrData() const;
		void setHash(const CssmData &given, bool generate);
		
		bool isGuestOf(Guest *host, GuestCheck check) const;
		bool matches(CFIndex count, CFTypeRef keys[], CFTypeRef values[]) const;
		
		IFDUMP(void dump() const);
	
	private:
		mutable CFRef<CFDataRef> mAttrData; // XML form of attributes (must live until guest destruction)
	};
	
	void registerCodeSigning(mach_port_t hostingPort, SecCSFlags flags);
	Port hostingPort() const { return mHostingPort; }
	
	SecGuestRef createGuest(SecGuestRef guest,
		uint32_t status, const char *path,
		const CssmData &cdhash, const CssmData &attributes, SecCSFlags flags);
	void setGuestStatus(SecGuestRef guest, uint32_t status, const CssmData &attributes);
	void removeGuest(SecGuestRef host, SecGuestRef guest);

public:	
	IFDUMP(void dump() const);
	
public:
	// internal use only (public for use by MIG handlers)
	Guest *findHost(SecGuestRef hostRef); // find most dedicated guest of this host
	Guest *findGuest(Guest *host, const CssmData &attrData); // by host and attributes
	Guest *findGuest(SecGuestRef guestRef, bool hostOk = false); // by guest reference
	Guest *findGuest(Guest *host);		// any guest of this host

	class Lock;
	friend class Lock;
	
private:
	boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out);
	void eraseGuest(Guest *guest);

private:	
	mutable Mutex mLock;				// protects everything below
	
	// host port registry
	HostingState mHostingState;			// status of hosting support
	Port mHostingPort;					// his or ours or NULL

	// guest map (only used if mHostingState == proxyHosting)
	typedef std::map<SecGuestRef, RefPointer<Guest> > GuestMap;
	GuestMap mGuests;
};


//
// Proxy implementation of Code Signing Hosting protocol
//
#define CSH_ARGS	mach_port_t servicePort, mach_port_t replyPort, OSStatus *rcode

#define DATA_IN(base)	void *base, mach_msg_type_number_t base##Length
#define DATA_OUT(base)	void **base, mach_msg_type_number_t *base##Length
#define DATA(base)		CssmData(base, base##Length)

//
// Find a guest by arbitrary attribute set.
//
// This returns an array of canonical guest references describing the path
// from the host given to the guest found. If the host itself is returned
// as a guest, this will be an empty array (zero length).
//
// The subhost return argument may in the future return the hosting port for
// a guest who dynamically manages its hosting (thus breaking out of proxy mode),
// but this is not yet implemented.
//
kern_return_t cshosting_server_findGuest(CSH_ARGS, SecGuestRef hostRef,
                                         DATA_IN(attributes),
                                         GuestChain *foundGuest, mach_msg_type_number_t *depth, mach_port_t *subhost);

//
// Retrieve the path to a guest specified by canonical reference.
//
kern_return_t cshosting_server_identifyGuest(CSH_ARGS, SecGuestRef guestRef,
                                             char *path, char *hash, uint32_t *hashLength, DATA_OUT(attributes));

//
// Retrieve the status word for a guest specified by canonical reference.
//
kern_return_t cshosting_server_guestStatus(CSH_ARGS, SecGuestRef guestRef, uint32_t *status);

#undef CSH_ARGS
#undef DATA_IN
#undef DATA_OUT
#undef DATA

#endif //_H_CSPROXY
