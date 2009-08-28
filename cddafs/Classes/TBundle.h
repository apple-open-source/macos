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


#ifndef __TBUNDLE_H__
#define __TBUNDLE_H__


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <CoreFoundation/CoreFoundation.h>


//-----------------------------------------------------------------------------
//	Class Declaration
//-----------------------------------------------------------------------------

class TBundle
{
	
private:
	
	// Disable copy constructors
	TBundle ( TBundle &src );
	void operator = ( TBundle &src );
	
	CFBundleRef		fCFBundleRef;
	CFDictionaryRef	fLocalizationDictionaryForTable;
	
	CFArrayRef		CopyLocalizations ( void );
	CFArrayRef		CopyLocalizationsForPrefs ( CFArrayRef bundleLocalizations,
												CFArrayRef preferredLanguages );

	CFURLRef		CopyURLForResourceOfTypeInBundle ( CFStringRef	resource,
													   CFStringRef	type,
													   CFBundleRef	bundle );
	
	CFURLRef		CopyURLForResource ( CFStringRef resource,
										 CFStringRef type,
										 CFStringRef dir,
										 CFStringRef localization );
	
	CFDictionaryRef	CopyLocalizationDictionaryForTable ( CFStringRef table );
	
	
public:
	
	// Constructor
	TBundle ( CFBundleRef bundle );
	
	// Destructor
	virtual ~TBundle ( void );
	
	CFStringRef		CopyLocalizedStringForKey ( CFStringRef key,
												CFStringRef	defaultValue,
												CFStringRef tableName );
	
};


#endif	// __TBUNDLE_H__
