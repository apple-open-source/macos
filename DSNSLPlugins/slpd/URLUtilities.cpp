/*
	File:		URLUtilities.cp

	Contains:	some simple URL utilities

	Written by:	Dave Fisher and Kevin Arnold

	Copyright:	© 1997 - 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):



	To Do:
*/
#include <ctype.h>
#include <stdio.h>
#include <string.h>
//#include "NSLCore.h"
#include <Carbon/Carbon.h>

#include "URLUtilities.h"

// carbon d6-d9 workarounds
#if TARGET_CARBON
	#define charisalpha(c)		( ((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') )
	#define charisdigit(c)		( (c) >= '0' && (c) <= '9' )
	#define charisprint(c)		( (c) >= ' ' )
	#define charisxdigit(c)		( charisdigit((c)) || ( (c) >= 'a' && (c) <= 'f' ) || ( (c) >= 'A' && (c) <= 'F' ) )
#else
	#define charisalpha			isalpha
	#define charisdigit			isdigit
	#define charisprint			isprint
	#define charisxdigit		isxdigit
#endif

/*********
 * IsURL *
 *********
 
 Determines if given string is a URL, and if so, returns the offset to "://<address>", which gives
 us the length of the service type string as well as a way to find the address string.
 
 - Update, changed this to look for ":/" instead since the url "afp:/at/... is valid"
*/

Boolean	IsURL( const char* theString, UInt32 theURLLength, char** svcTypeOffset )
{

	Boolean		foundURL=false;
	
	if ( AllLegalURLChars(theString, theURLLength) )
	{
		*svcTypeOffset = strstr(theString, ":/");			// look for the interesting tag chars
		if ( *svcTypeOffset != NULL)
			foundURL = true;
	}
	
	return foundURL;
	
}


/********************
 * AllLegalURLChars *
 ********************
 
 Returns whether all chars in given RR are legal for use in a URL.  Legal chars are the printables, except
 for space, ", < and >.  
*/

Boolean	AllLegalURLChars( const char* theString, UInt32 theURLLength )
{

	Boolean	isLegal = true;
	
	if ( theString )				// make sure we have a string to examine
	{		
		for (SInt32 i = theURLLength - 1; i>=0 && isLegal; i--)
			isLegal = IsLegalURLChar( theString[i] );
	}
	else
		isLegal = false;
	
	return isLegal;
	
}


/********************
 * IsLegalURLChar *
 ********************
 Returns whether a char is legal for use in a URL.  Legal chars are the printables, except
 for space, ", < and >.  
*/

Boolean IsLegalURLChar( const char theChar )
{
	return ( charisprint( theChar ) && ( theChar != ' ' && theChar != '\"' && theChar != '<' && theChar != '>' ) );
}


/*************************
 * GetServiceTypeFromURL *
 *************************
 
 Return the service type from the URL.  Format of the URL is:
 <service type>://<address>, so just grab the first part.
*/

void GetServiceTypeFromURL(	const char* readPtr,
							UInt32 theURLLength,
							char*	URLType )
{

	char*	curOffset;
	UInt16	typeLen;
	
	if ( IsURL( readPtr, theURLLength, &curOffset))
	{
		typeLen = curOffset - readPtr;
		::memcpy(URLType, (unsigned char*)readPtr, typeLen);
		URLType[typeLen] = '\0';
	}
	else
	{
		URLType[0] = '\0';				// nothing here to find
	}

}


Boolean IsCharURLReservedOrIllegal( const char c )
{
	Boolean 	isIllegal = false;
	
	if ( c <= 0x1F || c == 0x7F || (unsigned char)c >= 0x80 )
		isIllegal = true;
	else
	{
		switch (c)
		{
			case '<':
			case '>':
			case 0x22:	// double quote
			case 0x5c:	// back slash
			case '#':
			case '{':
			case '}':
			case '|':
			case '^':
			case '~':
			case '[':
			case ']':
			case '`':
			case ';':
			case '/':
			case '?':
			case ':':
			case '@':
			case '=':
			case '&':
			case ' ':
			case ',':
			case '%':
				isIllegal = true;
			break;
		}
	}
	
	return isIllegal;
}

