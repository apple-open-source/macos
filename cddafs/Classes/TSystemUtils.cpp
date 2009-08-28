/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
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


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#define DEBUG_ASSERT_COMPONENT_NAME_STRING 					"TSystemUtils"
#include <AssertMacros.h>

#include "TSystemUtils.h"

#include <unistd.h>
#include <sys/stat.h>
#include <SystemConfiguration/SystemConfiguration.h>


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG	0

#define kAppleLanguagesString	"AppleLanguages"
#define kEmptyString			""


//-----------------------------------------------------------------------------
// GetPreferredLanguages - Gets preferred languages of console user.
//															 [STATIC][PUBLIC]
//-----------------------------------------------------------------------------

CFArrayRef
TSystemUtils::GetPreferredLanguages ( void )
{
	
	CFArrayRef			preferredLanguages	= NULL;
	CFPropertyListRef	languages			= NULL;
	CFStringRef			userName			= NULL;
	CFComparisonResult	equal				= kCFCompareEqualTo;
	uid_t				uid					= 0;

	uid = FindUIDToUse ( );
	seteuid ( uid );
	
	userName = ::CFGetUserName ( );
	require ( ( userName != NULL ), ErrorExit );
	
	equal = ::CFStringCompare ( userName, CFSTR ( kEmptyString ), 0 );
	require ( ( equal != kCFCompareEqualTo ), ErrorExit );
	
	languages = ::CFPreferencesCopyValue ( CFSTR ( kAppleLanguagesString ),
										   kCFPreferencesAnyApplication,
										   userName,
										   kCFPreferencesAnyHost );
	
	require ( ( languages != NULL ), ErrorExit );
	require_action ( ( ::CFGetTypeID ( languages ) == ::CFArrayGetTypeID ( ) ),
					 ErrorExit,
					 ::CFRelease ( languages ) );
	
	preferredLanguages = ( CFArrayRef ) languages;
	
	
ErrorExit:
	
	
	seteuid ( 0 );
	
	return preferredLanguages;
	
}


//-----------------------------------------------------------------------------
// FindUIDToUse - Finds the UID of the console user.		 [STATIC][PUBLIC]
//-----------------------------------------------------------------------------

uid_t
TSystemUtils::FindUIDToUse ( void )
{
	
	uid_t 				uid			= 0;
	gid_t				gid			= 0;
	CFStringRef			userName	= NULL;
	SCDynamicStoreRef	storeRef	= NULL;

	storeRef = ::SCDynamicStoreCreate ( kCFAllocatorDefault,
										CFSTR ( "cddafs.util" ),
										NULL,
										NULL );
	require ( ( storeRef != NULL ), ErrorExit );

	userName = ::SCDynamicStoreCopyConsoleUser ( storeRef,
												 &uid,
												 &gid );
	require ( ( userName != NULL ), ReleaseDynamicStore );
	::CFRelease ( userName );
	
	
ReleaseDynamicStore:
	
	
	require_quiet ( ( storeRef != NULL ), ErrorExit );
	::CFRelease ( storeRef );
	storeRef = NULL;
	
	
ErrorExit:
	
	
	return uid;
	
}


//-----------------------------------------------------------------------------
//					End				Of			File
//-----------------------------------------------------------------------------
