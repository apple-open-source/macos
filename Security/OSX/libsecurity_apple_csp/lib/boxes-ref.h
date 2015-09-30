/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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


#ifndef	_AES_BOXES_H_
#define _AES_BOXES_H_

#include "rijndael-alg-ref.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define AES_MUL_BY_LOOKUP	1

#if			AES_MUL_BY_LOOKUP
extern const word8 mulBy0x02[256];
extern const word8 mulBy0x03[256];
extern const word8 mulBy0x0e[256];
extern const word8 mulBy0x0b[256];
extern const word8 mulBy0x0d[256];
extern const word8 mulBy0x09[256];
#else
extern const unsigned char Logtable[256];
extern const unsigned char Alogtable[256];
#endif	/* AES_MUL_BY_LOOKUP */

extern const unsigned char S[256];
extern const unsigned char Si[256];
extern const unsigned char iG[4][4];
extern const unsigned long rcon[30];

#ifdef	__cplusplus
}
#endif

#endif	/* _AES_BOXES_H_ */
