/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __heimodadmin_h__
#define __heimodadmin_h__ 1 

#import <CoreFoundation/CoreFoundation.h>
#import <CFOpenDirectory/CFOpenDirectory.h>
#import <Security/Security.h>

/*
 * "CFTypeRef flags" is either a CFStringRef with the kPrincipalFlag
 *     or a CFArrayRef with CFStringRef with kPrincipalFlag
 */


#define kPrincipalFlagInitial			CFSTR("Initial") /* can only be used for initial tickets */
#define kPrincipalFlagForwardable		CFSTR("Forwardable") /* forwardable ticket allowed */
#define kPrincipalFlagProxyable			CFSTR("Proxiable") /* proxyable ticket allowed */
#define kPrincipalFlagRenewable			CFSTR("Renewable") /* renewabled ticket allowed */
#define kPrincipalFlagServer			CFSTR("Server") /* allowed to be used as server */
#define kPrincipalFlagPasswordChangeService	CFSTR("PasswordChangeService") /* allowed to be used as the password change service */
#define kPrincipalFlagOKAsDelegate		CFSTR("OkAsDelegate") /* ok to delegate/forward to */
#define kPrincipalFlagRequireStrongPreAuthentication	CFSTR("RequireStrongPreAuth") /* require smartcard or other strong mech */
#define kPrincipalFlagImmutable			CFSTR("Immutable") /* Immutable, can't remove or possibly change */
#define kPrincipalFlagInvalid			CFSTR("Invalid") /* Invalid and not usable yet */

#define kHeimODACLAll				CFSTR("kHeimODACLAll")
#define kHeimODACLChangePassword		CFSTR("kHeimODACLChangePassword")
#define kHeimODACLList				CFSTR("kHeimODACLList")
#define kHeimODACLDelete			CFSTR("kHeimODACLDelete")
#define kHeimODACLModify			CFSTR("kHeimODACLModify")
#define kHeimODACLAdd				CFSTR("kHeimODACLAdd")
#define kHeimODACLGet				CFSTR("kHeimODACLGet")

/* constants for the srptype argument to HeimODCreateSRPKeys */
#define kHeimSRPGroupRFC5054_4096_PBKDF2_SHA512	CFSTR("kHeimSRPGroupRFC5054_4096_PBKDF2_SHA512")

enum {
    kHeimODAdminSetKeysAppendKey			= 1,
    kHeimODAdminAppendKeySet			= 1, /* add an additional keyset */
    kHeimODAdminDeleteEnctypes			= 2  /* delete enctype from all keysets */
};

enum {
    kHeimODAdminLoadAsAppend				= 1
};

