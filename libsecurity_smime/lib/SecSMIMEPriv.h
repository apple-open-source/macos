/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*!
    @header SecSMIMEPriv.h
    @copyright 2004 Apple Computer, Inc. All Rights Reserved.

    @availability 10.4 and later
    @abstract Private S/MIME Specific routines.
    @discussion Header file for routines specific to S/MIME.  Keep
		things that are pure pkcs7 out of here; this is for
		S/MIME policy, S/MIME interoperability, etc.
*/

#ifndef _SECURITY_SECSMIMEPRIV_H_
#define _SECURITY_SECSMIMEPRIV_H_ 1

#include <Security/SecCmsBase.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cipher family IDs used for configuring ciphers for export control
 */

/* Cipher Suite "Families" */
#define CIPHER_FAMILYID_MASK			0xFFFF0000L
#define CIPHER_FAMILYID_SMIME			0x00010000L

/* SMIME "Cipher Suites" */
/*
 * Note that it is assumed that the cipher number itself can be used
 * as a bit position in a mask, and that mask is currently 32 bits wide.
 * So, if you want to add a cipher that is greater than 0033, secmime.c
 * needs to be made smarter at the same time.
 */
#define	SMIME_RC2_CBC_40		(CIPHER_FAMILYID_SMIME | 0001)
#define	SMIME_RC2_CBC_64		(CIPHER_FAMILYID_SMIME | 0002)
#define	SMIME_RC2_CBC_128		(CIPHER_FAMILYID_SMIME | 0003)
#define	SMIME_DES_CBC_56		(CIPHER_FAMILYID_SMIME | 0011)
#define	SMIME_DES_EDE3_168		(CIPHER_FAMILYID_SMIME | 0012)
#define SMIME_AES_CBC_128		(CIPHER_FAMILYID_SMIME | 0013)
#define	SMIME_RC5PAD_64_16_40		(CIPHER_FAMILYID_SMIME | 0021)
#define	SMIME_RC5PAD_64_16_64		(CIPHER_FAMILYID_SMIME | 0022)
#define	SMIME_RC5PAD_64_16_128		(CIPHER_FAMILYID_SMIME | 0023)
#define	SMIME_FORTEZZA			(CIPHER_FAMILYID_SMIME | 0031)


/*
 * Initialize the local recording of the user S/MIME cipher preferences.
 * This function is called once for each cipher, the order being
 * important (first call records greatest preference, and so on).
 * When finished, it is called with a "which" of CIPHER_FAMILID_MASK.
 * If the function is called again after that, it is assumed that
 * the preferences are being reset, and the old preferences are
 * discarded.
 *
 * XXX This is for a particular user, and right now the storage is
 * XXX local, static.  The preference should be stored elsewhere to allow
 * XXX for multiple uses of one library?  How does SSL handle this;
 * XXX it has something similar?
 *
 *  - The "which" values are defined in ciferfam.h (the SMIME_* values,
 *    for example SMIME_DES_CBC_56).
 *  - If "on" is non-zero then the named cipher is enabled, otherwise
 *    it is disabled.  (It is not necessary to call the function for
 *    ciphers that are disabled, however, as that is the default.)
 *
 * If the cipher preference is successfully recorded, SECSuccess
 * is returned.  Otherwise SECFailure is returned.  The only errors
 * are due to failure allocating memory or bad parameters/calls:
 *	SEC_ERROR_XXX ("which" is not in the S/MIME cipher family)
 *	SEC_ERROR_XXX (function is being called more times than there
 *		are known/expected ciphers)
 */
extern OSStatus SecSMIMEEnableCipher(uint32 which, Boolean on);

/*
 * Initialize the local recording of the S/MIME policy.
 * This function is called to allow/disallow a particular cipher.
 *
 * XXX This is for a the current module, I think, so local, static storage
 * XXX is okay.  Is that correct, or could multiple uses of the same
 * XXX library expect to operate under different policies?
 *
 *  - The "which" values are defined in ciferfam.h (the SMIME_* values,
 *    for example SMIME_DES_CBC_56).
 *  - If "on" is non-zero then the named cipher is enabled, otherwise
 *    it is disabled.
 */
extern OSStatus SecSMIMEAllowCipher(uint32 which, Boolean on);

/*
 * Does the current policy allow S/MIME decryption of this particular
 * algorithm and keysize?
 */
extern Boolean SecSMIMEDecryptionAllowed(SECAlgorithmID *algid, SecSymmetricKeyRef key);

/*
 * Does the current policy allow *any* S/MIME encryption (or decryption)?
 *
 * This tells whether or not *any* S/MIME encryption can be done,
 * according to policy.  Callers may use this to do nicer user interface
 * (say, greying out a checkbox so a user does not even try to encrypt
 * a message when they are not allowed to) or for any reason they want
 * to check whether S/MIME encryption (or decryption, for that matter)
 * may be done.
 *
 * It takes no arguments.  The return value is a simple boolean:
 *   PR_TRUE means encryption (or decryption) is *possible*
 *	(but may still fail due to other reasons, like because we cannot
 *	find all the necessary certs, etc.; PR_TRUE is *not* a guarantee)
 *   PR_FALSE means encryption (or decryption) is not permitted
 *
 * There are no errors from this routine.
 */
extern Boolean SecSMIMEEncryptionPossible(void);

/*
 * SecSMIMECreateSMIMECapabilities - get S/MIME capabilities attr value
 *
 * scans the list of allowed and enabled ciphers and construct a PKCS9-compliant
 * S/MIME capabilities attribute value.
 */
extern OSStatus SecSMIMECreateSMIMECapabilities(SecArenaPoolRef pool, CSSM_DATA_PTR dest, Boolean includeFortezzaCiphers);

/*
 * SecSMIMECreateSMIMEEncKeyPrefs - create S/MIME encryption key preferences attr value
 */
extern OSStatus SecSMIMECreateSMIMEEncKeyPrefs(SecArenaPoolRef pool, CSSM_DATA_PTR dest, SecCertificateRef cert);

/*
 * SecSMIMECreateMSSMIMEEncKeyPrefs - create S/MIME encryption key preferences attr value using MS oid
 */
extern OSStatus SecSMIMECreateMSSMIMEEncKeyPrefs(SecArenaPoolRef pool, CSSM_DATA_PTR dest, SecCertificateRef cert);

/*
 * SecSMIMEGetCertFromEncryptionKeyPreference - find cert marked by EncryptionKeyPreference
 *          attribute
 */
extern SecCertificateRef SecSMIMEGetCertFromEncryptionKeyPreference(SecKeychainRef keychainOrArray, CSSM_DATA_PTR DERekp);


#ifdef __cplusplus
}
#endif

#endif /* _SECURITY_SECSMIMEPRIV_H_ */
