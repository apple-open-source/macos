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
 

#include "LMBDiscoverer.h"
#include "CommandLineUtilities.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

void PrintHelpInfo( void );

#define	kMaxURLLen				1024
#define	kMaxAttributeLen		1024
#define kMaxArgs		7		// [-r url] [-d url] [-a attribute_list]

UInt32 GetCurrentTime( void )			// in seconds
{
    struct	timeval curTime;
    
    if ( gettimeofday( &curTime, NULL ) != 0 )
        fprintf( stderr, "call to gettimeofday returned error: %s", strerror(errno) );
    
    return curTime.tv_sec;
}

CFStringEncoding	gsEncoding = kCFStringEncodingInvalidId;
void AddServiceResult( CFStringRef workgroupRef, CFStringRef netBIOSRef, CFStringRef commentRef );
void DiscoverLMBs( LMBDiscoverer* discoverer = NULL );
void GetWorkgroupServers( char* workgroup );

CFStringEncoding NSLGetSystemEncoding( void )
{
	if ( gsEncoding == kCFStringEncodingInvalidId )
	{
		// need to parse out the encoding from /var/root/.CFUserTextEncoding
		FILE *				fp;
		char 				buf[1024];
		CFStringEncoding	encoding = 0;
		
		fp = fopen("/var/root/.CFUserTextEncoding","r");
		
		if (fp == NULL) {
			DBGLOG( "NSLGetSystemEncoding: Could not open config file, return 0 (MacRoman)" );
			return 0;
		}
		
		if ( fgets(buf,sizeof(buf),fp) != NULL) 
		{
			int	i = 0;
			
			while ( buf[i] != '\0' && buf[i] != ':' )
				i++;
	
			buf[i] = '\0';
			
			char*	endPtr = NULL;
			encoding = strtol(buf,&endPtr,10);
		}
		
		fclose(fp);
		
		gsEncoding = encoding;
	}
	
	DBGLOG( "NSLGetSystemEncoding: returning encoding (%ld)", gsEncoding );
	
	return gsEncoding;
}

void AddNode( CFStringRef nodeNameRef )
{
	// print name?
}

int main(int argc, char *argv[])
{
	if ( argc == 1 )
	{
		DiscoverLMBs();
	}
	else if ( argc == 3 && strcmp( argv[1], "-service" ) == 0 )
	{
		GetWorkgroupServers( argv[2] );
	}
	else if ( argc == 3 && strcmp( argv[1], "-lmbquery" ) == 0 )
	{
		LMBDiscoverer*	ourLMBDiscoverer = new LMBDiscoverer();
		
		ourLMBDiscoverer->Initialize();
		CFStringRef	lmbRef = CFStringCreateWithCString( NULL, argv[2], kCFStringEncodingUTF8 );
		CFStringRef	workgroupNameRef = ourLMBDiscoverer->DoNMBLookupOnLMB( lmbRef );
		
		CFShow( workgroupNameRef );
	}
	
	return 0; 
}

void DiscoverLMBs( LMBDiscoverer* discoverer )
{
	printf( "Starting search for LMBs...\n" );

	UInt32	startTime = GetCurrentTime();
	LMBDiscoverer*	ourLMBDiscoverer = discoverer;
	
	if ( !discoverer )
		ourLMBDiscoverer = new LMBDiscoverer();
	
	ourLMBDiscoverer->Initialize();
	
	ourLMBDiscoverer->DiscoverCurrentWorkgroups();
	
	printf( "Time to discover: %d seconds\n", GetCurrentTime()-startTime );
	printf( "\nResults:\n" );
	CFShow( ourLMBDiscoverer->GetAllKnownLMBs() );
	CFShow( ourLMBDiscoverer->GetOurLMBs() );
	
	if (!discoverer )
		delete( ourLMBDiscoverer );
}

void GetWorkgroupServers( char* workgroup )
{
    char*		resultPtr = NULL;
	const char*	argv[7] = {0};
	char		lmbName[256] = {0,};
	CFStringRef	lmbNameRef = NULL;
	CFStringRef	workgroupRef = CFStringCreateWithCString( NULL, workgroup, NSLGetSystemEncoding() );
    Boolean		canceled = false;
	CFArrayRef	listOfLMBs = NULL;
	
	LMBDiscoverer*	ourLMBDiscoverer = new LMBDiscoverer();
	
	ourLMBDiscoverer->Initialize();
	
	listOfLMBs = ourLMBDiscoverer->CreateCopyOfCachedLMBs( workgroupRef );
	
	if (!listOfLMBs)
	{
		DiscoverLMBs(ourLMBDiscoverer);

		listOfLMBs = ourLMBDiscoverer->CreateCopyOfCachedLMBs( workgroupRef );
	}
	
	if ( listOfLMBs )
	{
		DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers doing lookup on %ld LMBs responsible for %s\n", CFArrayGetCount(listOfLMBs), workgroup );
		for ( CFIndex i=CFArrayGetCount(listOfLMBs)-1; i>=0; i-- )
		{
			lmbNameRef = (CFStringRef)CFArrayGetValueAtIndex( listOfLMBs, i );
			
			if ( !lmbNameRef )
			{
				DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers, couldn't get lmb for workgroup: %s\n", workgroup );
			}
			else
			{
				CFStringGetCString( lmbNameRef, lmbName, sizeof(lmbName), NSLGetSystemEncoding() );
					
				argv[0] = "/usr/bin/smbclient";
				argv[1] = "-W";
				argv[2] = workgroup;
				argv[3] = "-NL";
				argv[4] = lmbName;
				argv[5] = "-U%";
			
				DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers calling smbclient -W %s -NL %s -U\%\n", workgroup, lmbName );
				if ( myexecutecommandas( NULL, "/usr/bin/smbclient", argv, false, kTimeOutVal, &resultPtr, &canceled, getuid(), getgid() ) < 0 )
				{
					DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers smbclient -W %s -NL %s -U\%\n", workgroup, lmbName );
				}
				else if ( resultPtr )
				{
					DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers, resultPtr = 0x%lx, length = %ld\n", (UInt32)resultPtr, strlen(resultPtr) );
					DBGLOG( "%s\n", resultPtr );
					
					if ( ExceptionInResult(resultPtr) )
					{
						DBGLOG( "CSMBServiceLookupThread::GetWorkgroupServers, got an error, clearing cached lmb results\n" );
						ourLMBDiscoverer->ClearLMBForWorkgroup(workgroupRef, lmbNameRef);
					}
					else
					{        
						CFArrayRef results = ParseOutStringsFromSMBClientResult( resultPtr, "Server", "Comment" );
		
						if ( results )
						{
							for ( CFIndex i=CFArrayGetCount(results)-2; i>=0; i-=2 )
							{
								AddServiceResult( workgroupRef, (CFStringRef)CFArrayGetValueAtIndex(results, i), (CFStringRef)CFArrayGetValueAtIndex(results, i+1) );	// adding workgroup as key, lmb as value
							}
							
							CFRelease( results );
						}
					}
					
					free( resultPtr );
					resultPtr = NULL;
				}
			}
		}
		
		CFRelease( listOfLMBs );
	}
	
	if ( workgroupRef )
		CFRelease( workgroupRef );
	workgroupRef = NULL;
}

void AddServiceResult( CFStringRef workgroupRef, CFStringRef netBIOSRef, CFStringRef commentRef )
{
	if ( netBIOSRef && commentRef )
	{
		char	name[32];
		
		CFStringGetCString( netBIOSRef, name, sizeof(name), kCFStringEncodingUTF8 );
		
		printf( "%s\n", name );
	}
}

