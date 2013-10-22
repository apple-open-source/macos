/*
 * Copyright (c) 2008-2009 Apple Inc. All Rights Reserved.
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

/*!
	@header SecBasePriv
	SecBasePriv contains private error codes from the Security framework. 
*/

#ifndef _SECURITY_SECBASEPRIV_H_
#define _SECURITY_SECBASEPRIV_H_

#include <Security/SecBase.h>

__BEGIN_DECLS

/*******************************************************
 *** Private OSStatus values unique to Security APIs ***
 *******************************************************/

/*
    Note: the comments that appear after these errors are used to create SecErrorMessages.strings.
    The comments must not be multi-line, and should be in a form meaningful to an end user. If
    a different or additional comment is needed, it can be put in the header doc format, or on a
    line that does not start with errZZZ.
*/

enum
{
    errSecReadOnly               = -25292,	/* This keychain cannot be modified. */
    errSecNoSuchKeychain         = -25294,	/* The specified keychain could not be found. */
    errSecInvalidKeychain        = -25295,	/* The specified keychain is not a valid keychain file. */
    errSecDuplicateKeychain      = -25296,	/* A keychain with the same name already exists. */
    errSecDuplicateCallback      = -25297,	/* The specified callback function is already installed. */
    errSecInvalidCallback        = -25298,	/* The specified callback function is not valid. */
    errSecBufferTooSmall         = -25301,	/* There is not enough memory available to use the specified item. */
    errSecDataTooLarge           = -25302,	/* This item contains information which is too large or in a format that cannot be displayed. */
    errSecNoSuchAttr             = errSecParam, // -25303,	/* The specified attribute does not exist. */
    errSecInvalidItemRef         = -25304,	/* The specified item is no longer valid. It may have been deleted from the keychain. */
    errSecInvalidSearchRef       = -25305,	/* Unable to search the current keychain. */
    errSecNoSuchClass            = errSecParam, // -25306,	/* The specified item does not appear to be a valid keychain item. */
    errSecNoDefaultKeychain      = -25307,	/* A default keychain could not be found. */
    errSecReadOnlyAttr           = -25309,	/* The specified attribute could not be modified. */
    errSecWrongSecVersion        = -25310,	/* This keychain was created by a different version of the system software and cannot be opened. */
    errSecKeySizeNotAllowed      = errSecParam, // -25311,	/* This item specifies a key size which is too large. */
    errSecNoStorageModule        = -25312,	/* A required component (data storage module) could not be loaded. You may need to restart your computer. */
    errSecNoCertificateModule    = -25313,	/* A required component (certificate module) could not be loaded. You may need to restart your computer. */
    errSecNoPolicyModule         = -25314,	/* A required component (policy module) could not be loaded. You may need to restart your computer. */
    errSecInteractionRequired    = -25315,	/* User interaction is required, but is currently not allowed. */
    errSecDataNotAvailable       = -25316,	/* The contents of this item cannot be retrieved. */
    errSecDataNotModifiable      = -25317,	/* The contents of this item cannot be modified. */
    errSecCreateChainFailed      = -25318,	/* One or more certificates required to validate this certificate cannot be found. */
	errSecACLNotSimple           = -25240,	/* The specified access control list is not in standard (simple) form. */
	errSecInvalidTrustSetting    = -25242,	/* The specified trust setting is invalid. */
	errSecNoAccessForItem        = -25243,	/* The specified item has no access control. */
	errSecInvalidOwnerEdit       = -25244,  /* Invalid attempt to change the owner of this item. */
	errSecInvalidPrefsDomain	 = -25319,  /* The specified preferences domain is not valid. */
	errSecTrustNotAvailable      = -25245,	/* No trust results are available. */
	errSecUnsupportedFormat		 = -25256,  /* Import/Export format unsupported. */
	errSecUnknownFormat			 = -25257,  /* Unknown format in import. */
	errSecKeyIsSensitive		 = -25258,  /* Key material must be wrapped for export. */
	errSecMultiplePrivKeys		 = -25259,  /* An attempt was made to import multiple private keys. */
	errSecPassphraseRequired	 = -25260,  /* Passphrase is required for import/export. */
	errSecInvalidPasswordRef     = -25261,  /* The password reference was invalid. */
	errSecInvalidTrustSettings 	 = -25262,	/* The Trust Settings record was corrupted. */
	errSecNoTrustSettings		 = -25263,	/* No Trust Settings were found. */
	errSecPkcs12VerifyFailure 	 = -25264,	/* MAC verification failed during PKCS12 import. */
    errSecInvalidCertificate     = errSecDecode, // -26265,  /* This certificate could not be decoded. */
    errSecNotSigner              = -26267,  /* A certificate was not signed by its proposed parent. */
    errSecPolicyDenied			 = -26270,  /* The certificate chain was not trusted due to a policy not accepting it. */
    errSecInvalidKey             = errSecDecode, // -26274,  /* The provided key material was not valid. */
    errSecInternal               = -26276,  /* An internal error occured in the Security framework. */
    errSecUnsupportedAlgorithm   = errSecUnimplemented, // -26268,  /* An unsupported algorithm was encountered. */
    errSecUnsupportedOperation   = errSecUnimplemented, // -26271,  /* The operation you requested is not supported by this key. */
    errSecUnsupportedPadding     = errSecParam, // -26273,  /* The padding you requested is not supported. */
    errSecItemInvalidKey         = errSecParam, // -34000,  /* A string key in dictionary is not one of the supported keys. */
    errSecItemInvalidKeyType     = errSecParam, // -34001,  /* A key in a dictionary is neither a CFStringRef nor a CFNumberRef. */
    errSecItemInvalidValue       = errSecParam, // -34002,  /* A value in a dictionary is an invalid (or unsupported) CF type. */
    errSecItemClassMissing       = errSecParam, // -34003,  /* No kSecItemClass key was specified in a dictionary. */
    errSecItemMatchUnsupported   = errSecParam, // -34004,  /* The caller passed one or more kSecMatch keys to a function which does not support matches. */
    errSecUseItemListUnsupported = errSecParam, // -34005,  /* The caller passed in a kSecUseItemList key to a function which does not support it. */
    errSecUseKeychainUnsupported = errSecParam, // -34006,  /* The caller passed in a kSecUseKeychain key to a function which does not support it. */
    errSecUseKeychainListUnsupported = errSecParam, // -34007,  /* The caller passed in a kSecUseKeychainList key to a function which does not support it. */
    errSecReturnDataUnsupported  = errSecParam, // -34008,  /* The caller passed in a kSecReturnData key to a function which does not support it. */
    errSecReturnAttributesUnsupported = errSecParam, // -34009,  /* The caller passed in a kSecReturnAttributes key to a function which does not support it. */
    errSecReturnRefUnsupported   = errSecParam, // -34010,  /* The caller passed in a kSecReturnRef key to a function which does not support it. */
    errSecReturnPersitentRefUnsupported   = errSecParam, // -34010,  /* The caller passed in a kSecReturnPersistentRef key to a function which does not support it. */
    errSecValueRefUnsupported    = errSecParam, // -34012,  /* The caller passed in a kSecValueRef key to a function which does not support it. */
    errSecValuePersistentRefUnsupported = errSecParam, // -34013,  /* The caller passed in a kSecValuePersistentRef key to a function which does not support it. */
    errSecReturnMissingPointer   = errSecParam, // -34014,  /* The caller passed asked for something to be returned but did not pass in a result pointer. */
	errSecMatchLimitUnsupported  = errSecParam, // -34015,  /* The caller passed in a kSecMatchLimit key to a call which does not support limits. */
	errSecItemIllegalQuery       = errSecParam, // -34016,  /* The caller passed in a query which contained too many keys. */
	errSecWaitForCallback        = -34017,  /* This operation is incomplete, until the callback is invoked (not an error). */
    errSecMissingEntitlement     = -34018,  /* Internal error when a required entitlement isn't present. */
    errSecUpgradePending         = -34019,  /* Error returned if keychain database needs a schema migration but the device is locked, clients should wait for a device unlock notification and retry the command. */

    errSecMPSignatureInvalid     = -25327,  /* Signature invalid on MP message */
    errSecOTRTooOld              = -25328,  /* Message is too old to use */
    errSecOTRIDTooNew            = -25329,  /* Key ID is too new to use! Message from the future? */

};


__END_DECLS

#endif /* !_SECURITY_SECBASEPRIV_H_ */
