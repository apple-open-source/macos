/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		appleGlue.h

	Contains:	Glue layer between Apple SecureTransport and 
				original SSLRef code. 

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_SSL_H_
#include "ssl.h"
#endif

#ifndef	_APPLE_GLUE_H_
#define _APPLE_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Functions to allow old code to use SSLBuffer-based I/O calls.
 * We redirect the calls here to an SSLIOFunc.
 * This is of course way inefficient due to an extra copy for
 * each I/O, but let's do it this way until the port settles down.
 */
 
SSLErr sslIoRead(
 	SSLBuffer 		buf, 
 	UInt32 			*actualLength, 
 	SSLContext 		*ctx);
 
SSLErr sslIoWrite(
 	SSLBuffer 		buf, 
 	UInt32 			*actualLength, 
 	SSLContext 		*ctx);
 
 /*
  * Convert between SSLErr and OSStatus.
  */
extern SSLErr sslErrFromOsStatus(OSStatus o);
extern OSStatus sslErrToOsStatus(SSLErr s);
 
/*
 * Time functions - replaces SSLRef's SSLTimeFunc, SSLConvertTimeFunc
 */
extern SSLErr sslTime(UInt32 *time);
SSLErr sslConvertTime(UInt32 *time);

#ifdef __cplusplus
}
#endif

 #endif	/* _APPLE_GLUE_H_ */

 