#ifdef __cplusplus
extern "C" {
#endif

/* Creates a support principal in the realm in node */
int		HeimODCreateRealm(ODNodeRef node, CFStringRef realm, CFErrorRef *error);

/* Principals are created with Invalid set and have be be cleared with DeleteFlags */
int		HeimODCreatePrincipalData(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFStringRef principal, CFErrorRef *error); 
int		HeimODRemovePrincipalData(ODNodeRef node, ODRecordRef record, CFStringRef principal, CFErrorRef *error);

/* Manage kerberos flags for this entry */
int		HeimODSetKerberosFlags(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFErrorRef *error);
CFArrayRef	HeimODCopyKerberosFlags(ODNodeRef node, ODRecordRef record, CFErrorRef *error); /* return set flags */
int		HeimODClearKerberosFlags(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFErrorRef *error);

/* Manage ACL for a entry */
int
HeimODSetACL(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFErrorRef *error);
CFArrayRef
HeimODCopyACL(ODNodeRef node, ODRecordRef record, CFErrorRef *error);
int
HeimODClearACL(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFErrorRef *error);

/* Mange server aliases for this record */
int		HeimODAddServerAlias(ODNodeRef node, ODRecordRef record, CFStringRef alias, CFErrorRef *error);
int		HeimODRemoveServerAlias(ODNodeRef node, ODRecordRef record, CFStringRef alias, CFErrorRef *error);
CFArrayRef	HeimODCopyServerAliases(ODNodeRef node, ODRecordRef record, CFErrorRef *error);

/* Lifetimes */
int		HeimODSetKerberosMaxLife(ODNodeRef node, ODRecordRef record, time_t, CFErrorRef *error);
time_t		HeimODGetKerberosMaxLife(ODNodeRef node, ODRecordRef record, CFErrorRef *error);
int		HeimODSetKerberosMaxRenewable(ODNodeRef node, ODRecordRef record, time_t, CFErrorRef *error);
time_t		HeimODGetKerberosMaxRenewable(ODNodeRef node, ODRecordRef record, CFErrorRef *error);

/* Set password */
                /* enctypes are optional, if NULL, default types are used */
                /* if password is NULL, a random password is used */
int		HeimODSetKeys(ODNodeRef node, ODRecordRef record, CFStringRef principal, CFArrayRef enctypes, CFTypeRef password, unsigned long flags, CFErrorRef *error);
CFArrayRef	HeimODCopyDefaultEnctypes(CFErrorRef *error);

/* SRP */
bool		HeimODSetVerifiers(ODNodeRef node, ODRecordRef record, CFStringRef principal, CFArrayRef types, CFTypeRef password, unsigned long flags, CFErrorRef *error);

/**
 * Add/delete/modify keyset
 *
 * Used manipulate Kerberos Keys.
 * This function does not manipulate the keys stored in OpenDirectory, that up to the caller to do.
 *
 * Most callers should not pass in kHeimODAdminAppendKeySet when changing password for users, they should only be used for services that does key rollover and for Mac OS X Server that stores keysets for different principals in same computer record.
 *
 * @param prevKeyset keyset to be manipulated, can me NULL
 * @param principal user changed for, used for salting
 * @param enctypes to set, use HeimODCopyDefaultEnctypes() to get default list
 * @param password new password, can be NULL is enctypes are deleted
 * @param flags
 * 	flags is 0, return a new keyset
 * 	flags is kHeimODAdminAppendKeySet, add additional keyset (keep old versions)
 * 	flags is kHeimODAdminDeleteEnctypes, delete enctype from old keysets
 *
 * @param error return CFErrorRef with user error in case there is one, NULL is allowed if no error is expected.
 * @return the new keyset or NULL on failure, error might be set
 */
CFArrayRef	HeimODModifyKeys(CFArrayRef prevKeyset, CFStringRef principal, CFArrayRef enctypes, CFTypeRef password, unsigned long flags, CFErrorRef *error) __attribute__((cf_returns_retained));


CFArrayRef	HeimODCreateSRPKeys(CFArrayRef srptype, CFStringRef principal, CFTypeRef password, unsigned long flags, CFErrorRef *error);

/**
 * Debug function to print content of a keyset element
 *
 * @param element element to print
 * @param error eventual error, by default, NULL
 * @return the debug string
 */
CFStringRef	HeimODKeysetToString(CFDataRef element, CFErrorRef *error) __attribute__((cf_returns_retained));

/* Mange allowed cert names for this principal: aka AltSecurityIdentities */
int		HeimODAddCertificate(ODNodeRef node, ODRecordRef record, SecCertificateRef ref, CFErrorRef *error);
int		HeimODAddSubjectAltCertName(ODNodeRef node, ODRecordRef record, CFStringRef subject, CFStringRef issuer, CFErrorRef *error);
int		HeimODAddSubjectAltCertSHA1Digest(ODNodeRef node, ODRecordRef record, CFDataRef hash, CFErrorRef *error);
CFArrayRef	HeimODCopySubjectAltNames(ODNodeRef node, ODRecordRef record, CFErrorRef *error);
int		HeimODRemoveSubjectAltElement(ODNodeRef node, ODRecordRef record, CFTypeRef element, CFErrorRef *error); /* return on element as returned by HeimODCopySubjectAltNames */

/* These are for MMe/AppleId certs infratructure */
int		HeimODAddCertificateSubjectAndTrustAnchor(ODNodeRef node, ODRecordRef record, CFStringRef leafSubject, CFStringRef trustAnchorSubject, CFErrorRef *error);
int		HeimODRemoveCertificateSubjectAndTrustAnchor(ODNodeRef node, ODRecordRef record, CFStringRef leafSubject, CFStringRef trustAnchorSubject, CFErrorRef *error);

/* Add Kerberos principal alias for MMe/AppleID */
int		HeimODAddAppleIDAlias(ODNodeRef node, ODRecordRef record, CFStringRef alias, CFErrorRef *error);
int		HeimODRemoveAppleIDAlias(ODNodeRef node, ODRecordRef record, CFStringRef alias, CFErrorRef *error);


/* dump and load entries */
CFDictionaryRef	HeimODDumpRecord(ODNodeRef node, ODRecordRef record, CFStringRef principal, CFErrorRef *error) __attribute__((cf_returns_retained));
bool		HeimODLoadRecord(ODNodeRef node, ODRecordRef record, CFDictionaryRef dict, unsigned long flags, CFErrorRef *error);

struct hdb_entry;
CFDictionaryRef	HeimODDumpHdbEntry(struct hdb_entry *, CFErrorRef *error) __attribute__((cf_returns_retained));

#ifdef __cplusplus
};
#endif

#endif /* __heimodadmin_h__ */
