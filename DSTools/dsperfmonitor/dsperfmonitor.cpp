/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header dsperfmonitor
 * Tool used to extract DirectoryService API statistics.
 */


#include <stdio.h>
#include <string.h>

#include <DirectoryService/DirectoryService.h>
#include <DirectoryServiceCore/DirectoryServiceCore.h>
#include "dstools_version.h"

#define kMaxArgs		2		// -a | -d | -dump | -flush
#define kMinArgs		2

void SendDSEvent( tPluginCustomCallRequestCode event );

void PrintHelpInfo( void )
{
	printf( "Usage: dsperfmonitor -a | -d | -dump | -flush \n\t-a\tactivate API stat gathering\n\t-d\tdeactivate API stat gathering and dump to system.log\n\t-dump\tdump stats to system.log\n\t-flush\treset statistics\n" );
}

int main(int argc, char *argv[])
{
	if ( argc > kMaxArgs || argc < kMinArgs )
    {
        PrintHelpInfo();
        return -1;
    }
	
	if ( strcmp(argv[1], "-appleversion") == 0 )
        dsToolAppleVersionExit( argv[0] );
	
	if ( strcmp(argv[1], "-a") == 0 ) 
	{
		SendDSEvent( eDSCustomCallActivatePerfMonitor );
		printf( "DirectoryService API statistics are now being gathered\n" );
	}
	else if ( strcmp(argv[1], "-d") == 0 )
	{
		SendDSEvent( eDSCustomCallDeactivatePerfMonitor );
		printf( "DirectoryService API statistics have been turned off and dumped to /var/log/system.log\n" );
	}
	else if ( strcmp(argv[1], "-dump") == 0 )
	{
		SendDSEvent( eDSCustomCallDumpStatsPerfMonitor );
		printf( "DirectoryService API statistics have been dumped to /var/log/system.log\n" );
	}
	else if ( strcmp(argv[1], "-flush") == 0 )
	{
		SendDSEvent( eDSCustomCallFlushStatsPerfMonitor );
		printf( "DirectoryService API statistics reset\n" );
	}
	else
	{
        PrintHelpInfo();
        return -1;
	}
	
	return 0;
}

void SendDSEvent( tPluginCustomCallRequestCode event )
{
	// read the file through Directory Services since it is only root readable.
	tDirNodeReference			nodeRef				= 0;
	tDataList				   *dataList			= NULL;
	tDataBuffer				   *customBuff1			= NULL;
	tDataBuffer				   *emptyBuff			= NULL;
	tDirReference				dsRef				= 0;
	long						status				= eDSNoErr;

    status = dsOpenDirService(&dsRef);

	do
	{
		dataList = dsBuildListFromStrings( dsRef, "Configure", NULL );
		if (dataList == NULL) break;
		
		status = dsOpenDirNode( dsRef, dataList, &nodeRef );
		if (status != eDSNoErr)
		{
			printf( "dsOpenDirNode returned %ld\n", status );
			break;
		}

		// get data
		emptyBuff = dsDataBufferAllocate( dsRef, 1 );
		if (emptyBuff == NULL) break;
		
		customBuff1 = dsDataBufferAllocate( dsRef, 1 );
		if (customBuff1 == NULL) break;

		do
		{
			status = dsDoPlugInCustomCall( nodeRef, event, emptyBuff, customBuff1 );
			if ( status == eDSBufferTooSmall )
			{
				unsigned long buffSize = customBuff1->fBufferSize;
				dsDataBufferDeAllocate( dsRef, customBuff1 );
				customBuff1 = dsDataBufferAllocate( dsRef, buffSize*2 );
			}
		} while (status == eDSBufferTooSmall);

		if (status != eDSNoErr)
		{
			printf( "dsDoPlugInCustomCall returned %ld\n", status );
			break;
		}
	} while ( false );

	if (emptyBuff != NULL)
	{
		dsDataBufferDeAllocate( dsRef, emptyBuff );
		emptyBuff = NULL;
	}
	if (customBuff1 != NULL)
	{
		dsDataBufferDeAllocate( dsRef, customBuff1 );
		customBuff1 = NULL;
	}	
}