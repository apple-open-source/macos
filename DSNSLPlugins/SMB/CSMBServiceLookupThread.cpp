/*
 *  CSMBServiceLookupThread.cpp
 *  DSSMBPlugIn
 *
 *  Created by imlucid on Wed Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

//#include <Carbon/Carbon.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "CommandLineUtilities.h"
#include "CNSLHeaders.h"
#include "CSMBPlugin.h"
#include "CSMBServiceLookupThread.h"

#ifdef __APPLE_NMBLOOKUP_HACK_2987131
CFStringEncoding GetWindowsSystemEncodingEquivalent( void );
#endif

CSMBServiceLookupThread::CSMBServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep )
    : CNSLServiceLookupThread( parentPlugin, serviceType, nodeDirRep )
{
	DBGLOG( "CSMBServiceLookupThread::CSMBServiceLookupThread\n" );
}

CSMBServiceLookupThread::~CSMBServiceLookupThread()
{
	DBGLOG( "CSMBServiceLookupThread::~CSMBServiceLookupThread\n" );
}

void* CSMBServiceLookupThread::Run( void )
{
	DBGLOG( "CSMBServiceLookupThread::Run\n" );

    char		command[255];
    char*		resultPtr = NULL;
    char*		curPtr = NULL;
    char*		curResult = NULL;
    Boolean		canceled = false;
    char		workgroup[20] = {0};
	const char*	argv[6] = {0};

	if ( !::CFStringGetCString(GetNodeName(), workgroup, sizeof(workgroup), kCFStringEncodingUTF8) )
		return NULL;
	
    if ( ((CSMBPlugin*)GetParentPlugin())->IsWINSWorkgroup( workgroup ) )
	{
		sprintf( command, "/usr/bin/nmblookup -U %s -T %s", ((CSMBPlugin*)GetParentPlugin())->GetWinsServer(), workgroup );
		argv[0] = "/usr/bin/nmblookup";
		argv[1] = "-U";
		argv[2] = ((CSMBPlugin*)GetParentPlugin())->GetWinsServer();
		argv[3] = "-T";
		argv[4] = workgroup;
	}
	else
	{
		sprintf( command, "/usr/bin/nmblookup -T %s", workgroup );
		argv[0] = "/usr/bin/nmblookup";
		if ( ((CSMBPlugin*)GetParentPlugin())->GetBroadcastAdddress() )
		{
			argv[1] = "-B";
			argv[2] = ((CSMBPlugin*)GetParentPlugin())->GetBroadcastAdddress();
			argv[3] = "-T";
			argv[4] = workgroup;
		}
		else
		{
			DBGLOG( "CSMBNodeLookupThread::Run, no broadcast address skipping lookup\n" );
			return NULL;
		}
	}
	
    if ( myexecutecommandas( NULL, "/usr/bin/nmblookup", argv, false, kTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
    {
        DBGLOG( "CSMBServiceLookupThread %s failed\n", command );
    }
    else if ( resultPtr )
    {
        DBGLOG( "CSMBServiceLookupThread::Run, resultPtr = 0x%lx\n", (UInt32)resultPtr );
        DBGLOG( "%s\n", resultPtr );
        
        if ( !ExceptionInResult(resultPtr) )
        {        
			
            curPtr = resultPtr;
            curResult = curPtr;
			
            while( (curResult = GetNextMachine(&curPtr)) != NULL )
            {
				SMBServiceLookupNotifier( curResult );
				free( curResult );
            }

            DBGLOG( "CSMBServiceLookupThread finished reading\n" );
        }
        
        free( resultPtr );
    }

    return NULL;
}

char* CSMBServiceLookupThread::GetNextMachine( char** buffer )
{
	if ( !buffer || !(*buffer) )
		return NULL;
		
	char*	nextLine = strstr( *buffer, "\n" );
	char*	machineName = NULL;
	char	testString[1024];
	char*	curPtr = *buffer;
	
	DBGLOG( "CSMBServiceLookupThread::GetNextMachine, parsing %s\n", *buffer );
	while ( !machineName )
	{
		if ( nextLine )
		{
			*nextLine = '\0';
			nextLine++;
		}
		
		if ( sscanf( curPtr, "%s", testString ) )
		{
			long	ignore;

			DBGLOG( "CSMBServiceLookupThread::GetNextMachine, testing %s to see if its an IPAddress\n", testString );
			if ( testString[strlen(testString)-1] == ',' )
				testString[strlen(testString)-1] = '\0';			// nmblookup puts a comma at the end of this name
			
			if ( IsDNSName(testString) || IsIPAddress( testString, &ignore ) )
			{
				machineName = (char*)malloc(strlen(testString)+1);
				sprintf( machineName, testString );
				
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
			DBGLOG( "CSMBServiceLookupThread::GetNextMachine, try next line: %s\n", curPtr );
		}
		else
			break;
	}

	return machineName;
}

char* CSMBServiceLookupThread::GetMachineName( char* machineAddress )
{
    FILE*		lookupFP;
    char		resultBuffer[1024] = {0};
    char		result[256];
    char*		curPtr;
    char		command[255];
    OSStatus	status = noErr;
    int			scanResult;
    
    DBGLOG( "CSMBServiceLookupThread::GetMachineName: %s\n", machineAddress );
    sprintf( command, "nmblookup -A %s", machineAddress );
    lookupFP = popen( command, "r" );
    
    if ( lookupFP <= 0 )
        printf( "popen failed\n" );
    else
    {
        char	curChar[2] = {0};
        
        while( !mCanceled && !status && fread(curChar, 1, 1, lookupFP ) > 0 )
        {
            if ( curChar[0] == '\n' )
            {
                // ok, we have a full line of data in our resultBuffer...
                curPtr = strstr( resultBuffer, "of" );
                
                if ( !curPtr )	// skip if we are in the first line (containing "of")
                {
                    curPtr = resultBuffer;
                    
                    while ( isblank( *curPtr ) && *curPtr != '\n' )
                        curPtr++;
                    
                    scanResult = sscanf( resultBuffer, "%s", result );
                    
                    // now we want to see if this machine has file services running
                    if ( scanResult > 0 && strstr( resultBuffer, "<20>" ) )
                    {
                        scanResult = sscanf( curPtr, "%s\n", result );
                        
                        if ( scanResult > 0 && result[0] != '\0' )
                        {
							char*		returnResult = (char*)malloc( strlen(result) + 1 );
							strcpy( returnResult, result );

							pclose( lookupFP );
							return returnResult;
//                            status = SMBServiceLookupNotifier( result );
                        }
                    }
                }
                
                resultBuffer[0] = '\0';
            }
            else
            {
                strcat( resultBuffer, curChar );
            }
        }
        
        pclose( lookupFP );
    }
    
    return NULL;
}


OSStatus CSMBServiceLookupThread::SMBServiceLookupNotifier( char* machineName )
{
    OSStatus		status = noErr;
    char			smbURL[1024];
    
    if ( !AreWeCanceled() && machineName )
    {
        CNSLResult* newResult = new CNSLResult();
        
        sprintf( smbURL, "smb://%s", machineName );
        DBGLOG( "SMBServiceLookupNotifier creating new result with url:%s\n", smbURL );

        newResult->SetURL( smbURL );
        newResult->SetServiceType( "smb" );
        newResult->AddAttribute( kDSNAttrDNSName, machineName );		// set the host name so that we can filter this out if needed

		// now for the name, just make it the hostname
		long	ipAdrs;
		if ( IsIPAddress( machineName, &ipAdrs ) )
		{
			// oops, no dns name.  We can either just return the IP address, or do another lookup and get the SMB name
			// go with IP for now.
			char* name = GetMachineName( machineName );
			
			if ( name )
			{
#ifdef __APPLE_NMBLOOKUP_HACK_2987131
				// we are asking for raw bytes and we will try and figure out the encoding
				CFStringRef		nameRef = CFStringCreateWithCString( NULL, name, GetWindowsSystemEncodingEquivalent() );
				
				if ( !nameRef )
				{
					// this wasn't the encoding needed.  Try ASCII, non-7bit are treated as utf8
					nameRef = CFStringCreateWithCString( NULL, name, kCFStringEncodingASCII );
				}
				
				if ( nameRef )
				{
					newResult->AddAttribute( CFSTR(kDSNAttrRecordName), nameRef );		// this should be what is displayed
					CFRelease( nameRef );
				}
				else
					newResult->AddAttribute( kDSNAttrRecordName, machineName );	// just use the IP address
#else
				newResult->AddAttribute( kDSNAttrRecordName, name );		// this should be what is displayed
#endif				
				free( name );
			}
			else
				newResult->AddAttribute( kDSNAttrRecordName, machineName );	// just use the IP address

			AddResult( newResult );
		}
		else
		{
			char*	firstDot = strstr( machineName, "." );
			if ( firstDot )
				*firstDot = '\0';
			
			newResult->AddAttribute( kDSNAttrRecordName, machineName );		// this should be what is displayed
			AddResult( newResult );
		}
    }

    return status;
}

#ifdef __APPLE_NMBLOOKUP_HACK_2987131
CFStringEncoding GetWindowsSystemEncodingEquivalent( void )
{
	// So we want to try and map our Mac System encoding to what the equivalent windows encoding would be on the
	// same network  (i.e. kCFStringEncodingMacJapanese to kCFStringEncodingShiftJIS)
	CFStringEncoding	encoding = NSLGetSystemEncoding();		// use our version due to bug where CFGetSystemEncoding doesn't work for boot processes
	
	switch ( encoding )
	{
		case	kCFStringEncodingMacRoman:
			encoding = kCFStringEncodingISOLatin1;
		break;
		
		case kCFStringEncodingMacJapanese:
			encoding = kCFStringEncodingShiftJIS;
		break;
		
		case kCFStringEncodingMacChineseSimp:
			encoding = kCFStringEncodingHZ_GB_2312;
		break;
		
		case kCFStringEncodingMacChineseTrad:
			encoding = kCFStringEncodingBig5_HKSCS_1999;
		break;
		
		case kCFStringEncodingMacKorean:
			encoding = kCFStringEncodingKSC_5601_92_Johab;
		break;
		
		case kCFStringEncodingMacArabic:
			encoding = kCFStringEncodingWindowsArabic;
		break;
		
		case kCFStringEncodingMacHebrew:
			encoding = kCFStringEncodingWindowsHebrew;
		break;
		
		case kCFStringEncodingMacGreek:
			encoding = kCFStringEncodingWindowsGreek;
		break;
		
		case kCFStringEncodingMacCyrillic:
			encoding = kCFStringEncodingWindowsCyrillic;
		break;
	}
	
	return encoding;
}
#endif
