/*
 * Copyright (c) 2008-2010,2012,2014 Apple Inc. All Rights Reserved.
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


/* This file contains the names of all known entitlements currently
   in use on the system. */

#ifndef _SECURITY_SECENTITLEMENTS_H_
#define _SECURITY_SECENTITLEMENTS_H_

#include <CoreFoundation/CFString.h>

__BEGIN_DECLS

/* Allow other tasks to get this task's name port. This is needed so the app
   can be debugged. */
#define kSecEntitlementGetTaskAllow CFSTR("get-task-allow")

#if TARGET_OS_IPHONE
/* The identifier of this application, typically the same as the
   CFBundleIdentifier.  Used as the default access group for any keychain
   items this application creates and accesses. */
#define kSecEntitlementApplicationIdentifier CFSTR("application-identifier")
#else
/* The identifier of this application, for Mac App Store applications. */
#define kSecEntitlementAppleApplicationIdentifier CFSTR("com.apple.application-identifier")
#define kSecEntitlementApplicationIdentifier kSecEntitlementAppleApplicationIdentifier
#endif

/* The value should be an array of strings.  Each string is the name of an
   access group that the application has access to.  The
   application-identifier is implicitly added to this list.   When creating
   a new keychain item use the kSecAttrAccessGroup attribute (defined in
   <Security/SecItem.h>) to specify its access group.  If omitted, the
   access group defaults to the first access group in this list or the
   application-identifier if there is no keychain-access-groups entitlement. */
#define kSecEntitlementKeychainAccessGroups CFSTR("keychain-access-groups")

/* The value should be an array of strings.  Each string is the name of an
   access group that the application has access to.  The first of
   kSecEntitlementKeychainAccessGroups,
   kSecEntitlementApplicationIdentifier or
   kSecEntitlementAppleSecurityApplicationGroups to have a value becomes the default
   application group for keychain clients that don't specify an explicit one. */
#define kSecEntitlementAppleSecurityApplicationGroups CFSTR("com.apple.security.application-groups")

/* Boolean entitlement, if present the application with the entitlement is
   allowed to modify the which certificates are trusted as anchors using
   the SecTrustStoreSetTrustSettings() and SecTrustStoreRemoveCertificate()
   SPIs. */
#define kSecEntitlementModifyAnchorCertificates CFSTR("modify-anchor-certificates")

#define kSecEntitlementDebugApplications CFSTR("com.apple.springboard.debugapplications")

#define kSecEntitlementOpenSensitiveURL CFSTR("com.apple.springboard.opensensitiveurl")

/* Boolean entitlement, if present allows the application to wipe the keychain
   and truststore. */
#define kSecEntitlementWipeDevice CFSTR("com.apple.springboard.wipedevice")

#define kSecEntitlementRemoteNotificationConfigure CFSTR("com.apple.remotenotification.configure")

#define kSecEntitlementMigrateKeychain CFSTR("migrate-keychain")

#define kSecEntitlementRestoreKeychain CFSTR("restore-keychain")

/* Entitlement needed to call SecKeychainSyncUpdate SPI. */
#define kSecEntitlementKeychainSyncUpdates CFSTR("keychain-sync-updates")

/* Boolean entitlement, if present you get access to the SPIs for keychain sync circle manipulation */
#define kSecEntitlementKeychainCloudCircle CFSTR("keychain-cloud-circle")

/* Associated Domains entitlement (contains array of fully-qualified domain names) */
#define kSecEntitlementAssociatedDomains CFSTR("com.apple.developer.associated-domains")

/* Entitlement needed to call swcd and swcagent processes. */
#define kSecEntitlementPrivateAssociatedDomains CFSTR("com.apple.private.associated-domains")

__END_DECLS

#endif /* !_SECURITY_SECENTITLEMENTS_H_ */
