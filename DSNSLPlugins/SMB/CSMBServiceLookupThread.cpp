/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

CSMBServiceLookupThread::CSMBServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep, CFStringRef lmbNameRef )
    : CNSLServiceLookupThread( parentPlugin, serviceType, nodeDirRep )
{
	DBGLOG( "CSMBServiceLookupThread::CSMBServiceLookupThread\n" );
	mLMBNameRef = lmbNameRef;
	if ( mLMBNameRef )
		CFRetain( mLMBNameRef );
}

CSMBServiceLookupThread::~CSMBServiceLookupThread()
{
	DBGLOG( "CSMBServiceLookupThread::~CSMBServiceLookupThread\n" );
	if ( mLMBNameRef )
		CFRelease( mLMBNameRef );
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
	const char*	argv[9] = {0};
	char		lmbName[256] = {0,};
	CFStringRef	workgroupRef = GetNodeName();
//	CFStringRef	workgroupRef = CFStringCreateWithCString( NULL, workgroup, NSLGetSystemEncoding() );
    Boolean		canceled = false;
	
	if ( !mLMBNameRef )
	{
		DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers, couldn't get lmb for workgroup: %s\n", workgroup );
	}
	else
	{
		CFStringGetCString( mLMBNameRef, lmbName, sizeof(lmbName), GetWindowsSystemEncodingEquivalent() );
//		CFStringGetCString( mLMBNameRef, lmbName, sizeof(lmbName), NSLGetSystemEncoding() );
			
		argv[0] = "/usr/bin/smbclient";
		argv[1] = "-W";
		argv[2] = workgroup;
		argv[3] = "-NL";
		argv[4] = lmbName;
		argv[5] = "-U%";
		argv[6] = "-s";
		argv[7] = kBrowsingConfFilePath;
	
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
				((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer()->MarkLMBAsBad( mLMBNameRef );
				((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer()->ClearLMBForWorkgroup(workgroupRef, mLMBNameRef);
			}
			else
			{        
				char*		resultPtrCurrent = NULL;
				CFArrayRef 	results = ParseOutStringsFromSMBClientResult( resultPtr, "Server", "Comment", &resultPtrCurrent );

				if ( results )
				{
					for ( CFIndex i=CFArrayGetCount(results)-2; i>=0; i-=2 )
					{
						AddServiceResult( workgroupRef, (CFStringRef)CFArrayGetValueAtIndex(results, i), (CFStringRef)CFArrayGetValueAtIndex(results, i+1) );	// adding workgroup as key, lmb as value
					}
					
					CFRelease( results );
				}
				else if ( strstr(resultPtr, "NT_STATUS_ACCESS_DENIED") )
				{
					syslog( LOG_INFO, "Unable to browse contents of workgroup (%s) due to %s returning NT_STATUS_ACCESS_DENIED\n", workgroup, lmbName );
					((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer()->MarkLMBAsBad( mLMBNameRef );
					// at least add the LMB itself?
//					AddServiceResult( workgroupRef, mLMBNameRef, kEmptySAFE_CFSTR );	// adding workgroup as key, lmb as value
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
/*							LMBInterrogationThread*		interrogator = new LMBInterrogationThread();
							interrogator->Initialize( ((CSMBPlugin*)GetParentPlugin())->OurLMBDiscoverer(), lmbNameRef, workgroupRef );
							
							interrogator->Resume();
*/
						}
					}
					
					CFRelease( lmbResults );
				}
			}
			
			free( resultPtr );
			resultPtr = NULL;
		}
	}
	
/*	if ( workgroupRef )
		CFRelease( workgroupRef );
	workgroupRef = NULL;
*/
}

void CSMBServiceLookupThread::AddServiceResult( CFStringRef workgroupRef, CFStringRef netBIOSRef, CFStringRef commentRef )
{
	if ( netBIOSRef && commentRef )
	{
		CNSLResult*				newResult = new CNSLResult();
		CFMutableStringRef		smbURLRef = CFStringCreateMutable( NULL, 0 );
		
		newResult->AddAttribute( kDSNAttrRecordNameSAFE_CFSTR, netBIOSRef );		// this should be what is displayed
		newResult->AddAttribute( kDS1AttrCommentSAFE_CFSTR, commentRef );			// additional information
	
		if ( smbURLRef )
		{
			CFStringRef             escapedWorkgroup = CFURLCreateStringByAddingPercentEscapes(NULL, workgroupRef, NULL, NULL, GetWindowsSystemEncodingEquivalent());
			CFStringRef             escapedName = CFURLCreateStringByAddingPercentEscapes(NULL, netBIOSRef, NULL, NULL, GetWindowsSystemEncodingEquivalent());
			
			if ( escapedWorkgroup && escapedName )
			{
				CFStringAppend( smbURLRef, kSMBColonSlashSlashSAFE_CFSTR );
				CFStringAppend( smbURLRef, escapedWorkgroup );
				CFStringAppend( smbURLRef, kColonSAFE_CFSTR );
				CFStringAppend( smbURLRef, escapedName );

				newResult->SetURL( smbURLRef );
				newResult->SetServiceType( "smb" );
				
#ifdef LOG_TO_FILE
	#warning "DEBUG CODE, DO NOT SUBMIT!!!!"
	// let's dump this out to a file
	char	name[256];
	char	workgroup[256];
	char	url[256];
	
	CFStringGetCString( netBIOSRef, name, sizeof(name), GetWindowsSystemEncodingEquivalent() );
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
	
		AddResult( newResult );
	}
}

