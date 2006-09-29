
/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPCLIENTPROPERTIES_H
#define _EAP8021X_EAPCLIENTPROPERTIES_H

#include <CoreFoundation/CFString.h>

/*
 * The type of the value corresponding to the following keys are CFString's 
 * unless otherwise noted
 */

/*
 * kEAPClientProp*
 * - properties used to configure the EAPClient, and for the client to report
 *   its configuration needs
 * Note: default values shown in parenthesis (when applicable)
 */
#define kEAPClientPropUserName	       		CFSTR("UserName")
#define kEAPClientPropUserPassword		CFSTR("UserPassword")
#define kEAPClientPropUserPasswordKeychainItemID CFSTR("UserPasswordKeychainItemID")
#define kEAPClientPropAcceptEAPTypes		CFSTR("AcceptEAPTypes") /* array[integer] */

/* 
 * Note:
 * TLSTrustedRootCertificates is not used unless 
 * TLSReplaceTrustedRootCertificates is also supplied.
 */
#define kEAPClientPropTLSReplaceTrustedRootCertificates \
	CFSTR("TLSReplaceTrustedRootCertificates")	/* boolean (false) */
#define kEAPClientPropTLSTrustedRootCertificates \
	CFSTR("TLSTrustedRootCertificates") 		/* array[data] */
#define kEAPClientPropTLSVerifyServerCertificate \
	CFSTR("TLSVerifyServerCertificate") 		/* boolean (true) */
#define kEAPClientPropTLSAllowAnyRoot \
	CFSTR("TLSAllowAnyRoot") 			/* boolean (false) */
#define kEAPClientPropTLSEnableSessionResumption \
	CFSTR("TLSEnableSessionResumption") 		/* boolean (true) */
#define kEAPClientPropTLSUserTrustProceed \
	CFSTR("TLSUserTrustProceed")			/* integer */

/* for TTLS: */
#define kEAPClientPropTTLSInnerAuthentication	CFSTR("TTLSInnerAuthentication")
#define kEAPTTLSInnerAuthenticationPAP		CFSTR("PAP")
#define kEAPTTLSInnerAuthenticationCHAP		CFSTR("CHAP")
#define kEAPTTLSInnerAuthenticationMSCHAP	CFSTR("MSCHAP")
#define kEAPTTLSInnerAuthenticationMSCHAPv2	CFSTR("MSCHAPv2")
#define kEAPTTLSInnerAuthenticationEAP		CFSTR("EAP")

#define kEAPClientPropNewPassword		CFSTR("NewPassword")
/* for TTLS, PEAP, EAP-FAST: */
#define kEAPClientPropOuterIdentity		CFSTR("OuterIdentity")

/* for TLS: */
#define kEAPClientPropTLSIdentityHandle		CFSTR("TLSIdentityHandle") /* EAPSecIdentityHandle */

/* for EAP-FAST */
#define kEAPClientPropEAPFASTUsePAC		CFSTR("EAPFASTUsePAC") /* boolean (false) */
#define kEAPClientPropEAPFASTProvisionPAC	CFSTR("EAPFASTProvisionPAC") /* boolean (false) */
#define kEAPClientPropEAPFASTProvisionPACAnonymously	CFSTR("EAPFASTProvisionPACAnonymously") /* boolean (false) */


/* for EAP-MSCHAPv2 */
#define kEAPClientPropEAPMSCHAPv2ServerChallenge CFSTR("EAPMSCHAPv2ServerChallenge") /* data */
#define kEAPClientPropEAPMSCHAPv2ClientChallenge CFSTR("EAPMSCHAPv2ClientChallenge") /* data */

/*
 * Properties supplied by the client as published/additional properties
 */
#define kEAPClientInnerEAPType		CFSTR("InnerEAPType")	/* integer (EAPType) */
#define kEAPClientInnerEAPTypeName	CFSTR("InnerEAPTypeName")
#define kEAPClientPropTLSServerCertificateChain	\
	CFSTR("TLSServerCertificateChain") /* array[data] */
#define kEAPClientPropTLSTrustClientStatus	CFSTR("TLSTrustClientStatus") /* integer (EAPClientStatus) */
#define kEAPClientPropTLSSessionWasResumed \
	CFSTR("TLSSessionWasResumed")	/* boolean */
#define kEAPClientPropTLSNegotiatedCipher \
	CFSTR("TLSNegotiatedCipher")	/* integer (UInt32) */

#define kEAPClientPropEAPFASTPACWasProvisioned	CFSTR("EAPFASTPACWasProvisioned") /* boolean */

/* 
 * Deprecated properties
 */
#define kEAPClientPropTLSTrustedServerCertificates \
	CFSTR("TLSTrustedServerCertificates") 		/* array[data] */
#define kEAPClientPropIdentity			CFSTR("Identity")
#endif _EAP8021X_EAPCLIENTPROPERTIES_H
