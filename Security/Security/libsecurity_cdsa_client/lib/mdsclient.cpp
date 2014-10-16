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
#include <security_cdsa_client/mdsclient.h>
#include <Security/mdspriv.h>


namespace Security {
namespace MDSClient {


//
// The MDS access object singleton
//
ModuleNexus<Directory> mds;


//
// Directory construction initializes MDS and opens the "CDSA" database
//
Directory::Directory()
	: mMemoryFunctions(Allocator::standard())
{
	StLock<Mutex> _(mInitLock);
	CssmError::check(MDS_Initialize(&mCallerGuid, &mMemoryFunctions,
		this, &mCDSA.DLHandle));
	mCDSA.DBHandle = CSSM_INVALID_HANDLE;
}


//
// Cleanup (only called if the ModuleNexus is explicitly reset)
//
Directory::~Directory()
{
	if (mCDSA.DBHandle)
		CssmError::check(DbClose(mCDSA));
	CssmError::check(MDS_Terminate(mds()));
}


//
// Open MDS database if needed
//
const MDS_DB_HANDLE &Directory::cdsa() const
{
	if (mCDSA.DBHandle == CSSM_INVALID_HANDLE) {
		StLock<Mutex> _(mInitLock);
		if (mCDSA.DBHandle == CSSM_INVALID_HANDLE)
			CssmError::check(DbOpen(mCDSA.DLHandle, MDS_CDSA_DIRECTORY_NAME, NULL,
				CSSM_DB_ACCESS_READ,	// access mode
				NULL,					// credentials
				NULL,					// OpenParameters
				&mCDSA.DBHandle));
	}
	return mCDSA;
}


//
// The DLAccess implementation for MDS.
// We don't ever return record data, of course; we just zero it out.
//
CSSM_HANDLE Directory::dlGetFirst(const CSSM_QUERY &query, CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes,
	CSSM_DATA *data, CSSM_DB_UNIQUE_RECORD *&id)
{
	CSSM_HANDLE result;
	switch (CSSM_RETURN rc = DataGetFirst(cdsa(), &query, &result, &attributes, NULL, &id)) {
	case CSSM_OK:
		if (data)
			*data = CssmData();
		return result;
	case CSSMERR_DL_ENDOFDATA:
		return CSSM_INVALID_HANDLE;
	default:
		CssmError::throwMe(rc);
		return CSSM_INVALID_HANDLE; // placebo
	}
}

bool Directory::dlGetNext(CSSM_HANDLE handle, CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes,
	CSSM_DATA *data, CSSM_DB_UNIQUE_RECORD *&id)
{
	CSSM_RETURN rc = DataGetNext(cdsa(), handle, &attributes, NULL, &id);
	switch (rc) {
	case CSSM_OK:
		if (data)
			*data = CssmData();
		return true;
	case CSSMERR_DL_ENDOFDATA:
		return false;
	default:
		CssmError::throwMe(rc);
		return false;   // placebo
	}
}

void Directory::dlAbortQuery(CSSM_HANDLE handle)
{
	CssmError::check(DataAbortQuery(cdsa(), handle));
}

void Directory::dlFreeUniqueId(CSSM_DB_UNIQUE_RECORD *id)
{
	CssmError::check(FreeUniqueRecord(cdsa(), id));
}

void Directory::dlDeleteRecord(CSSM_DB_UNIQUE_RECORD *id)
{
	CssmError::check(DataDelete(cdsa(), id));
}

Allocator &Directory::allocator()
{
	return Allocator::standard();
}


//
// Public MDS operations
//
void Directory::install()
{
	CssmError::check(MDS_Install(this->mds()));
}

void Directory::install(const MDS_InstallDefaults *defaults,
	const char *path, const char *subdir, const char *file)
{
	CssmError::check(MDS_InstallFile(this->mds(), defaults, path, subdir, file));
}

void Directory::uninstall(const char *guid, uint32 ssid)
{
	CssmError::check(MDS_RemoveSubservice(this->mds(), guid, ssid));
}


} // end namespace MDSClient
} // end namespace Security
