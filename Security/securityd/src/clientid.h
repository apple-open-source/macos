/*
 * Copyright (c) 2000-2004,2006-2007,2012 Apple Inc. All Rights Reserved.
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
// clientid - track and manage identity of securityd clients
//
#ifndef _H_CLIENTID
#define _H_CLIENTID

#include "codesigdb.h"
#include <Security/SecCode.h>
#include <security_utilities/cfutilities.h>
#include <string>


//
// A ClientIdentification object is a mix-in class that tracks
// the identity of associated client processes and their sub-entities
// (aka Code Signing Guest objects).
//
class ClientIdentification : public CodeSignatures::Identity {
public:
	ClientIdentification();
	
	SecCodeRef processCode() const;
	SecCodeRef currentGuest() const;

	// CodeSignatures::Identity personality
	string getPath() const;
	const CssmData getHash() const;
    const bool checkAppleSigned() const;
	
protected:
	void setup(pid_t pid);

public:
	IFDUMP(void dump());
	
private:
	CFRef<SecCodeRef> mClientProcess;	// process-level client object

	mutable Mutex mLock;				// protects everything below
	
	struct GuestState {
		GuestState() : gotHash(false) { }
		CFRef<SecCodeRef> code;
		mutable bool gotHash;
		mutable SHA1::Digest legacyHash;
        mutable bool checkedSignature;
        mutable bool appleSigned;
	};
	typedef std::map<SecGuestRef, GuestState> GuestMap;
	mutable GuestMap mGuests;
	
	GuestState *current() const;
};


//
// Bonus function
//
std::string codePath(SecStaticCodeRef code);


#endif //_H_CLIENTID
