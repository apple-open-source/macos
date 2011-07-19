/*
 * Copyright (c) 2008 - 2010 Apple Inc. All rights reserved.
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

#include <smbclient/smbclient.h>
#include <smbclient/ntstatus.h>
#include <smbclient/smbclient_internal.h>

#include <algorithm>
#include <vector>
#include <cstdlib>
#include <assert.h>
#include <string>


#include "LsarLookup.h"
#include "memory.hpp"
#include "rpc_helpers.hpp"


extern "C" {
#include <dce/dcethread.h>
}

/* 
 * Create a CFString that contains a fully qualified account names based on 
 * either DNS or NetBIOS names. For example: example.example.com\user_name or 
 * example\user_name, where the generalized form is domain\user account name, 
 * and domain is either the fully qualified DNS name or the NetBIOS name of the 
 * trusted domain.
 *
 * Both account name and either domain (DNS or NetBIOS) names must exist.
 */

static 
CFStringRef CreateFullyQualifiedAccountName(PRPC_UNICODE_STRING AccountName, 
											PRPC_UNICODE_STRING DomainName)
{
	CFStringRef AccountRef = NULL, DomainRef = NULL;
	CFMutableStringRef FullyQualifiedRef = NULL;
	UInt16 *nullchar;
	
	/* We require both are we fail */
	if (!AccountName || (AccountName->Buffer == NULL)) {
		return NULL;
	}
	if (!DomainName || (DomainName->Buffer == NULL)) {
		return NULL;
	}
	/* 
	 * Samba adds two null bytes to the names and counts them as part of the
	 * length. So if either name ends with two null bytes we need to remove them
	 * from the length field before we create the qualified name.
	 */
	nullchar = (UInt16 *)(void *)&((UInt8 *)(DomainName->Buffer))[DomainName->Length-2];
	if (*nullchar == 0) { 
		/* Remove the null terminate bytes */
		DomainName->Length -= 2;
	}
	nullchar = (UInt16 *)(void *)&((UInt8 *)(AccountName->Buffer))[AccountName->Length-2];
	if (*nullchar == 0) {
		/* Remove the null terminate bytes */
		AccountName->Length -= 2;
	}
	DomainRef = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, 
										   (const UInt8 *)DomainName->Buffer, 
										   DomainName->Length, 
										   kCFStringEncodingUTF16LE, 
										   false, kCFAllocatorNull);
	if (DomainRef) {
		AccountRef = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, 
											 (const UInt8 *)AccountName->Buffer, 
											 AccountName->Length, 
											 kCFStringEncodingUTF16LE, 
											 false, kCFAllocatorNull);
	}
	if (!AccountRef) {
		/* Something went wrong fail */
		goto done;
	}
	
	/* Create the fully qualifed name starting with the domain */
	FullyQualifiedRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, DomainRef);
	if (FullyQualifiedRef == NULL) {
		/* Something went wrong fail */
		goto done;
	}
	/* Now add the back slash separator */
	CFStringAppend(FullyQualifiedRef, CFSTR("\\"));
	/* Now add the account name */
	CFStringAppend(FullyQualifiedRef, AccountRef);
	
done:
	if (AccountRef) {
		CFRelease(AccountRef);
	}
	if (DomainRef) {
		CFRelease(DomainRef);
	}
	return FullyQualifiedRef;
}	

/* 
 * Given a server and a name obtain the sid. We only support looking up one name
 * at a time currently with this routine.
 * 
 * ServerName	-	The UTF16 name of the server.
 * AccountName	-	Contains the security principal names to translate. The 
 *					RPC_UNICODE_STRING structure is defined in [MS-DTYP] section 
 *					2.3.5. 
 *					The following name forms MUST be supported:
 *					User principal names (UPNs), such as user_name@example.example.com.
 *
 *					Fully qualified account names based on either DNS or NetBIOS names. 
 *					For example: example.example.com\user_name or example\user_name, 
 *					where the generalized form is domain\user account name, and 
 *					domain is either the fully qualified DNS name or the NetBIOS 
 *					name of the trusted domain.
 *
 *					Unqualified or isolated names, such as user_name.
 * NOTE: The comparisons used by the RPC server MUST NOT be case-sensitive, so 
 * case for inputs is not important.
 * 
 */
