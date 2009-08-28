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


#ifndef __CDDA_TRACK_NAME_H__
#define __CDDA_TRACK_NAME_H__


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include "TBundle.h"


//-----------------------------------------------------------------------------
//	Class Declaration
//
//	CDDATrackName is the base class for all databases used. It provides
//	localized variants of the artist, title, and track names, as well as a
//	possible separator string used for diskarbitrationd
//-----------------------------------------------------------------------------

class CDDATrackName
{
	
	private:
		
		// Disable copy constructors
		CDDATrackName ( CDDATrackName &src );
		void operator = ( CDDATrackName &src );
		
		TBundle *		fBundle;
		
		CFStringRef		fTrackNameStringRef;
		CFStringRef		fAlbumStringRef;
		CFStringRef		fArtistStringRef;
		CFStringRef		fSeparatorStringRef;
		
	public:
		
		// Constructor
		CDDATrackName ( void );
		
		// Destructor
		virtual ~CDDATrackName ( void );		
		
		virtual SInt32			Init ( const char * bsdDevNode, const void * TOCData );
		
		virtual CFStringRef 	GetArtistName ( void );
		virtual CFStringRef 	GetAlbumName ( void );
		virtual CFStringRef 	GetSeparatorString ( void );
		virtual CFStringRef 	GetTrackName ( UInt8 trackNumber );
		
};


#endif	/* __CDDA_TRACK_NAME_H__ */
