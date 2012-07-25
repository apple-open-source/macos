/*
	File:		ckconfig.h

	Contains:	Common config info.

	Written by:	Doug Mitchell

	Copyright:	Copyright 1998 by Apple Computer, Inc.
                All rights reserved.

	Change History (most recent first):

	<7>	10/06/98	ap		Changed to compile with C++.

	To Do:
*/

/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 */

#ifndef	_CK_CONFIG_H_
#define _CK_CONFIG_H_

/*
 * Common build flags.
 */
#define DEBUG_ENGINE	0

#define ENGINE_127_BITS	0	/* hard-coded 127 elliptic() */

/*
 * These flags are set en masse, one set per target in the XCode project file or 
 * Makefile. They determine what gets compiled into the library. Every flag
 * has to be defined for every configureation - preprocessors directives use
 * #if, not #ifdef. 
 */
 
#ifdef	CK_SECURITY_BUILD
/* 
 * Standard Security.framework build
 */
#define	CRYPTKIT_DER_ENABLE	    1	    /* DER encoding support */
#define	CRYPTKIT_LIBMD_DIGEST	    1	    /* use CommonCrypto digests */
#define CRYPTKIT_ELL_PROJ_ENABLE    1	    /* elliptic projection */
#define CRYPTKIT_ECDSA_ENABLE	    1	    /* ECDSA (requires ELL_PROJ_ENABLE) */
#define CRYPTKIT_CIPHERFILE_ENABLE  0	    /* cipherfile w/symmetric encryption */
#define CRYPTKIT_SYMMETRIC_ENABLE   0	    /* symmetric encryption */
#define CRYPTKIT_ASYMMETRIC_ENABLE  1	    /* asymmetric encryption */
#define CRYPTKIT_MD5_ENABLE	    1	    /* MD5 hash */
#define CRYPTKIT_SHA1_ENABLE	    1	    /* SHA1 hash - needed for GHMAX_LEGACY */
#define CRYPTKIT_HMAC_LEGACY	    1
#define CRYPTKIT_KEY_EXCHANGE	    0	    /* FEE key exchange */
#define CRYPTKIT_HIGH_LEVEL_SIG	    0	    /* high level one-shot signature */
#define CRYPTKIT_GIANT_STACK_ENABLE 0	    /* cache of giants */

#elif	defined(CK_STANDALONE_BUILD)
/*
 * Standalone library build
 */
#define	CRYPTKIT_DER_ENABLE	    0	    
#define	CRYPTKIT_LIBMD_DIGEST	    0	    
#define CRYPTKIT_ELL_PROJ_ENABLE    1	
#define CRYPTKIT_ECDSA_ENABLE	    1	   
#define CRYPTKIT_CIPHERFILE_ENABLE  1	
#define CRYPTKIT_SYMMETRIC_ENABLE   1	
#define CRYPTKIT_ASYMMETRIC_ENABLE  1	
#define CRYPTKIT_MD5_ENABLE	    1
#define CRYPTKIT_SHA1_ENABLE	    1
#define CRYPTKIT_HMAC_LEGACY	    0
#define CRYPTKIT_KEY_EXCHANGE	    1
#define CRYPTKIT_HIGH_LEVEL_SIG	    1
#define CRYPTKIT_GIANT_STACK_ENABLE 1

#elif	defined(CK_MINIMUM_SIG_BUILD)
/*
 * Standalone, just ElGamal signature and key generation
 */
#define	CRYPTKIT_DER_ENABLE	    0	    
#define	CRYPTKIT_LIBMD_DIGEST	    0	    
#define CRYPTKIT_ELL_PROJ_ENABLE    0
#define CRYPTKIT_ECDSA_ENABLE	    0	   
#define CRYPTKIT_CIPHERFILE_ENABLE  0
#define CRYPTKIT_SYMMETRIC_ENABLE   0	
#define CRYPTKIT_ASYMMETRIC_ENABLE  0
#define CRYPTKIT_MD5_ENABLE	    1
/* FIXME convert native ElGamal to use SHA1! */
#define CRYPTKIT_SHA1_ENABLE	    0
#define CRYPTKIT_HMAC_LEGACY	    0
#define CRYPTKIT_KEY_EXCHANGE	    0	
#define CRYPTKIT_HIGH_LEVEL_SIG	    0
#define CRYPTKIT_GIANT_STACK_ENABLE 1

#else

#error You must supply a build configuration. 
#endif

#endif	/* _CK_CONFIG_H_ */
