/*
	File:		URLUtilities.h

	Contains:	dec'ls for using URL utilities

	Written by:	Kevin Arnold & Dave Fisher

	Copyright:	© 1997 - 1999 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):
	
	<11>	04/28/99	KA		adding IsCharURLReservedOrIllegal, EncodeCharToHex, DecodeHexToChar
	<10>	04/05/99	sns		carbon tweaks
	<09>	03-30-99	KA		incorporated changes for Steve
	<08>	03/26/99	sns		use std namespace
	<07>	03/24/99	KA		changing NSLAPI.h to NSL.h
	<06>	02/24/99	KA		Changed GetServiceTypeFromURL params
	<05>	11/06/98	KA		added EncodeHTTPString and DecodeHTTPString
	<04>	02/11/98	DMF		Switched error returns to NSLError

	To Do:
*/



#ifndef __URLUTILITIES__
#define __URLUTILITIES__
#include <string>

#if TARGET_CARBON
	#define string	std::string
#endif

//#include "NSLCore.h"
//#include <MacTypes.h>

#define	kFileURLDelimiter	'\r'				// delimits URL's within cache files


#if __cplusplus
//extern "C" {
#endif

Boolean IsCharURLReservedOrIllegal( const char c );
void EncodeCharToHex( const char c, char* newHexChar );
char DecodeHexToChar( const char* oldHexTriplet, Boolean* wasHexTriplet  );


Boolean	IsURL( const char* theString, UInt32 theURLLength, char** svcTypeOffset );
Boolean	AllLegalURLChars( const char* theString, UInt32 theURLLength );
Boolean IsLegalURLChar( char theChar );

void GetServiceTypeFromURL(	const char* readPtr,
							UInt32 theURLLength,
							char*	URLType );		// URLType should be pointing at valid memory

Boolean EncodeHTTPString( string& text );
void DecodeHTTPString( string& theString );

#if __cplusplus
//}
#endif

#endif