/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/param.h>
#include <sys/types.h>

// private includes
#include "CDDATrackName.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 0

#if DEBUG
#define PRINT(x)	printf x
#else
#define PRINT(x)
#endif

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "CDDATrackName"
#include <AssertMacros.h>


#define kCDDAFSUtilBundlePath	"/System/Library/Filesystems/cddafs.fs"
#define kArtistString			"Artist"
#define kTitleString			"Title"
#define kTrackNameString		"Audio Track"
#define kSeparatorString		"Separator"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Constructor													[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDDATrackName::CDDATrackName ( void ) :
	fBundle ( NULL ),
	fTrackNameStringRef ( NULL ),
	fAlbumStringRef ( NULL ),
	fArtistStringRef ( NULL ),
	fSeparatorStringRef ( NULL )
{
	
	CFURLRef	urlRef		= NULL;
	CFBundleRef	bundleRef	= NULL;
	
	PRINT ( ( "CDDATrackName constructor called\n" ) );
	
	urlRef = CFURLCreateWithFileSystemPath ( kCFAllocatorDefault,
											 CFSTR ( kCDDAFSUtilBundlePath ),
											 kCFURLPOSIXPathStyle,
											 true );
	
	if ( urlRef != NULL )
	{
		
		#if DEBUG
		CFShow ( urlRef );
		#endif
		
		bundleRef = CFBundleCreate ( kCFAllocatorDefault, urlRef );
		CFRelease ( urlRef );
		urlRef = 0;
		
	}
	
	if ( bundleRef != NULL )
	{
		
		#if DEBUG
		CFShow ( bundleRef );
		#endif
		
		fBundle = new TBundle ( bundleRef );
				
		fArtistStringRef = fBundle->CopyLocalizedStringForKey (
									CFSTR ( kArtistString ),
									CFSTR ( kArtistString ),
									NULL ); // defaults to Localizable.strings
	
		fAlbumStringRef = fBundle->CopyLocalizedStringForKey (
									CFSTR ( kTitleString ),
									CFSTR ( kTitleString ),
									NULL ); // defaults to Localizable.strings
		
		fTrackNameStringRef = fBundle->CopyLocalizedStringForKey (
									CFSTR ( kTrackNameString ),
									CFSTR ( kTrackNameString ),
									NULL ); // defaults to Localizable.strings
	
		fSeparatorStringRef = fBundle->CopyLocalizedStringForKey (
									CFSTR ( kSeparatorString ),
									CFSTR ( kSeparatorString ),
									NULL ); // defaults to Localizable.strings
		
		CFRelease ( bundleRef );
		bundleRef = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Destructor												[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDDATrackName::~CDDATrackName ( void )
{
	
	PRINT ( ( "CDDATrackName destructor called\n" ) );	
	
	#if DEBUG

	CFShow ( fArtistStringRef );
	CFShow ( fAlbumStringRef );
	CFShow ( fTrackNameStringRef );
	CFShow ( fSeparatorStringRef );

	#endif
	
	CFRelease ( fArtistStringRef );
	CFRelease ( fAlbumStringRef );
	CFRelease ( fTrackNameStringRef );
	CFRelease ( fSeparatorStringRef );
	
	fArtistStringRef 	= 0;
	fAlbumStringRef		= 0;
	fTrackNameStringRef	= 0;
	fSeparatorStringRef	= 0;
	
	if ( fBundle != NULL )
	{
		
		delete fBundle;
		fBundle = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Init															[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SInt32
CDDATrackName::Init ( const char * bsdDevNode, const void * TOCData )
{
	return 0;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetArtistName													[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CFStringRef
CDDATrackName::GetArtistName ( void )
{
	CFRetain ( fArtistStringRef );
	return fArtistStringRef;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetAlbumName													[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CFStringRef
CDDATrackName::GetAlbumName ( void )
{
	CFRetain ( fAlbumStringRef );
	return fAlbumStringRef;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTrackName													[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CFStringRef
CDDATrackName::GetTrackName ( UInt8 trackNumber )
{
	CFRetain ( fTrackNameStringRef );
	return fTrackNameStringRef;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSeparatorString											[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CFStringRef
CDDATrackName::GetSeparatorString ( void )
{
	CFRetain ( fSeparatorStringRef );
	return fSeparatorStringRef;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//					End				Of			File
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