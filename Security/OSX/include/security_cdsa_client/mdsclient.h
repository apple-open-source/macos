/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// mdsclient - friendly interface to CDSA MDS API
//
// It is useful to think of the mdsclient interface as "slightly below" the
// rest of the cdsa_client layer. It does not actually call into CSSM (we
// consider MDS as a separate facility, "slightly lower" than CSSM as well).
// This means that you can use mdsclient without creating a binary dependency
// on CSSM, and thus Security.framework.
//

#ifndef _H_CDSA_CLIENT_MDSCLIENT
#define _H_CDSA_CLIENT_MDSCLIENT

#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/refcount.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <security_cdsa_utilities/cssmpods.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <security_cdsa_client/dliterators.h>
#include <Security/mdspriv.h>
#include <Security/mds_schema.h>


namespace Security {
namespace MDSClient {

// import query sublanguage classes into MDSClient namespace
using CssmClient::Attribute;
using CssmClient::Query;
using CssmClient::Record;
using CssmClient::Table;


//
// A singleton for the MDS itself.
// This is automatically created as a ModuleNexus when needed.
// You can reset() it to release resources.
// Don't make your own.
//
class Directory : public MDS_FUNCS, public CssmClient::DLAccess {
public:
	Directory();
	virtual ~Directory();

	MDS_HANDLE mds() const { return mCDSA.DLHandle; }
	const MDS_DB_HANDLE &cdsa() const;

public:
	CSSM_HANDLE dlGetFirst(const CSSM_QUERY &query,
		CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes, CSSM_DATA *data,
		CSSM_DB_UNIQUE_RECORD *&id);
	bool dlGetNext(CSSM_HANDLE handle,
		CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes, CSSM_DATA *data,
		CSSM_DB_UNIQUE_RECORD *&id);
	void dlAbortQuery(CSSM_HANDLE handle);
	void dlFreeUniqueId(CSSM_DB_UNIQUE_RECORD *id);
	void dlDeleteRecord(CSSM_DB_UNIQUE_RECORD *id);
	Allocator &allocator();
	
public:
	// not for ordinary use - system administration only
	void install();						// system default install/regenerate
	void install(const MDS_InstallDefaults *defaults, // defaults
		const char *path,				// path to bundle (NULL -> main)
		const char *subdir = NULL,		// subdirectory in Resources (NULL -> all)
		const char *file = NULL);		// individual file (NULL -> all)
	void uninstall(const char *guid, uint32 ssid);

private:
	mutable MDS_DB_HANDLE mCDSA;		// CDSA database handle
	mutable Mutex mInitLock;			// interlock for lazy DB open
	CssmAllocatorMemoryFunctions mMemoryFunctions;
	Guid mCallerGuid;					//@@@ fake/unused
};

extern ModuleNexus<Directory> mds;


} // end namespace MDSClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_MDSCLIENT