static 
NTSTATUS GetAccountNameSID(WCHAR * ServerName, PRPC_UNICODE_STRING AccountName, 
						   ntsid_t **ntsid, rpc_binding *binding)
{
	LSAPR_OBJECT_ATTRIBUTES ObjectAttributes;
	LSAPR_HANDLE PolicyHandle = NULL;
	ACCESS_MASK DesiredAccess = 0x00000800;
    NTSTATUS nt_status = STATUS_SUCCESS;
    error_status_t rpc_status = rpc_s_ok;
	LSAPR_TRANSLATED_SIDS TranslatedSids;
	idl_ulong_int RequestCount, MappedCount = 0;
	PLSAPR_REFERENCED_DOMAIN_LIST ReferencedDomains = NULL;
	SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
	PRPC_SID DomainSid = NULL;
	ntsid_t *sid;
	int ii;

	RequestCount = 1; /* We only request one name at a time */
	memset(&ObjectAttributes, 0, sizeof(ObjectAttributes));
	/*
	 * We could just leave ObjectAttributes zeroed out since that works. Notice
	 * that windows fills in the SecurityQualityOfService so lets do the same
	 * for now.
	 */
	SecurityQualityOfService.Length = 12; /* Size of SecurityQualityOfService */
	SecurityQualityOfService.ImpersonationLevel = SecurityImpersonation;
	SecurityQualityOfService.ContextTrackingMode = 1;
	SecurityQualityOfService.EffectiveOnly = 0;
	ObjectAttributes.Length = 24;	 /* Size of ObjectAttributes */
	ObjectAttributes.SecurityQualityOfService = &SecurityQualityOfService;

 	memset(&TranslatedSids, 0, sizeof(TranslatedSids));
	
	DCETHREAD_TRY
		/* 
		 * The second parameter in LsarOpenPolicy2 is the SystemName, window always
		 * puts the server name in here, it helps with tracing so we will also. Since
		 * the docs say the following it should hurt:
		 * SystemName: This parameter does not have any effect on message processing 
		 *			    in any environment. It MUST be ignored on receipt.
		*/
		nt_status = LsarOpenPolicy2(binding->get(), ServerName, &ObjectAttributes,
							   DesiredAccess, &PolicyHandle, &rpc_status);
		if ((NT_SUCCESS(nt_status)) && (rpc_status == rpc_s_ok)) {
			nt_status = LsarLookupNames(binding->get(), PolicyHandle, RequestCount, 
										AccountName, &ReferencedDomains, &TranslatedSids, 
										LsapLookupWksta, &MappedCount, &rpc_status);
		}
		/* 
		 * Close the handle if we have one. Ignore any any errors, since I am 
		 * not sure what I would do about them anyways
		 */
		if (PolicyHandle) {
			(void)LsarClose(binding->get(), &PolicyHandle, &rpc_status);
		}
	DCETHREAD_CATCH_ALL(exc)
		/* Catch any exceptions */
		rpc_status = rpc_exception_status(exc);
	DCETHREAD_ENDTRY
	
	if (rpc_status != rpc_s_ok) {
        SMBLogInfo("RPC to lsarpc gave rpc status of %#08x", ASL_LEVEL_DEBUG, rpc_status);
		/* Need a routine that converts rpc status to nt status */
		return (STATUS_UNSUCCESSFUL);
    } else if (!NT_SUCCESS(nt_status)) {		
        SMBLogInfo("RPC to lsarpc gave nt status of %#08x", ASL_LEVEL_DEBUG, nt_status);
		return nt_status;
	}
	/* Make sure they returned the users RID */
	if ((TranslatedSids.Entries == 0) || 
		(TranslatedSids.Sids == NULL) || 
		(TranslatedSids.Sids->Use != SidTypeUser)) {
        SMBLogInfo("No Relative Id (RID)?", ASL_LEVEL_DEBUG);
		return STATUS_NO_SUCH_USER;
	}
	/* Now get the domain sid */
	if (ReferencedDomains && ReferencedDomains->Entries && ReferencedDomains->Domains) {
		DomainSid = ReferencedDomains->Domains->Sid;
	}
	/* No domain sid returned, error out */
	if (!DomainSid) {
        SMBLogInfo("No domain sid?", ASL_LEVEL_DEBUG);
		return (STATUS_NO_SUCH_DOMAIN);
	}
	/* Need room for the users RID */
	if (DomainSid->SubAuthorityCount >= KAUTH_NTSID_MAX_AUTHORITIES) {
        SMBLogInfo("Invalid domain sid?", ASL_LEVEL_DEBUG);
		return (STATUS_INVALID_SID);
	}

	sid = (ntsid_t *)malloc(sizeof(ntsid_t));
	if (sid == NULL) {
        SMBLogInfo("Couldn't allocate ntsid", ASL_LEVEL_DEBUG);
		return (STATUS_NO_MEMORY);
	}

	memset(sid, 0, sizeof(*sid));

	sid->sid_kind = DomainSid->Revision;
	sid->sid_authcount = DomainSid->SubAuthorityCount;	
	memcpy(sid->sid_authority, DomainSid->IdentifierAuthority.Value, 
		   sizeof(sid->sid_authority));
	
	for (ii = 0; ii < sid->sid_authcount; ii++)
		sid->sid_authorities[ii] = DomainSid->SubAuthority[ii];
	/* Now add the users RID */
	sid->sid_authorities[sid->sid_authcount++] = TranslatedSids.Sids->RelativeId;
	
	*ntsid = sid;
	return 0;
	
}

