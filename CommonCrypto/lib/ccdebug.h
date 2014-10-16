/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

/*
 *  ccdebug.h - CommonCrypto debug macros
 *
 */

#if defined(COMMON_DIGEST_FUNCTIONS) || defined(COMMON_CMAC_FUNCTIONS) || defined(COMMON_GCM_FUNCTIONS) \
    || defined (COMMON_AESSHOEFLY_FUNCTIONS) || defined(COMMON_CASTSHOEFLY_FUNCTIONS) \
    || defined (COMMON_CRYPTOR_FUNCTIONS) || defined(COMMON_HMAC_FUNCTIONS) \
    || defined(COMMON_KEYDERIVATION_FUNCTIONS) || defined(COMMON_SYMMETRIC_KEYWRAP_FUNCTIONS) \
    || defined(COMMON_RSA_FUNCTIONS) || defined(COMMON_EC_FUNCTIONS) || defined(COMMON_DH_FUNCTIONS) \
    || defined(COMMON_BIGNUM_FUNCTIONS) || defined(COMMON_RANDOM_FUNCTIONS)

#define DIAGNOSTIC
#endif

#ifdef KERNEL
#include <stdarg.h>

#define	CC_DEBUG		 1
#define	CC_DEBUG_BUG		2
#define	CC_DEBUG_FAILURE	3

#if DIAGNOSTIC
#define CC_DEBUG_LOG(lvl, fmt, ...) do {				      \
	const char *lvl_type[] = { "INVALID", "DEBUG", "ERROR", "FAILURE" };  \
	char fmtbuffer[256]; 						      \
	int l = lvl;							      \
									      \
	if (l < 0 || l > 3) l = 0;					      \
	snprintf(fmtbuffer, sizeof(fmtbuffer),				      \
	    "CommonCrypto Function: %s:%d (%s) - %s", __FILE__, __LINE__,     \
	    lvl_type[l], fmt);					              \
	printf(fmtbuffer, __VA_ARGS__);		      			      \
} while (0)

#else

#define CC_DEBUG_LOG(lvl,fmt,...) {}

#endif /* DIAGNOSTIC */

#else

#include <asl.h>
#include <stdarg.h>

#define CC_DEBUG_FAILURE		ASL_LEVEL_EMERG
#define CC_DEBUG_BUG			ASL_LEVEL_ERR
#define CC_DEBUG			ASL_LEVEL_ERR

void ccdebug_imp(int level, const char *funcname, const char *format, ...);

#ifdef DIAGNOSTIC

#define CC_DEBUG_LOG(lvl,...) ccdebug_imp(lvl, __FUNCTION__, __VA_ARGS__)

#else

#define CC_DEBUG_LOG(lvl,...) {}
#endif /* DEBUG */

#endif /* KERNEL */