void EncodeCharToHex( const char c, char* newHexChar )
{
	// Convert ascii to %xx equivalent
	div_t			result;
	short			hexValue = c;
	char 			c1, c2;
	
	if ( hexValue < 0 )
		hexValue -= 0xFF00;	// clear out the high byte
	
	result = div( hexValue, 16 );
	
	if ( result.quot < 0xA )
		c1 = (char)result.quot + '0';
	else
		c1 = (char)result.quot + 'a' - 10;
	
	if ( result.rem < 0xA )
		c2 = (char)result.rem + '0';
	else
		c2 = (char)result.rem + 'a' - 10;
	
	newHexChar[0] = '%';
	newHexChar[1] = c1;
	newHexChar[2] = c2;
}

char DecodeHexToChar( const char* oldHexTriplet, Boolean* wasHexTriplet  )
{
	char c, c1, c2;
	
	*wasHexTriplet = false;
	
	c = *oldHexTriplet;

	if ( c == '%' )
	{
		// Convert %xx to ascii equivalent
		c1 = tolower(oldHexTriplet[1]);
		c2 = tolower(oldHexTriplet[2]);
		if (charisxdigit(c1) && charisxdigit(c2)) 
		{
			c1 = charisdigit(c1) ? c1 - '0' : c1 - 'a' + 10;
			c2 = charisdigit(c2) ? c2 - '0' : c2 - 'a' + 10;
			c = (c1 << 4) | c2;
			
			*wasHexTriplet = true;
		}
	}
	
	return c;
}

Boolean EncodeHTTPString( string& text )
{
	// Decode characters in theString.
	// Along the way, change:
	//   ascii to '%xx' equivalent
	string		temp, theString = text.c_str();
	
	short 			i=0;
	short 			len = theString.size();
	char 			c, c1, c2;
	Boolean			encodedTheString = false;

	if ( *(text.c_str()) == '.' && text.size() == 1 )
	{
		// this means the item name is just a period which can cause all sorts of wierdnesses if it is
		// a folder name.  Encode this special case	// KA - 1/7/97
		text = "%2E";
		encodedTheString = true;
	}
	else
	{
		temp.reserve(len);	// so we don't keep bumping up the size
		
		while (i < len)
		{
			// Convert space to +
			c = theString[i];
			
			if ( c == '>' )
			{
				temp += "%3E";
				encodedTheString = true;
			}
			else if ( c == '<' )
			{
				temp += "%3C";
				encodedTheString = true;
			}
			else if ( c == '?' )
			{
				temp += "%3F";
				encodedTheString = true;
			}
			else if ( c == '/' )
			{
				temp += "%2F";	// KA - 7/24/96
				encodedTheString = true;
			}
			else if ((c < 0x2C) || (c > 'z'))	// not between - and z, convert
			{
				// Convert ascii to %xx equivalent
				div_t	result;
				short	hexValue = c;
				
				if ( hexValue < 0 )
					hexValue -= 0xFF00;	// clear out the high byte
				
				result = div( hexValue, 16 );
				
				if ( result.quot < 0xA )
					c1 = (char)result.quot + '0';
				else
					c1 = (char)result.quot + 'a' - 10;
				
				if ( result.rem < 0xA )
					c2 = (char)result.rem + '0';
				else
					c2 = (char)result.rem + 'a' - 10;
				
				temp += '%';
				temp += c1;
				temp += c2;

				encodedTheString = true;
			}
			else
			{
			// Copy regular characters
				temp += c;
			}

			i = i + 1;
		}
		
		text = temp.c_str();
	}
	
	return encodedTheString;
}

void DecodeHTTPString( string& theString )
{
	// Decode characters in theString.
	// changing '%xx' to the ascii equivalent
	
	short i=0;
	short len = theString.size();
	char c, c1, c2;
	
	string temp;
	temp.reserve(len);	// so we don't keep bumping up the size
	
	while (i < len)
	{
		c = theString[i];

		if ((c == '%') & (i+2 < len))
		{
			// Convert %xx to ascii equivalent
			c1 = tolower(theString[i+1]);
			c2 = tolower(theString[i+2]);
			if (charisxdigit(c1) && charisxdigit(c2)) 
			{
				c1 = charisdigit(c1) ? c1 - '0' : c1 - 'a' + 10;
				c2 = charisdigit(c2) ? c2 - '0' : c2 - 'a' + 10;
				c = (c1 << 4) | c2;
				
				temp += c;
			}
			i = i + 3;
		}

		// Copy regular characters
		else/* if (isgraph(c) || c == '\r')*/
		{
			temp += c;
			i = i + 1;
		}
	}
	
	theString = temp;
}



