static 
NTSTATUS GetAccountName(WCHAR * ServerName, PRPC_UNICODE_STRING *UserName, 
						PRPC_UNICODE_STRING *DomainName, rpc_binding *binding)
{
#pragma unused(ServerName)
    NTSTATUS nt_status = STATUS_SUCCESS;
    error_status_t rpc_status = rpc_s_ok;
	
	*UserName = NULL;
	*DomainName = NULL;	
	DCETHREAD_TRY
		nt_status = LsarGetUserName(binding->get(), NULL, UserName, 
									DomainName, &rpc_status);
	DCETHREAD_CATCH_ALL(exc)
		rpc_status = rpc_exception_status(exc);
	DCETHREAD_ENDTRY
	
	
	if (rpc_status != rpc_s_ok) {
        SMBLogInfo("RPC to lsarpc gave rpc status of %#08x", ASL_LEVEL_DEBUG, rpc_status);
		/* Need a routine that converts rpc status to nt status */
		return (STATUS_UNSUCCESSFUL);
    } else if (!NT_SUCCESS(nt_status)) {		
        SMBLogInfo("RPC to lsarpc gave nt status of %#08x", ASL_LEVEL_DEBUG, nt_status);
		return nt_status;
	}
	return 0;
}

NTSTATUS GetNetworkAccountSID(const char *ServerName, char **account, char **domain, ntsid_t **ntsid)
{
	CFStringRef FullyQualifiedRef = NULL;
	PRPC_UNICODE_STRING AccountName = NULL;
	PRPC_UNICODE_STRING DomainName = NULL;
    NTSTATUS nt_status = STATUS_SUCCESS;
	WCHAR * UTF16ServerName = SMBConvertFromUTF8ToUTF16(ServerName, 1024, 0);
    rpc_ss_allocator_t  allocator;
    rpc_binding binding(make_rpc_binding(ServerName, "lsarpc"));
    rpc_mempool * mempool(rpc_mempool::allocate(0));
	
	if (!UTF16ServerName) {
		nt_status = STATUS_NO_MEMORY; 
		errno = ENOMEM;
		goto done;
	}
	/* Setup the memory allocator */
	memset(&allocator, 0, sizeof(allocator));
    allocator.p_allocate = rpc_pool_allocate;
    allocator.p_free = rpc_pool_free;
    allocator.p_context = (idl_void_p_t)mempool;
	
    rpc_ss_swap_client_alloc_free_ex(&allocator, &allocator);
	
	nt_status = GetAccountName(UTF16ServerName, &AccountName, &DomainName, &binding);
	/* Some servers will return success, but they won't return the user name.  */
	if (!AccountName && (NT_SUCCESS(nt_status))) {
        SMBLogInfo("Server return a NULL account name", ASL_LEVEL_DEBUG);
		nt_status = STATUS_NO_SUCH_USER; 
	}
	if (!NT_SUCCESS(nt_status)) {
        SMBLogInfo("Couldn't get the account name: %d", ASL_LEVEL_DEBUG, nt_status);
		errno = ENOENT;
		goto done;
	}
	FullyQualifiedRef = CreateFullyQualifiedAccountName(AccountName, DomainName);
	if (FullyQualifiedRef) {
		RPC_UNICODE_STRING FullyQualified;
		
		/*
		 * CFStringGetLength returns the number of 16-bit Unicode characters in 
		 * the string. This means CFStringGetLength returns half the bytes in the
		 * buffer.
		 */
		FullyQualified.MaximumLength = CFStringGetLength(FullyQualifiedRef) * sizeof(UniChar);
		FullyQualified.Length = FullyQualified.MaximumLength;
		/* Remember that CFStringGetCharactersPtr can fail */
		FullyQualified.Buffer = (WCHAR *)CFStringGetCharactersPtr(FullyQualifiedRef);
		if (FullyQualified.Buffer) {
			nt_status = GetAccountNameSID(UTF16ServerName, &FullyQualified, ntsid, &binding);
		} else {
			nt_status = STATUS_NO_MEMORY; 
		}
		CFRelease(FullyQualifiedRef);
	} else {
		nt_status = STATUS_UNSUCCESSFUL;
	}
	/* The fully qualified account name failed, try just the account name */
	if (!NT_SUCCESS(nt_status)) {
        SMBLogInfo("Failed to get the sid using the fully qualified account name", ASL_LEVEL_DEBUG);
		nt_status = GetAccountNameSID(UTF16ServerName, AccountName, ntsid, &binding);
	}
	if (!NT_SUCCESS(nt_status)) {
        SMBLogInfo("Couldn't get the account sid: %d", ASL_LEVEL_DEBUG, nt_status);
		errno = ENOENT;
		goto done;
	}
	if (account) {
		*account = NULL;
		/* 
		 * They want us to return the account name, not sure all the testing 
		 * is need, but for now lets keep testing.
		 */
		if (AccountName && AccountName->Length && AccountName->Buffer && ((char *)AccountName->Buffer != (char *)AccountName)) {
			*account = SMBConvertFromUTF16ToUTF8((const uint16_t *)AccountName->Buffer, AccountName->Length, 0);
		}
	}
	if (domain) {
		*domain = NULL;
		/* 
		 * They want us to return the account name, not sure all the testing 
		 * is need, but for now lets keep testing.
		 */
		if (DomainName && DomainName->Length && DomainName->Buffer && ((char *)DomainName->Buffer != (char *)DomainName)) {
			*domain = SMBConvertFromUTF16ToUTF8((const uint16_t *)DomainName->Buffer, DomainName->Length, 0);
		}
	}
	
done:
	/* Need to free the memory allocator here */
	if (UTF16ServerName) {
		free(UTF16ServerName);
	}
	rpc_mempool::destroy(mempool);
	return nt_status;
}
