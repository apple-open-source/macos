/*
 *  KerberosHelper.h
 *  KerberosHelper
*/

/*
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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
#ifndef _KERBEROSHELPER_H_
#define _KERBEROSHELPER_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
	KRBCreateSession will start a kerberos session and return a pointer to a kerberosSession that is passed to the other SPIs.
		inHostName is the name of the host to get the service principal and/or user principal for.  If inHostName is NULL, it is
			assumed that the local machine is the target.
		inAdvertisedPrincipal is a service principal guess (can be NULL), perhaps provided by the service. This is not secure 
			and is given the least priorty when other information is available
		outKerberosSession is a pointer that should be passed to the other KRB functions.
*/
OSStatus KRBCreateSession (CFStringRef inHostName, CFStringRef inAdvertisedPrincipal, void **outKerberosSession);

/*
	KRBCopyREALM will return the best-guess REALM for the host that was passed to KRBCreateSession
		inKerberosSession is the pointer returned by KRBCreateSession
		outREALM is the REALM of the host
*/
OSStatus KRBCopyRealm (void *inKerberosSession, CFStringRef *outRealm);

/*
	KRBCopyKeychainLookupInfo will return a dictionary containing information related to Kerberos and keychain items.
		inKerberosSession is the pointer returned by KRBCreateSession
		inUsername is an available and usable Username or NULL
		outKeychainLookupInfo is a dictionary containing keychain lookup info and if it is acceptable to store a
			password in the keychain.

		This call for use by KerberosAgent and NetAuthAgent only.
		
		outKeychainLookupInfo
			kKRBUsernameKey					: CFStringRef
			kKRBKeychainAccountName			: CFStringRef
			kKRBDisableSaveToKeychainKey	: CFBooleanRef
		
*/	
#define	kKRBDisableSaveToKeychainKey		CFSTR("DisableSaveToKeychain")
#define kKRBKeychainAccountName				CFSTR("KeychainAccountName")
#define kKRBAgentBundleIdentifier			CFSTR("edu.mit.Kerberos.KerberosAgent")

OSStatus KRBCopyKeychainLookupInfo (void *inKerberosSession, CFStringRef inUsername, CFDictionaryRef *outKeychainLookupInfo);

/*
	KRBCopyServicePrincipal will return the service principal for the inServiceName on the host associated with inKerberosSession
		inKerberosSession is the pointer returned by KRBCreateSession
		inServiceName is the name of the service on the host, it can be NULL if inAdvertisedPrincipal was non-NULL.  
			However it is highly recommended that this be set as it is insecure to rely on remotely provided information 
		outServicePrincipal the service principal
 */
OSStatus KRBCopyServicePrincipal (void *inKerberosSession, CFStringRef inServiceName, CFStringRef *outServicePrincipal);

/*
	 KRBCopyClientPrincipalInfo will return a dictionary with the user principal and other information.
	 inKerberosSession is the pointer returned by KRBCreateSession.
	 inOptions a dictionary with options regarding the acquisition of the user principal.
	 inIdentityRef is a reference to list of usable identities
	 outClientPrincipalInfo a dictionary containing the user principal and other information necessary to get a ticket.
	 
	 inOptions Dictionary Keys
		kKRBAllowKerberosUIKey			: CFStringRef [See AllowKeberosUI values]
		kKRBServerDisplayNameKey		: CFStringRef
		kKRBUsernameKey					: CFStringRef
		kKRBClientPasswordKey			: CFStringRef
		kKRBCertificateKey				: SecCertificateRef
	 outClientPrincipalInfo
		kKRBClientPrincipalKey			: CFStringRef
		kKRBUsernameKey					: CFStringRef
		kKRBCertificateHashKey			: CFStringRef
		kKRBCertificateInferredLabelKey : CFStringRef
		and private information
*/
#define kKRBUsernameKey						CFSTR("Username")
#define kKRBClientPasswordKey               CFSTR("Password")
#define kKRBCertificateKey					CFSTR("Certificate")
#define kKRBCertificateHashKey				CFSTR("CetificateHash")
#define kKRBUsingCertificateKey				CFSTR("UsingCertificate")
#define kKRBCertificateInferredLabelKey		CFSTR("CertificateInferredLabel")
#define kKRBAllowKerberosUI					CFSTR("AllowKerberosUI")
#define kKRBServerDisplayNameKey			CFSTR("ServerDisplayName")
#define kKRBClientPrincipalKey              CFSTR("ClientPrincipal")

/* AllowKeberosUI values */
#define kKRBOptionNoUI						CFSTR("NoUI")
#define kKRBOptionAllowUI					CFSTR("AllowUI")
#define kKRBOptionForceUI					CFSTR("ForceUI")

OSStatus KRBCopyClientPrincipalInfo (void *inKerberosSession,  CFDictionaryRef inOptions, CFDictionaryRef *outClientPrincipalInfo);

	
/*
	 KRBTestForExistingTicket will look for an existing ticket in the
	 ccache.  This call looks for a principal that matches the principal
	 stored in the outClientPrincipalInfo dictionary fom the
	 KRBCopyClientPrincipalInfo call.
	 This call should be performed before prompting the user to enter credential
	 information.
	 inKerberosSession is the pointer returned by KRBCreateSession
	 inClientPrincipalInfo the dictionary containing the
	 kKRBClientPrincipalKey.
*/
OSStatus KRBTestForExistingTicket (void *inKerberosSession, CFDictionaryRef inClientPrincipalInfo);
	

/*
	 KRBAcquireTicket will acquire a ticket for the user.
		inKerberosSession is the pointer returned by KRBCreateSession.
		inClientPrincipalInfo is the outClientPrincipalInfo dictionary from KRBCopyClientPrincipalInfo.
*/
OSStatus KRBAcquireTicket(void *inKerberosSession, CFDictionaryRef inClientPrincipalInfo);


/*
	KRBCloseSession will release the kerberos session
		inKerberosSession is the pointer returned by KRBCreateSession.
*/
OSStatus KRBCloseSession (void *inKerberosSession);

#ifdef __cplusplus
}
#endif

#endif /* _KERBEROSHELPER_H_ */
