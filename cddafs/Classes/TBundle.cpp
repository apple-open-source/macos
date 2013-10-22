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

#include "TBundle.h"
#include "TSystemUtils.h"


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG	0

#ifndef DEBUG_ASSERT_COMPONENT_NAME_STRING
	#define DEBUG_ASSERT_COMPONENT_NAME_STRING "TBundle"
#endif

#include <AssertMacros.h>

#define kLocalizableString		"Localizable"
#define kStringsTypeString		"strings"
#define kEmptyString			""


//-----------------------------------------------------------------------------
//	Constructor														 [PUBLIC]
//-----------------------------------------------------------------------------

TBundle::TBundle ( CFBundleRef bundle ) :
	fLocalizationDictionaryForTable ( NULL )
{
	
	check ( bundle );
	fCFBundleRef = ( CFBundleRef ) ::CFRetain ( bundle );
	
}


//-----------------------------------------------------------------------------
//	Destructor														 [PUBLIC]
//-----------------------------------------------------------------------------

TBundle::~TBundle ( void )
{
	
	check ( fCFBundleRef );
	::CFRelease ( fCFBundleRef );
	fCFBundleRef = NULL;
	
	if ( fLocalizationDictionaryForTable != NULL )
	{
		
		::CFRelease ( fLocalizationDictionaryForTable );
		fLocalizationDictionaryForTable = NULL;
		
	}
	
}


//-----------------------------------------------------------------------------
//	CopyLocalizedStringForKey										 [PUBLIC]
//-----------------------------------------------------------------------------

CFStringRef
TBundle::CopyLocalizedStringForKey ( CFStringRef key,
									 CFStringRef defaultValue,
									 CFStringRef tableName )
{
	
	CFStringRef		result = NULL;

	if ( tableName == NULL )
	{
		tableName = CFSTR ( kLocalizableString );
	}
	
	if ( fLocalizationDictionaryForTable == NULL )
	{
		fLocalizationDictionaryForTable = CopyLocalizationDictionaryForTable ( tableName );
	}
	
	if ( fLocalizationDictionaryForTable != NULL )
	{
		result = ( CFStringRef ) ::CFDictionaryGetValue ( fLocalizationDictionaryForTable, key );
		::CFRetain ( result );
	}
	
	if ( ( result == NULL ) && ( defaultValue != NULL ) )
	{
		result = ( CFStringRef ) ::CFRetain ( defaultValue );
	}
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	CopyLocalizationDictionaryForTable								[PRIVATE]
//-----------------------------------------------------------------------------

CFDictionaryRef
TBundle::CopyLocalizationDictionaryForTable ( CFStringRef table )
{
	
	CFDictionaryRef		stringTable				= NULL;
	CFURLRef			localizedStringsURL		= NULL;
	CFDataRef           tableData				= NULL;
	CFStringRef			errStr					= NULL;
    
	localizedStringsURL = CopyURLForResourceOfTypeInBundle (
				table,
				CFSTR ( kStringsTypeString ),
				fCFBundleRef );

	require ( ( localizedStringsURL != NULL ), ErrorExit );

    tableData = TSystemUtils::ReadDataFromURL ( localizedStringsURL );
    
	require ( ( tableData != NULL ), ReleaseURL );
	
	stringTable = ( CFDictionaryRef ) ::CFPropertyListCreateFromXMLData (
											kCFAllocatorDefault,
											tableData,
											kCFPropertyListImmutable,
											&errStr );

	if ( errStr != NULL )
	{
	
	#if DEBUG
		::CFShow ( errStr );
	#endif
		
		::CFRelease ( errStr );
		errStr = NULL;
		
	}
	
	::CFRelease ( tableData );
	tableData = NULL;
	
	check ( stringTable != NULL );
	
    
ReleaseURL:
	
	
	::CFRelease ( localizedStringsURL );
	localizedStringsURL = NULL;
	
	
ErrorExit:
	
	
    return stringTable;
	
}


//-----------------------------------------------------------------------------
//	CopyLocalizations												[PRIVATE]
//-----------------------------------------------------------------------------

CFArrayRef
TBundle::CopyLocalizations ( void )
{
	return ::CFBundleCopyBundleLocalizations ( fCFBundleRef );
}


//-----------------------------------------------------------------------------
//	CopyLocalizationsForPrefs										[PRIVATE]
//-----------------------------------------------------------------------------

CFArrayRef
TBundle::CopyLocalizationsForPrefs ( CFArrayRef bundleLocalizations,
									 CFArrayRef preferredLanguages )
{
	return ::CFBundleCopyLocalizationsForPreferences ( bundleLocalizations, preferredLanguages );
}


//-----------------------------------------------------------------------------
//	CopyURLForResourceOfTypeInBundle								[PRIVATE]
//-----------------------------------------------------------------------------

CFURLRef
TBundle::CopyURLForResourceOfTypeInBundle ( CFStringRef		resource,
										    CFStringRef		type,
										    CFBundleRef 	bundle )
{
	
	CFURLRef			result					= NULL;
	CFArrayRef			preferredLanguages		= NULL;
	CFArrayRef			bundleLocalizations		= NULL;
	CFArrayRef			preferredLocalizations	= NULL;
	CFIndex				index					= 0;
	CFIndex				count					= 0;

	if ( bundle == NULL )
	{
		bundle = ::CFBundleGetMainBundle ( );
	}
	
	require ( ( bundle != NULL ), ErrorExit );
	
	preferredLanguages = TSystemUtils::GetPreferredLanguages ( );
	require ( ( preferredLanguages != NULL ), ErrorExit );
	
	bundleLocalizations		= CopyLocalizations ( );
	preferredLocalizations	= CopyLocalizationsForPrefs ( bundleLocalizations, preferredLanguages );

	count = ::CFArrayGetCount ( preferredLocalizations );

	for ( index = 0; ( result == NULL ) && ( index < count ); index++)
	{
		
		CFStringRef	item = ( CFStringRef ) ::CFArrayGetValueAtIndex ( preferredLocalizations, index );
		
		result = CopyURLForResource ( resource,
									  type,
									  NULL,
									  item );
		
	}
	
	if ( result == NULL )
	{
		
		CFStringRef	developmentLocalization = ::CFBundleGetDevelopmentRegion ( fCFBundleRef );
		
		if ( developmentLocalization != NULL )
		{
			result = CopyURLForResource ( resource, type, NULL, developmentLocalization );
		}
		
	}

	if ( preferredLocalizations != NULL )
		::CFRelease ( preferredLocalizations );
    
	if ( preferredLanguages != NULL )
		::CFRelease ( preferredLanguages );
	
	if ( bundleLocalizations != NULL )
		::CFRelease ( bundleLocalizations );
	
	
ErrorExit:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	CopyURLForResource												[PRIVATE]
//-----------------------------------------------------------------------------

CFURLRef
TBundle::CopyURLForResource ( CFStringRef resource,
							  CFStringRef type,
							  CFStringRef dir,
							  CFStringRef localization )
{
	
	CFURLRef		resultURL 	= NULL;
	
	resultURL = ::CFBundleCopyResourceURLForLocalization ( fCFBundleRef,
														   resource,
														   type,
														   dir,
														   localization );
	
	return resultURL;
	
}


//-----------------------------------------------------------------------------
//					End				Of			File
//-----------------------------------------------------------------------------
