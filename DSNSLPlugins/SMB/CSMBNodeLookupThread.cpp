/*
 *  CSMBNodeLookupThread.cpp
 *  DSSMBPlugIn
 *
 *  Created by imlucid on Wed Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "CommandLineUtilities.h"
#include "CSMBPlugin.h"
#include "CSMBNodeLookupThread.h"
 
#define kMinNumSecsToWaitAfterStartup	120	// two minutes?
UInt32 GetSecondsSinceStartup( void );	// temporary workaround until we can get loaded after startup

CSMBNodeLookupThread::CSMBNodeLookupThread( CNSLPlugin* parentPlugin )
    : CNSLNodeLookupThread( parentPlugin )
{
	DBGLOG( "CSMBNodeLookupThread::CSMBNodeLookupThread\n" );
}

CSMBNodeLookupThread::~CSMBNodeLookupThread()
{
	DBGLOG( "CSMBNodeLookupThread::~CSMBNodeLookupThread\n" );
}

void* CSMBNodeLookupThread::Run( void )
{
	DBGLOG( "CSMBNodeLookupThread::Run\n" );
    
/*    while ( GetSecondsSinceStartup() < kMinNumSecsToWaitAfterStartup )
        sleep( 30 );		// just sleep for another 30 seconds
*/
    if ( ((CSMBPlugin*)GetParentPlugin())->GetWinsServer() )
		DoMasterBrowserLookup( ((CSMBPlugin*)GetParentPlugin())->GetWinsServer() );
	
	// Now do additional local brodcast lookup too?
	DoMasterBrowserLookup( NULL );
	
	((CSMBPlugin*)GetParentPlugin())->NodeLookupIsCurrent();
	
    return NULL;
}

