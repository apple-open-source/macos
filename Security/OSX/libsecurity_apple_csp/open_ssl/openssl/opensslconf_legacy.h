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


/*
 * Copyright (c) 2000-2002,2011,2014 Apple Inc. All Rights Reserved.
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
 * opensslconf_legacy.h - hand-rolled config #defines for openssl code used in AppleCSP
 * Written by Doug Mitchell 4/3/2001
 */
#ifndef _OPENSSL_CONF_H_
#define _OPENSSL_CONF_H_

#include <Security/cssmtype.h>		/* for uint32, etc. */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Flags to disable a lot of code we don't need.
 */
#define _OPENSSL_APPLE_CDSA_	1

#define NO_MD5 		1
#define NO_RIPEMD 	1
#define NO_DES 		1
#define NO_IDEA 	1
#define NO_MDC2		1
#define NO_MD2      1

#ifdef	NDEBUG
#define NO_ERR		1
#endif

/* disable the static callback ptrs in cryptlib.c */
#define CRYPTO_CALLBACK_ENABLE		0

/* disable the BN_{set,get}_params mechanism, unused */
#define BN_PARAMS_ENABLE			0

typedef uint32 RC2_INT;
typedef uint32 RC4_INT;

/* the following two need calibration and lots of testing; see rc4_enc.c... */
#undef RC4_CHUNK
#undef RC4_INDEX

typedef uint32 RC5_32_INT;
typedef uint32 MD2_INT;

#if defined(HEADER_BF_LOCL_H) && !defined(CONFIG_HEADER_BF_LOCL_H)
#define CONFIG_HEADER_BF_LOCL_H
#define BF_PTR
#endif /* HEADER_BF_LOCL_H */

/*
 * FIXME - this could certainly use some tweaking
 */
/* Should we define BN_DIV2W here? */

/* Only one for the following should be defined */
/* The prime number generation stuff may not work when
 * EIGHT_BIT but I don't care since I've only used this mode
 * for debuging the bignum libraries */

/*
 * Using 64 bit results in an 8% speedup for RSA sign, but a 3%
 * slowdown for RSA verify on a G4 cube compared to 32 bit. 
 *    --dpm, 5/10/01
 */
#undef SIXTY_FOUR_BIT_LONG
#undef SIXTY_FOUR_BIT      
#define THIRTY_TWO_BIT
#undef SIXTEEN_BIT
#undef EIGHT_BIT

#ifdef	__cplusplus
}
#endif


#endif	/* _OPENSSL_CONF_H_ */
