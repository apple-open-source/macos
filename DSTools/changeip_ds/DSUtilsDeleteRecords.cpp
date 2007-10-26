/*
 *  DSUtilsDeleteRecords.cpp
 *  NeST
 *
 *  Created by admin on Mon Sep 29 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include <PasswordServer/AuthFile.h>
#include <PasswordServer/CReplicaFile.h>

#include "DSUtilsDeleteRecords.h"
#include "Common.h"

DSUtilsDeleteRecords::DSUtilsDeleteRecords( bool removeDefaultIPRec ) : DSUtils()
{
	mRemoveDefaultIPRec = removeDefaultIPRec;
}


DSUtilsDeleteRecords::~DSUtilsDeleteRecords()
{
}


tDirStatus
DSUtilsDeleteRecords::DoActionOnCurrentNode( void )
{
    tDirStatus					status				= eDSNoErr;
    tRecordReference			recordRef			= 0;
	CReplicaFile				replicaFile;
	char						replicaName[256];
	char						recordName[256];
	
	if ( mRemoveDefaultIPRec )
	{
		status = this->OpenRecord( kDSStdRecordTypeConfig, kPWConfigDefaultRecordName, &recordRef );
		if ( status == eDSNoErr )
		{
			dsDeleteRecord( recordRef );
			recordRef = 0;
		}
	}
	
	if ( replicaFile.GetUniqueID( replicaName ) )
	{
		strcpy( recordName, kPWConfigRecordPrefix );
		strcat( recordName, replicaName );
		
		status = this->OpenRecord( kDSStdRecordTypeConfig, recordName, &recordRef );
		if ( status == eDSNoErr )
		{
			dsDeleteRecord( recordRef );
			recordRef = 0;
		}
	}
	
	return status;
}
