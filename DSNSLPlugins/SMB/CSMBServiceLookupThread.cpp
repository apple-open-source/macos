/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 *  @header CSMBServiceLookupThread
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "CommandLineUtilities.h"
#include "CNSLHeaders.h"
#include "LMBDiscoverer.h"
#include "CSMBPlugin.h"
#include "CSMBServiceLookupThread.h"

const CFStringRef	kSMBColonSlashSlashSAFE_CFSTR = CFSTR("smb://");
const CFStringRef	kColonSAFE_CFSTR = CFSTR(";");

CSMBServiceLookupThread::CSMBServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep, CFArrayRef lmbListRef, const char* winsServer )
    : CNSLServiceLookupThread( parentPlugin, serviceType, nodeDirRep )
{
	DBGLOG( "CSMBServiceLookupThread::CSMBServiceLookupThread\n" );
	mLMBsRef = lmbListRef;
	if ( mLMBsRef )
		CFRetain( mLMBsRef );
	mResultList = NULL;
	mWINSServer = winsServer;	// if they pass this in, use it in your url creation
}

CSMBServiceLookupThread::~CSMBServiceLookupThread()
{
	DBGLOG( "CSMBServiceLookupThread::~CSMBServiceLookupThread\n" );
	if ( mLMBsRef )
		CFRelease( mLMBsRef );
	mLMBsRef = NULL;
	
	if ( mResultList )
		CFRelease( mResultList );
	mResultList = NULL;
}

void* CSMBServiceLookupThread::Run( void )
{
    char		workgroup[20] = {0};

	DBGLOG( "CSMBServiceLookupThread::Run\n" );
	if ( !::CFStringGetCString(GetNodeName(), workgroup, sizeof(workgroup), GetWindowsSystemEncodingEquivalent()) )
		return NULL;
	
	GetWorkgroupServers( workgroup );
	
	return NULL;
}

