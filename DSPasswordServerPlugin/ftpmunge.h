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

#ifndef	_FTPMUNGE_H_
#define	_FTPMUNGE_H_

#include <new>

/*	The ftpaccess file used by xftpd does not support spaces and tabs as
	in filenames because it uses them as delimitors. To get around this
	problem xftpd allows users to convert string with spaces and tabs into
	ftp strings.

	The conversions are done the same way URL encoding is done, except that
	the caret chararcter (^) is used instead of the % character, but only
	for spaces, tabs, and carets

	raw string				ftp string
	----------				----------
	'\t'					^09
	'\n'					^0A
	' '					^20
	^					^5E
*/

/* Return a new new'd string that is the encoding of RAW. Returns NULL
   on failure. The caller is responsible for deallocating the string. */
char *raw2ftpstr( const char *raw ) throw( std::bad_alloc );

/* Return a new new'd string that is the unencoding of FTP. Returns
   NULL on failure. The caller is responsible for deallocating the string. */
char *ftp2rawstr( const char *ftp ) throw( std::bad_alloc );

#endif	// _FTPMUNGE_H_