void CSMBNodeLookupThread::DoMasterBrowserLookup( const char* winsServer )
{
   // first look up the master browser
	DBGLOG( "CSMBNodeLookupThread::DoMasterBrowserLookup\n" );
    char*		resultPtr = NULL;
    char*		curPtr = NULL;
    char*		curResult = NULL;
	const char*	argv[5] = {0};
    Boolean		canceled = false;
    OSStatus	status = noErr;
	
	if ( winsServer )
	{
		DBGLOG( "CSMBNodeLookupThread::DoMasterBrowserLookup, using WINS Server: %s\n", winsServer );
	
		argv[0] = "/usr/bin/nmblookup";
		argv[1] = "-U";
		argv[2] = winsServer;
		argv[3] = "-M";
		argv[4] = "-";
	}
	else
	{

		argv[0] = "/usr/bin/nmblookup";
		argv[1] = "-M";

		if ( ((CSMBPlugin*)GetParentPlugin())->GetBroadcastAdddress() )
		{
			DBGLOG( "CSMBNodeLookupThread::DoMasterBrowserLookup, using Broadcast address: %s\n", ((CSMBPlugin*)GetParentPlugin())->GetBroadcastAdddress() );
			argv[2] = "-B";
			argv[3] = ((CSMBPlugin*)GetParentPlugin())->GetBroadcastAdddress();
			argv[4] = "-";
		}
		else
		{
			DBGLOG( "CSMBNodeLookupThread::DoMasterBrowserLookup, no broadcast address skipping lookup\n" );
			return;
		}
	}
//		sprintf( command, "/usr/bin/nmblookup -M -" );

    if ( myexecutecommandas( NULL, "/usr/bin/nmblookup", argv, false, kTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
    {
        DBGLOG( "CSMBNodeLookupThread nmblookup -M - failed\n" );
    }
    else if ( resultPtr )
    {
        DBGLOG( "CSMBNodeLookupThread::Run, resultPtr = 0x%lx\n", (UInt32)resultPtr );
        DBGLOG( "%s\n", resultPtr );
        
        if ( !ExceptionInResult(resultPtr) )
        {        
            curPtr = resultPtr;
            curResult = curPtr;
			
            while( (curResult = GetNextMasterBrowser(&curPtr)) != NULL )
            {
				status = DoMachineLookup( curResult, winsServer );
				free( curResult );
            }

            DBGLOG( "CSMBNodeLookupThread finished reading\n" );
        }
        
        free( resultPtr );
//        pclose( lookupFP );
    }
}

char* CSMBNodeLookupThread::GetNextMasterBrowser( char** buffer )
{
	long	addrOfMasterBrowser;
	char*	nextLine = strstr( *buffer, "\n" );
	char*	masterBrowser = NULL;
	char	testString[1024];
	char*	curPtr = *buffer;
	
	DBGLOG( "CSMBNodeLookupThread::GetNextMasterBrowser, parsing %s\n", *buffer );
	while ( !masterBrowser )
	{
		if ( nextLine )
		{
			*nextLine = '\0';
			nextLine++;
		}
		
		if ( sscanf( curPtr, "%s", testString ) )
		{
			DBGLOG( "CSMBNodeLookupThread::GetNextMasterBrowser, testing %s to see if its an IPAddress\n", testString );
			if ( IsIPAddress(testString, &addrOfMasterBrowser) )
			{
				DBGLOG( "CSMBNodeLookupThread::GetNextMasterBrowser, testing %s to see if its an IPAddress\n", testString );
				masterBrowser = (char*)malloc(strlen(testString)+1);
				sprintf( masterBrowser, testString );
				
				*buffer = nextLine;
				if ( *buffer )
					*buffer++;
				break;
			}
		}
		
		curPtr = nextLine;
		if ( curPtr )
		{
			nextLine = strstr( curPtr, "\n" );	// look for next line
			DBGLOG( "CSMBNodeLookupThread::GetNextMasterBrowser, try next line: %s\n", curPtr );
		}
		else
			break;
	}

	return masterBrowser;
}

OSStatus CSMBNodeLookupThread::DoMachineLookup( const char* machineAddress, const char* winsServer )
{
    char*		resultPtr = NULL;
    char*		curPtr = NULL;
    char*		curResult = NULL;
    OSStatus	status = noErr;
    Boolean		canceled = false;
	const char*	argv[5] = {0};

    DBGLOG( "CSMBNodeLookupThread::DoMachineLookup: %s\n", machineAddress );
	argv[0] = "/usr/bin/nmblookup";
	argv[1] = "-A";
	argv[2] = machineAddress;

    if ( myexecutecommandas( NULL, "/usr/bin/nmblookup", argv, false, kTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
    {
		char		command[255] = {0};
		sprintf( command, "/usr/bin/nmblookup -A %s", machineAddress );
        DBGLOG( "CSMBNodeLookupThread::DoMachineLookup %s failed\n", command );
    }
    else if ( resultPtr )
    {
        if ( !ExceptionInResult(resultPtr) )
        {        
            curPtr = resultPtr;
            curResult = curPtr;
			
            while( (curResult = GetNextWorkgroup(&curPtr)) != NULL )
            {
				AddResult( curResult );
				if ( winsServer )
					((CSMBPlugin*)GetParentPlugin())->AddWINSWorkgroup( curResult );
					
				free( curResult );
            }

            DBGLOG( "CSMBNodeLookupThread finished reading\n" );
        }
        
        free( resultPtr );
    }

    return status;
}

char* CSMBNodeLookupThread::GetNextWorkgroup( char** buffer )
{
	char*	nextLine = strstr( *buffer, "\n" );
	char*	workgroup = NULL;
	char*	curPtr = *buffer;
	char*	eoWorkgroupName = NULL;
	
	DBGLOG( "CSMBNodeLookupThread::GetNextWorkgroup, parsing %s\n", *buffer );
	while ( !workgroup )
	{
		if ( nextLine )
		{
			*nextLine = '\0';
			nextLine++;
		}
		
		if ( curPtr && (eoWorkgroupName = strstr(curPtr,"<00> - <GROUP>")) )
		{
			DBGLOG( "CSMBNodeLookupThread::GetNextWorkgroup, found a workgroup line (%s)\n", curPtr );
		
			DBGLOG( "CSMBNodeLookupThread::GetNextWorkgroup, first char is 0x%x\n", *curPtr );
			
			while ( *curPtr == '\t')		// skip  tabs
			{
				curPtr++;
			}
			
			eoWorkgroupName--;	// back up
			
			while ( isspace(*eoWorkgroupName) )
				eoWorkgroupName--;					// go back
				
			*(++eoWorkgroupName) = '\0';			// allow for parsing
			
			DBGLOG( "CSMBNodeLookupThread::GetNextWorkgroup, workgroup found is (%s)\n", curPtr );
			workgroup = (char*)malloc(strlen(curPtr)+1);
			strcpy( workgroup, curPtr );
			
			*buffer = nextLine;
			if ( *buffer )
				*buffer++;
			break;
		}
		
		curPtr = nextLine;
		if ( curPtr )
		{
			nextLine = strstr( curPtr, "\n" );	// look for next line
			DBGLOG( "CSMBNodeLookupThread::GetNextWorkgroup, try next line: %s\n", curPtr );
		}
		else
			break;
	}

	return workgroup;
}


//-----------------------------------------------------------------------------
//	GetSecondsSinceStartup
//
//	Returns the ticks since startup.  TickCount is not usuable because it is above
//  us in the Carbon.  Not accessable by CarbonCore.
//-----------------------------------------------------------------------------

UInt32 GetSecondsSinceStartup( void )
{
	#define ASECONDINMICROSECONDS     (1000000)

        struct timeval          currentTime;
        struct timeval          bootTime; 
        struct timezone         tz;
        UInt32		       	 	secs;

        int mib[2];   
        size_t len;   

        mib[0] = CTL_KERN;   
        mib[1] = KERN_BOOTTIME;   
        len = sizeof(bootTime);   

        // Get System Boot time since 1970
        sysctl(mib, 2, &bootTime, &len, NULL, 0);
 
        gettimeofday( &currentTime, &tz ); 
        secs = (currentTime.tv_sec - bootTime.tv_sec)*60 + currentTime.tv_usec/ASECONDINMICROSECONDS;

        return secs;
}