void CSMBServiceLookupThread::GetWorkgroupServers( char* workgroup )
{
    char*		resultPtr = NULL;
	char*		argv[11] = {0};
	char		lmbName[256] = {0,};
	CFStringRef	workgroupRef = GetNodeName();
    Boolean		canceled = false;
	Boolean		resultsFound = false;
	
	if ( !mLMBsRef || CFArrayGetCount(mLMBsRef) == 0 )
	{
		DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers, couldn't get lmb for workgroup: %s, do broadcast lookup\n", workgroup );
	}
	else
	{
		CFIndex		numLMBs = CFArrayGetCount(mLMBsRef);
		
		for ( CFIndex i=0; i<numLMBs && !resultsFound; i++ )
		{
			CFStringRef		lmbNameRef = (CFStringRef)CFArrayGetValueAtIndex( mLMBsRef, i );
			
			CFStringGetCString( lmbNameRef, lmbName, sizeof(lmbName), GetWindowsSystemEncodingEquivalent() );
				
			argv[0] = "/usr/bin/smbclient";
			argv[1] = "-W";
			argv[2] = workgroup;
			argv[3] = "-p";
			argv[4] = "139";
			argv[5] = "-NL";
			argv[6] = lmbName;
			
			for ( int tryNum=1; tryNum<=2 && resultsFound==false; tryNum++ )
			{
				if ( tryNum == 1 )
				{
					argv[7] = "-U%";		// first try use USER or LOGNAME env variable
					argv[8] = "-s";
					argv[9] = kBrowsingConfFilePath;
				}
				else
				{
					argv[7] = "-s";
					argv[8] = kBrowsingConfFilePath;
					argv[9] = NULL;			// second try use NULL or anon
				}
		
				DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers calling smbclient -W %s -NL %s -U\%\n", workgroup, lmbName );
				if ( myexecutecommandas( NULL, "/usr/bin/smbclient", argv, false, kLMBGoodTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
				{
					DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers smbclient -W %s -NL %s -U\%\n", workgroup, lmbName );
				}
				else if ( resultPtr )
				{
					DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers, resultPtr = 0x%lx, length = %ld\n", (UInt32)resultPtr, strlen(resultPtr) );
					DBGLOG( "%s\n", resultPtr );
					
					if ( ExceptionInResult(resultPtr) )
					{
						syslog( LOG_INFO, "Unable to browse contents of workgroup (%s) due to %s returning an error\n", workgroup, lmbName );
						DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers, got an error, clearing cached lmb results\n" );
						((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer()->MarkLMBAsBad( lmbNameRef );
						((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer()->ClearLMBForWorkgroup(workgroupRef, lmbNameRef);
					}
					else
					{        
						char*		resultPtrCurrent = NULL;
						CFArrayRef 	results = ParseOutStringsFromSMBClientResult( resultPtr, "Server", "Comment", &resultPtrCurrent );
			
						if ( results )
						{
							for ( CFIndex i=CFArrayGetCount(results)-2; i>=0; i-=2 )
							{
								resultsFound = true;
								AddServiceResult( workgroupRef, (CFStringRef)CFArrayGetValueAtIndex(results, i), (CFStringRef)CFArrayGetValueAtIndex(results, i+1) );	// adding workgroup as key, lmb as value
							}
							
							if ( !AreWeCanceled() )
								GetNodeToSearch()->AddServices( mResultList );	
							else
							{
								CFIndex		numSearches = ::CFArrayGetCount( mResultList );
								CNSLResult*	result;
								for ( CFIndex i=numSearches-1; i>=0; i-- )
								{
									result = (CNSLResult*)::CFArrayGetValueAtIndex( mResultList, i );
									::CFArrayRemoveValueAtIndex( mResultList, i );
									delete result;
								}
							}
							
							CFRelease( results );
						}
			
						// now parse out the lmb results while we have the data!
						CFArrayRef lmbResults = ParseOutStringsFromSMBClientResult( resultPtrCurrent, "Workgroup", "Master" );
						
						if ( lmbResults )
						{
							DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers is now parsing out the %d results for workgroups and their masters\n", CFArrayGetCount(lmbResults)/2 );
							for ( CFIndex i=CFArrayGetCount(lmbResults)-2; i>=0; i-=2 )
							{
								CFStringRef		workgroupRef = (CFStringRef)CFArrayGetValueAtIndex(lmbResults, i);
								CFStringRef		lmbNameRef = (CFStringRef)CFArrayGetValueAtIndex(lmbResults, i+1);
								
								if ( workgroupRef && CFStringGetLength( workgroupRef ) > 0 && lmbNameRef && CFStringGetLength( lmbNameRef ) > 0 && !((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer()->IsLMBKnown( workgroupRef, lmbNameRef ) )		// do we already know about this workgroup?
								{
									((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer()->AddToCachedResults( workgroupRef, lmbNameRef );		// if this is valid, add it to our list - don't fire off new threads
								}
							}
							
							CFRelease( lmbResults );
						}
					}
					
					free( resultPtr );
					resultPtr = NULL;
				}
			}
		}
	}

	if ( !resultsFound )
	{
		DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers, we have no results from LMBs for this, just do a broadcast lookup\n" );
		DoBroadcastLookup( workgroup, workgroupRef );	// if we have no results from LMBs for this, just do a broadcast lookup
	}
}

void CSMBServiceLookupThread::DoBroadcastLookup( char* workgroup, CFStringRef workgroupRef )
{
	DBGLOG( "CSMBServiceLookupThread::DoBroadcastLookup\n" );

    char*			resultPtr = NULL;
    char*			curPtr = NULL;
    char*			curResult = NULL;
    Boolean			canceled = false;
	Boolean			foundResult = false;
	char*			argv[7] = {0};
	char*			broadcastAddress = NULL;
	
	if ( !workgroupRef || !workgroup )
	{
		DBGLOG( "CSMBServiceLookupThread::DoBroadcastLookup, no workgroup!\n" );
		return;
	}
		
	broadcastAddress = ((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer()->CopyBroadcastAddress();
	argv[0] = "/usr/bin/nmblookup";
	
	if ( broadcastAddress )
	{
		argv[1] = "-B";
		argv[2] = broadcastAddress;
		argv[3] = workgroup;
		argv[4] = "-s";
		argv[5] = kBrowsingConfFilePath;
		
		if ( myexecutecommandas( NULL, "/usr/bin/nmblookup", argv, false, kTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
		{
			DBGLOG( "CSMBServiceLookupThread /usr/bin/nmblookup %s failed\n", workgroup );
		}
		else if ( resultPtr )
		{
			DBGLOG( "CSMBServiceLookupThread::DoBroadcastLookup, resultPtr = 0x%lx\n", (UInt32)resultPtr );
			DBGLOG( "%s\n", resultPtr );
			
			if ( !strstr(resultPtr, "Exception") )
			{        
				curPtr = resultPtr;
				curResult = curPtr;
				
				while( (curResult = CopyNextMachine(&curPtr)) != NULL )
				{					
					CFStringRef		curName = CFStringCreateWithCString( NULL, curResult, GetWindowsSystemEncodingEquivalent() );
					
					if ( curName )
					{
						foundResult = true;
						AddServiceResult( workgroupRef, curName, CFSTR("") );
					
						CFRelease( curName );
					}
					
					free( curResult );
				}
	
				if ( !AreWeCanceled() )
				{
					GetNodeToSearch()->AddServices( mResultList );	
				}
				else
				{
					CFIndex		numSearches = ::CFArrayGetCount( mResultList );
					CNSLResult*	result;
					for ( CFIndex i=numSearches-1; i>=0; i-- )
					{
						result = (CNSLResult*)::CFArrayGetValueAtIndex( mResultList, i );
						::CFArrayRemoveValueAtIndex( mResultList, i );
						delete result;
					}
				}
				
				DBGLOG( "CSMBServiceLookupThread finished reading\n" );
			}
			
			free( resultPtr );
		}

		if ( foundResult )
		{
			((CSMBPlugin*)GetParentPlugin())->BroadcastServiceLookupSucceededInWorkgroup( workgroupRef );
		}
		else
		{
			// nothing found, let our parent know so they can control query throttling
			((CSMBPlugin*)GetParentPlugin())->BroadcastServiceLookupFailedInWorkgroup( workgroupRef );
		}

		free( broadcastAddress );
	}
	else
	{
		DBGLOG( "CSMBNodeLookupThread::Run, no broadcast address skipping lookup\n" );
	}
}

char* CSMBServiceLookupThread::CopyNextMachine( char** buffer )
{
	if ( !buffer || !(*buffer) )
		return NULL;
		
	char*	nextLine = strstr( *buffer, "\n" );
	char*	machineName = NULL;
	char	testString[1024];
	char*	curPtr = *buffer;
	
	DBGLOG( "CSMBServiceLookupThread::CopyNextMachine, parsing %s\n", *buffer );
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

			DBGLOG( "CSMBServiceLookupThread::CopyNextMachine, testing %s to see if its an IPAddress\n", testString );
			if ( testString[strlen(testString)-1] == ',' )
				testString[strlen(testString)-1] = '\0';			// nmblookup puts a comma at the end of this name
			
			if ( IsDNSName(testString) || IsIPAddress( testString, &ignore ) )
			{
				char*			curPtr;
				char*			resultPtr = NULL;
				Boolean			canceled = false;
				char*			argv[6] = {0};
				
				argv[0] = "/usr/bin/nmblookup";
				
				argv[1] = "-A";
				argv[2] = testString;
				argv[3] = "-s";
				argv[4] = kBrowsingConfFilePath;
			
				DBGLOG( "CSMBServiceLookupThread::CopyNextMachine: %s\n", testString );
			
				if ( myexecutecommandas( NULL, "/usr/bin/nmblookup", argv, false, kTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
				{
					DBGLOG( "CSMBServiceLookupThread::CopyNextMachine /usr/bin/nmblookup -A %s failed\n", testString );
				}
				else if ( resultPtr )
				{
					curPtr = strstr( resultPtr, "<20>" );
					
					if ( curPtr )
					{
					// we should be pointing to the filesharing line
						curPtr--;
						
						while ( curPtr > resultPtr && isblank( *curPtr ) )
							curPtr--;		// move backwards towards the name
						
						if ( curPtr > resultPtr )
							curPtr[1] = '\0';		// make the previous character a null terminated one
						
						while ( curPtr > resultPtr && !isblank( *curPtr ) )
							curPtr--;		// move backwards towards the beginning of the name
			
						if ( curPtr > resultPtr )
						{
							machineName = strdup( &curPtr[1] );
						}
					}
					
					free( resultPtr );
				}

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
			DBGLOG( "CSMBServiceLookupThread::CopyNextMachine, try next line: %s\n", curPtr );
		}
		else
			break;
	}

	return machineName;
}

void CSMBServiceLookupThread::AddServiceResult( CFStringRef workgroupRef, CFStringRef netBIOSRef, CFStringRef commentRef )
{
	CFStringRef			netBIOSNameRef = netBIOSRef;
	CFStringRef			workgroupNameRef = workgroupRef;
	
	if ( !mResultList )
		mResultList = CFArrayCreateMutable( NULL, 0, NULL );
	
	if ( UseCapitalization() )
	{
		CFMutableStringRef		modName= CFStringCreateMutableCopy( NULL, 0, netBIOSNameRef );

		CFStringCapitalize( modName, 0 );	// make pretty for display
		netBIOSNameRef = modName;
		
		modName = CFStringCreateMutableCopy( NULL, 0, workgroupNameRef );
		CFStringUppercase( modName, 0 );	// make uppercase for URL
		workgroupNameRef = modName;
	}

	if ( netBIOSNameRef && commentRef )
	{
		CNSLResult*				newResult = new CNSLResult();
		CFMutableStringRef		smbURLRef = CFStringCreateMutable( NULL, 0 );
		
		newResult->AddAttribute( kDSNAttrRecordNameSAFE_CFSTR, netBIOSNameRef );		// this should be what is displayed
		newResult->AddAttribute( kDS1AttrCommentSAFE_CFSTR, commentRef );			// additional information
	
		if ( smbURLRef )
		{
#define USE_UTF8_FOR_SMB_URL
#ifdef USE_UTF8_FOR_SMB_URL
			CFStringRef             escapedWorkgroup = CFURLCreateStringByAddingPercentEscapes(NULL, workgroupNameRef, NULL, NULL, kCFStringEncodingUTF8);
			CFStringRef             escapedName = CFURLCreateStringByAddingPercentEscapes(NULL, netBIOSRef, NULL, NULL, kCFStringEncodingUTF8);
#else			
			CFStringRef             escapedWorkgroup = CFURLCreateStringByAddingPercentEscapes(NULL, workgroupNameRef, NULL, NULL, GetWindowsSystemEncodingEquivalent());
			CFStringRef             escapedName = CFURLCreateStringByAddingPercentEscapes(NULL, netBIOSRef, NULL, NULL, GetWindowsSystemEncodingEquivalent());
#endif
			if ( escapedWorkgroup && escapedName )
			{
				CFStringAppend( smbURLRef, kSMBColonSlashSlashSAFE_CFSTR );
				CFStringAppend( smbURLRef, escapedWorkgroup );
				CFStringAppend( smbURLRef, kColonSAFE_CFSTR );
				CFStringAppend( smbURLRef, escapedName );

				if ( UseWINSURLMechanism() )
				{
					CFStringAppend( smbURLRef, CFSTR("?WINS=") );
					CFStringAppendCString( smbURLRef, mWINSServer, kCFStringEncodingASCII );
				}
				
				newResult->SetURL( smbURLRef );
				newResult->SetServiceType( "smb" );
				
#ifdef LOG_TO_FILE
	#warning "DEBUG CODE, DO NOT SUBMIT!!!!"
	// let's dump this out to a file
	char	name[256];
	char	workgroup[256];
	char	url[256];
	
	CFStringGetCString( netBIOSNameRef, name, sizeof(name), GetWindowsSystemEncodingEquivalent() );
	CFStringGetCString( workgroupRef, workgroup, sizeof(workgroup), GetWindowsSystemEncodingEquivalent() );
	CFStringGetCString( smbURLRef, url, sizeof(url), GetWindowsSystemEncodingEquivalent() );
	
	FILE* destFP = fopen( "/tmp/myexecutecommandas.out", "a" );
	char				headerString[1024];
	
	if ( destFP )
	{
		sprintf( headerString, "\n**** AddServiceResult ****\n\tNetBIOS Name: %s\n\tWorkgroup: %s\n\tEncoded URL: %s", name, workgroup, url);
		fputs( headerString, destFP );
		fputs( "\n**** endof results *****\n\n", destFP );
		fclose( destFP );
	}
	else
		syslog( LOG_ALERT, "COULD NOT OPEN /tmp/myexecutecommandas.out!\n" );
#endif
			}
			
			if ( escapedWorkgroup )
				CFRelease( escapedWorkgroup );
			
			if ( escapedName )
				CFRelease( escapedName );
				
			CFRelease( smbURLRef );
		}
	
		CFArrayAppendValue( mResultList, newResult );
	}
	
	if ( netBIOSNameRef != netBIOSRef )
		CFRelease( netBIOSNameRef );
	
	if ( workgroupNameRef != workgroupRef )
		CFRelease( workgroupNameRef );
}

Boolean CSMBServiceLookupThread::UseWINSURLMechanism( void )
{
	Boolean		returnValue = false;
	
	if ( mWINSServer )
		returnValue = true;
		
	return returnValue;
}








