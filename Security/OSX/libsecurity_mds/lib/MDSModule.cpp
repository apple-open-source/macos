/*
 * Copyright (c) 2000-2001,2011,2013-2014 Apple Inc. All Rights Reserved.
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


#include "MDSModule.h"
#include "MDSSession.h"
#include <Security/mds_schema.h>
#include <memory>

namespace Security
{

ModuleNexus<MDSModule> MDSModule::mModuleNexus;

// Names and IDs of tables used in the MDS databases

#define TABLE(t) { t, #t }

/*
 * For now, to allow compatibility with AppleFileDL, we use the same record IDs
 * it uses when constructing an AppleDatabaseManager. See Radar 2817921 for details. 
 * The fix requires that AppleDatabase be able to fetch its meta-table relationIDs 
 * from an existing DB at DbOpen time; I'm not sure that's possible. 
 */
#define USE_FILE_DL_TABLES		1

static const AppleDatabaseTableName kTableNames[] = {
    // the meta-tables. the parsing module is not used by MDS, but is required
    // by the implementation of the database
	#if USE_FILE_DL_TABLES
    TABLE(CSSM_DL_DB_SCHEMA_INFO),
    TABLE(CSSM_DL_DB_SCHEMA_ATTRIBUTES),
    TABLE(CSSM_DL_DB_SCHEMA_INDEXES),
	#else
    TABLE(MDS_CDSADIR_MDS_SCHEMA_RELATIONS),
    TABLE(MDS_CDSADIR_MDS_SCHEMA_ATTRIBUTES),
    TABLE(MDS_CDSADIR_MDS_SCHEMA_INDEXES),
	#endif
    TABLE(CSSM_DL_DB_SCHEMA_PARSING_MODULE),
	
    // the MDS-specific tables
    TABLE(MDS_OBJECT_RECORDTYPE),
    TABLE(MDS_CDSADIR_CSSM_RECORDTYPE),
    TABLE(MDS_CDSADIR_KRMM_RECORDTYPE),
    TABLE(MDS_CDSADIR_EMM_RECORDTYPE),
    TABLE(MDS_CDSADIR_COMMON_RECORDTYPE),
    TABLE(MDS_CDSADIR_CSP_PRIMARY_RECORDTYPE),
    TABLE(MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE),
    TABLE(MDS_CDSADIR_CSP_ENCAPSULATED_PRODUCT_RECORDTYPE),
    TABLE(MDS_CDSADIR_CSP_SC_INFO_RECORDTYPE),
    TABLE(MDS_CDSADIR_DL_PRIMARY_RECORDTYPE),
    TABLE(MDS_CDSADIR_DL_ENCAPSULATED_PRODUCT_RECORDTYPE),
    TABLE(MDS_CDSADIR_CL_PRIMARY_RECORDTYPE),
    TABLE(MDS_CDSADIR_CL_ENCAPSULATED_PRODUCT_RECORDTYPE),
    TABLE(MDS_CDSADIR_TP_PRIMARY_RECORDTYPE),
    TABLE(MDS_CDSADIR_TP_OIDS_RECORDTYPE),
    TABLE(MDS_CDSADIR_TP_ENCAPSULATED_PRODUCT_RECORDTYPE),
    TABLE(MDS_CDSADIR_EMM_PRIMARY_RECORDTYPE),
    TABLE(MDS_CDSADIR_AC_PRIMARY_RECORDTYPE),
    TABLE(MDS_CDSADIR_KR_PRIMARY_RECORDTYPE),
	
    // marker for the end of the list
    { ~0U, NULL }
};

MDSModule &
MDSModule::get ()
{
    return mModuleNexus ();
}

MDSModule::MDSModule ()
    :	mDatabaseManager(kTableNames),
	    mLastScanTime((time_t)0),
		mServerMode(false)
{
	mDbPath[0] = '\0';
}

/*
 * Called upon unload or process death by CleanModuleNexus.
 */
MDSModule::~MDSModule ()
{
	/* TBD - close all DBs */
}

void MDSModule::lastScanIsNow()
{
	mLastScanTime = Time::now();
}

double MDSModule::timeSinceLastScan()
{
	Time::Interval delta = Time::now() - mLastScanTime;
	return delta.seconds();
}

void MDSModule::getDbPath(
	char *path)
{
	StLock<Mutex> _(mDbPathLock);
	strcpy(path, mDbPath);
}

void MDSModule::setDbPath(const char *path)
{
	StLock<Mutex> _(mDbPathLock);
	/* caller assures this, and this is private to this module */
	assert(strlen(path) <= MAXPATHLEN);
	strcpy(mDbPath, path);
}

void MDSModule::setServerMode()
{
	secdebug("MDSModule", "setting global server mode");
	mServerMode = true;
}

} // end namespace Security
