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


//
// aesCommon.h - common AES/Rijndael constants
//
#ifndef _H_AES_COMMON_
#define _H_AES_COMMON_

#define MIN_AES_KEY_BITS		128
#define MID_AES_KEY_BITS		192
#define MAX_AES_KEY_BITS		256

#define MIN_AES_BLOCK_BITS		128
#define MID_AES_BLOCK_BITS		192
#define MAX_AES_BLOCK_BITS		256

#define MIN_AES_BLOCK_BYTES		(MIN_AES_BLOCK_BITS / 8)
#define DEFAULT_AES_BLOCK_BYTES	MIN_AES_BLOCK_BYTES

/*
 * When true, the Gladman AES implementation is present and is used
 * for all 128-bit block configurations.
 */
#define GLADMAN_AES_128_ENABLE	1

#endif	/* _H_AES_COMMON_ */
