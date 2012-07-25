/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * feeFunctions.h - general public function declarations
 *
 * Revision History
 * ----------------
 * 8/25/98		ap
 *	Fixed previous check-in comment.
 * 8/24/98		ap
 *	Added tags around #endif comment.
 * 23 Mar 98	Doug Mitchell at Apple
 *	Added initCryptKit().
 * 27 Aug 96	Doug Mitchell at NeXT
 *	Created.
 */

#ifndef	_CK_FEEFUNCTIONS_H_
#define _CK_FEEFUNCTIONS_H_

#ifdef	macintosh
#include <feeTypes.h>
#else
#include <security_cryptkit/feeTypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * One-time only init of CryptKit library.
 */
void initCryptKit(void);

/*
 * Shutdown.
 */
void terminateCryptKit();

#if	defined(NeXT) && !defined(WIN32)

#define PHRASELEN 128

/*
 * Prompt for password, get it in secure manner. Max password length is
 * PHRASELEN. NEXTSTEP only.
 */
extern void getpassword(const char *prompt, char *pbuf);

#endif /* NeXT */

/*
 * obtain a string describing  a feeReturn.
 */
extern const char *feeReturnString(feeReturn frtn);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_FEEFUNCTIONS_H_*/
