/*
 *  DSUtilsDeleteRecords.cpp
 *  NeST
 *
 *  Created by admin on Mon Sep 29 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include <Foundation/Foundation.h>
#include <PasswordServer/AuthFile.h>
#include <PasswordServer/ReplicaFile.h>

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
	ReplicaFile					*replicaFile		= [ReplicaFile new];
	CFStringRef					replicaIDString		= NULL;
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
	
	replicaIDString = [replicaFile getUniqueID];
	if ( replicaIDString != nil )
	{
		strcpy( recordName, kPWConfigRecordPrefix );
		strcat( recordName, [(NSString *)replicaIDString UTF8String] );
		
		status = this->OpenRecord( kDSStdRecordTypeConfig, recordName, &recordRef );
		if ( status == eDSNoErr )
		{
			dsDeleteRecord( recordRef );
			recordRef = 0;
		}
	}
	
	return status;
}
