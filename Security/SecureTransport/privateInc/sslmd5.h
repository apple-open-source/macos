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
	File:		sslmd5.h

	Contains:	public API to low-level MD5 module

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0, based on RSA code

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/* MD5.H - header file for MD5C.C
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

#ifndef	_SSL_MD5_H_
#define _SSL_MD5_H

#ifdef __cplusplus
extern "C" {
#endif

/* these are from aglobal.h, which we really don't want to compile against */
typedef unsigned long int UINT4;
#define PROTO_LIST(x) x
typedef unsigned char *POINTER;

/* MD5 context. */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

void SSLMD5Init PROTO_LIST ((MD5_CTX *));
void SSLMD5Update PROTO_LIST
  ((MD5_CTX *, const unsigned char *, unsigned int));
void SSLMD5Final PROTO_LIST ((unsigned char [16], MD5_CTX *));

#ifdef __cplusplus
}
#endif

#endif /* _SSL_MD5_H_ */
