/*
 * Copyright (c) 2002-2011 Apple Inc. All rights reserved.
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
#include <TargetConditionals.h>

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

/**
 ** Properties applicable to most protocols
 **/
#define kEAPClientPropUserName	       		CFSTR("UserName")
#define kEAPClientPropUserPassword		CFSTR("UserPassword")
#define kEAPClientPropUserPasswordKeychainItemID CFSTR("UserPasswordKeychainItemID")
#define kEAPClientPropOneTimeUserPassword CFSTR("OneTimeUserPassword") /* boolean (false) */
#define kEAPClientPropAcceptEAPTypes		CFSTR("AcceptEAPTypes") /* array[integer] */
#define kEAPClientPropInnerAcceptEAPTypes	CFSTR("InnerAcceptEAPTypes") /* array[integer] */

/**
 ** Properties for TLS-based authentication (EAP/TLS, EAP/TTLS, PEAP, EAP-FAST)
 **/
/*
 * kEAPClientPropTLSTrustedCertificates
 * - which certificates we should trust for this authentication session
 * - may contain root, leaf, or intermediate certificates
 */
#define kEAPClientPropTLSTrustedCertificates \
	CFSTR("TLSTrustedCertificates") 		/* array[data] */

/*
 * kEAPClientPropTLSTrustedServerNames
 * - which server names we should trust for this authentication session
 */
#define kEAPClientPropTLSTrustedServerNames \
	CFSTR("TLSTrustedServerNames") 		/* array[string] */

/*
 * kEAPClientPropProfileID
 * - the profile identifier of the configuration, if the configuration came
 *   from an EAPOLClientProfileRef
 */
#define kEAPClientPropProfileID 	CFSTR("ProfileID")	/* string */

#if TARGET_OS_EMBEDDED
/*
 * kEAPClientPropTLSSaveTrustExceptions
 * - tells the client to save trust exceptions for the current server
 *   certificate chain, kEAPClientPropTLSUserTrustProceedCertificateChain
 */
#define kEAPClientPropTLSSaveTrustExceptions \
	CFSTR("TLSSaveTrustExceptions")			/* boolean (false) */

/*
 * kEAPClientPropTLSAllowTrustExceptions
 * - enables trust exceptions for storing dynamic trust
 * - the default value of this property is true unless the 
 *   kEAPClientPropTLSTrustedCertificates property is specified, in which
 *   case the default is false
 */
#define kEAPClientPropTLSAllowTrustExceptions \
	CFSTR("TLSAllowTrustExceptions") 		/* boolean (see above) */
/*
 * kEAPClientPropTLSTrustExceptionsDomain 
 * kEAPClientPropTLSTrustExceptionsID
 * - properties used to locate the appropriate trust exception for the
 *   current authentication session
 */
#define kEAPClientPropTLSTrustExceptionsDomain \
	CFSTR("TLSTrustExceptionsDomain")
#define kEAPClientPropTLSTrustExceptionsID \
	CFSTR("TLSTrustExceptionsID")

/*
 * kEAPTLSTrustExceptionsDomain*
 *
 * Values for the kEAPClientPropTLSTrustExceptionsDomain property
 *
 * kEAPTrustExceptionsDomainWirelessSSID
 * - used when the desired trust domain is the wireless SSID to which we
 *   are authenticating
 *
 * kEAPTrustExceptionsDomainProfileID
 * - used when the desired trust domain is the UUID of the configuration profile
 *
 * kEAPTLSTrustExceptionsDomainNetworkInterfaceName
 * - used when the desired trust domain is the unique network interface name
 */
#define kEAPTLSTrustExceptionsDomainWirelessSSID \
    	CFSTR("WirelessSSID")
#define kEAPTLSTrustExceptionsDomainProfileID \
    	CFSTR("ProfileID")
#define kEAPTLSTrustExceptionsDomainNetworkInterfaceName \
    	CFSTR("NetworkInterfaceName")
#else /* TARGET_OS_EMBEDDED */
/*
 * kEAPClientPropTLSAllowTrustDecisions
 * - enables trust decisions by the user
 * - the default value of this property is true unless the 
 *   kEAPClientPropTLSTrustedCertificates property is specified, in which
 *   case the default is false
 */
#define kEAPClientPropTLSAllowTrustDecisions \
	CFSTR("TLSAllowTrustDecisions")		/* boolean (see above) */
#endif /* TARGET_OS_EMBEDDED */

#define kEAPClientPropTLSVerifyServerCertificate \
	CFSTR("TLSVerifyServerCertificate") 		/* boolean (true) */
#define kEAPClientPropTLSEnableSessionResumption \
	CFSTR("TLSEnableSessionResumption") 		/* boolean (true) */
#define kEAPClientPropTLSUserTrustProceedCertificateChain \
	CFSTR("TLSUserTrustProceedCertificateChain")	/* array[data] */

/*
 * kEAPClientPropSystemModeCredentialsSource
 * - tells the EAP client to use an alternate source for credentials when
 *   running in System mode
 * - when set to kEAPClientCredentialsSourceActiveDirectory, the EAP client
 *   will attempt to use the machine name/password used by Active Directory;
 *   if those credentials are missing, the authentication will fail
 */ 
#define kEAPClientPropSystemModeCredentialsSource	CFSTR("SystemModeCredentialsSource")
#define kEAPClientCredentialsSourceActiveDirectory	CFSTR("ActiveDirectory")

/**
 ** Properties for TTLS
 **/
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


/*
 * for EAP-MSCHAPv2
 *
 * Note: these are only used as an internal communication mechanism between the 
 * outer authentication and EAP-MSCHAPv2.
 */
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
 * Deprecated/unused properties
 */
#define kEAPClientPropIdentity			CFSTR("Identity")
#define kEAPClientPropTLSReplaceTrustedRootCertificates \
	CFSTR("TLSReplaceTrustedRootCertificates")	/* boolean (false) */
#define kEAPClientPropTLSTrustedRootCertificates \
	CFSTR("TLSTrustedRootCertificates") 		/* array[data] */
#define kEAPClientPropTLSAllowAnyRoot \
	CFSTR("TLSAllowAnyRoot") 			/* boolean (false) */
#endif /* _EAP8021X_EAPCLIENTPROPERTIES_H */
