/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <GSS/gssapi.h>
#include <GSS/gssapi_krb5.h>
#include <GSS/gssapi_ntlm.h>
#include <GSS/gssapi_spnego.h>
#include <Kernel/gssd/gssd_mach_types.h>
#include <sys/queue.h>

struct smb_gss_cred_list_entry {
	TAILQ_ENTRY(smb_gss_cred_list_entry) next;
	char *principal;
	uint32_t expire;
	gss_OID mech;
};

TAILQ_HEAD(smb_gss_cred_list, smb_gss_cred_list_entry);

struct smb_gss_cred_ctx {
	dispatch_semaphore_t sem;
	uint32_t maj;
	gss_cred_id_t creds;
};

int smb_gss_get_cred_list(struct smb_gss_cred_list **, const gss_OID /*mech*/);
void smb_gss_free_cred_entry(struct smb_gss_cred_list_entry **);
void smb_gss_free_cred_list(struct smb_gss_cred_list **);
int smb_gss_match_cred_entry(struct smb_gss_cred_list_entry *, const gss_OID /*mech*/,
			const char * /*name*/, const char * /*domain*/);

char *smb_gss_principal_from_cred(void *);
void smb_release_gss_cred(void *, int);
int smb_acquire_ntlm_cred(const char *, const char *, const char *, void **);
int smb_acquire_krb5_cred(const char *, const char *, const char *, void **);
CFStringRef TargetNameCreatedWithHostName(struct smb_ctx *ctx);
void GetTargetNameUsingHostName(struct smb_ctx *);
int serverSupportsKerberos(CFDictionaryRef);
