/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ftpmunge.h"


// Type to keep track of the state of the conversion process
typedef enum ConversionState {
	kNormal		= 0x01,		// anything else
	kCaret		= 0x02,		// ^
	kStartZero	= 0x04,		// ^0
	kStartTwo	= 0x08,		// ^2
	kStartFive	= 0x10		// ^5
} ConversionState;


// Compute the length of the unencoding of the encoded string FTP.
size_t ftp2rawstrlen( const char *ftp );

size_t ftp2rawstrlen( const char *ftp )
{
	size_t len = 0;
	ConversionState state = kNormal;
	for( const char *s = ftp; s && *s; s++ ) {
		if( kCaret == state && '0' == *s ) {
			state = kStartZero;
			continue;
		}
		else if( kCaret == state && '2' == *s ) {
			state = kStartTwo;
			continue;
		}
		else if( kCaret == state && '5' == *s ) {
			state = kStartFive;
			continue;
		}
		else if( kCaret == state )	// malformed ^# directive
			len += 2;
		else if( kStartZero == state && '9' == *s )	// \t
			len++;
		else if( kStartZero == state && ('A' == *s || 'a' == *s) )// \n
			len++;
		else if( kStartZero == state )	// malformed ^0* directive
			len += 3;
		else if( kStartTwo == state && '0' == *s )	// ' '
			len++;
		else if( kStartTwo == state )	// malformed ^2* directive
			len += 3;
		else if( kStartFive == state && ('E' == *s || 'e' == *s) )// ^
			len++;
		else if( kStartFive == state )	// malformed ^5* directive
			len += 3;
		else if( '^' == *s ) {
			state = kCaret;
			continue;
		}
		else	// everything else
			len++;
		
		state = kNormal;
	}

	if( kCaret == state )		// mailformed - trailing ^
		len += 1;
	else if( kNormal != state )	// malformed - trailing ^[0,2,5]
		len += 2;

	return len;
}

char *ftp2rawstr( const char *ftp ) throw( std::bad_alloc )
{
	if( !ftp )
		return NULL;

	char *raw = new char[ ftp2rawstrlen( ftp ) + 1 ];
	char *raw_start = raw;
	ConversionState state = kNormal;

	for( ; ftp && *ftp; ftp++ ) {
	retry:
		if( kCaret == state && '0' == *ftp ) {
			state = kStartZero;
			continue;
		}
		else if( kCaret == state && '2' == *ftp ) {
			state = kStartTwo;
			continue;
		}
		else if( kCaret == state && '5' == *ftp ) {
			state = kStartFive;
			continue;
		}
		else if( kCaret == state ) {	// malformed ^* directive
			*raw = '^'; raw++;
			state = kNormal;
			goto retry;
		}
		else if( kStartZero == state && '9' == *ftp )	// \t
			*raw = '\t';
		else if( kStartZero == state && ('A' == *ftp || 'a' == *ftp))
			*raw = '\n';
		else if( kStartZero == state ) {// malformed ^0* directive
			*raw = '^'; raw++;
			*raw = '0'; raw++;
			state = kNormal;
			goto retry;
		}
		else if( kStartTwo == state && '0' == *ftp )	// ' '
			*raw = ' ';
		else if( kStartTwo == state ) {	// malformed ^2* directive
			*raw = '^'; raw++;
			*raw = '2'; raw++;
			state = kNormal;
			goto retry;
		}
		else if( kStartFive == state && ('E' == *ftp || 'e' == *ftp) )
			*raw = '^';
		else if( kStartFive == state ) {// malformed ^5* directive
			*raw = '^'; raw++;
			*raw = '5'; raw++;
			state = kNormal;
			goto retry;
		}
		else if( '^' == *ftp ) {
			state = kCaret;
			continue;
		}
		else	// everything else
			*raw = *ftp;

		state = kNormal;
		raw++;
	}

	if( kCaret == state ) {	// malformed - trailing ^
		*raw = '^'; raw++;
	}
	else if( kStartZero == state ) {// malformed - trailing ^0
		*raw = '^'; raw++;
		*raw = '0'; raw++;
	}
	else if( kStartTwo == state ) {	// malformed - trailing ^2
		*raw = '^'; raw++;
		*raw = '2'; raw++;
	}
	else if( kStartFive == state ) {// malformed - trailing ^5
		*raw = '^'; raw++;
		*raw = '5'; raw++;
	}

	*raw = '\0';	// NULL terminate the string;

	return raw_start;
}

char *raw2ftpstr( const char *raw ) throw( std::bad_alloc )
{
	if( !raw )
		return NULL;

	// discover the space neeed for the ftp string
	int len = 0;
	for( const char *s = raw; s && *s; s++ ) {
		switch( *s ) {
		case ' ':
		case '\t':
		case '^':
			len += 3;
			break;
		default:
			len++;
			break;
		}
	}

	len++;	// terminating '\0'
	char *ftp = new char[ len ];
	char *ftp_start = ftp;
	if( !ftp )
		return NULL;

	// convert the characters
	for( ; raw && *raw; raw++, ftp++ ) {
		switch( *raw ) {
		case ' ':
			*ftp = '^'; ftp++;
			*ftp = '2'; ftp++;
			*ftp = '0';
			break;
		case '\t':
			*ftp = '^'; ftp++;
			*ftp = '0'; ftp++;
			*ftp = '9';
			break;
		case '^':
			*ftp = '^'; ftp++;
			*ftp = '5'; ftp++;
			*ftp = 'E';
			break;
		case '\n':
			*ftp = '^'; ftp++;
			*ftp = '0'; ftp++;
			*ftp = 'A'; ftp++;
			break;
		default:
			*ftp = *raw;
			break;
		}
	}

	// NULL terminate the string
	*ftp = '\0';

	return ftp_start;
}
