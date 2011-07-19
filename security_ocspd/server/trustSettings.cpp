/*
 * Copyright (c) 2004-2011 Apple Inc. All Rights Reserved.
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

/*
 * trustSettings.cpp - routines called by MIG for Trust Settings r/w
 *
 * Created 9 May 2006 by dmitch
 */

#include <security_ocspd/ocspd.h>	/* created by MIG */
#include <Security/Authorization.h>
#include <Security/AuthorizationDB.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <bsm/libbsm.h>				/* for audit_token_to_au32() */
#include <security_ocspd/ocspdDebug.h>
#include <Security/SecBase.h>
#include <Security/SecTrustSettings.h>
#include <Security/TrustSettingsSchema.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "ocspdServer.h"
#include <membership.h>				/* for mbr_uid_to_uuid() */

/* One lock for all accesses to TrustSettings files */
static ModuleNexus<Mutex> gTrustSettingsLock;

/* 
 * The auth rights we use.
 * This probably will get tweaked: what I'd like to have, I think, for 
 * the admin settings is "OK if root, else authenticate as admin". For
 * now just require root. 
 */
#define TRUST_SETTINGS_RIGHT_USER	"com.apple.trust-settings.user"
#define TRUST_SETTINGS_RIGHT_ADMIN	"com.apple.trust-settings.admin"

#define TRUST_SETTINGS_RULE_USER	CFSTR("authenticate-session-owner");
#define TRUST_SETTINGS_RULE_ADMIN	CFSTR("is-root");

/* 
 * Everyone can look in the settings directory, and hence stat the admin 
 * file (as a quick optimization to avoid an RPC to us if it's not there), 
 * but the individual files are readable only by root.
 */
#define TRUST_SETTINGS_PATH_MODE	0755
#define TRUST_SETTINGS_FILES_MODE	0600

/* 
 * Create a stringified UUID. Caller allocs result, length UUID_STR_LEN + 1. 
 * String has two characters for each UUID byte, plus 4 '-' chars, plus NULL. 
 */
#define UUID_STR_LEN	((2 * sizeof(uuid_t)) + 5)

static void uuidString(
	uuid_t uuid,
	char *cp)
{
	unsigned dex;
	unsigned char *uuidCp = (unsigned char *)uuid;

	for(dex=0; dex<sizeof(uuid_t); dex++) {
		sprintf(cp, "%02X", *uuidCp++);
		cp += 2;
		switch(dex) {
			case 3:
			case 5:
			case 7:
			case 9:
				*cp++ = '-';
				break;
			default:
				break;
		}
	}
	*cp = '\0';
}

/* 
 * Given the audit_token from an incoming message, cook up the path to the 
 * appropriate settings file. 
 * 
 * We use UUID here to form the file name, not uid, since uids can be reused. 
 * (User Joe, uid 600, has some settings, then quits to company. User Mary joins, 
 * is assigned to the currently unused uid 600. User Mary unwittingly inherits 
 * Joe's Trust Settings. Bad.)
 * 
 * I have no idea under which circumstances the mbr_uid_to_uuid() function could 
 * fail, but we fall back to uid if it does.
 * 
 * The UUID-based filename is of the same form as UUID are generally 
 * presented: FA36D0A2-D91A-4E36-8156-1A22E8982652.plist, 
 */
static void trustSettingsPath(
	audit_token_t auditToken,
	SecTrustSettingsDomain domain,
	char *path)				/* caller allocated, (MAXPATHLEN + 1) bytes */
{
	switch(domain) {
		case kSecTrustSettingsDomainUser:
		{
			uid_t euid;
			uuid_t uuid;

			/* uid from the incoming message's audit_token */
			audit_token_to_au32(auditToken, NULL, &euid, NULL, NULL, NULL, NULL, NULL, NULL);
			/* convert that to uuid */
			if(!mbr_uid_to_uuid(euid, uuid)) {
				char uuidStr[UUID_STR_LEN];
				uuidString(uuid, uuidStr);
				snprintf(path, MAXPATHLEN + 1, "%s/%s.plist", TRUST_SETTINGS_PATH, uuidStr);
			}
			else {
				/* can't get UUID - use the uid */
				snprintf(path, MAXPATHLEN + 1, "%s/%lu.plist", TRUST_SETTINGS_PATH, 
					(unsigned long)euid);
			}
			break;
		}
		case kSecTrustSettingsDomainAdmin:
			snprintf(path, MAXPATHLEN + 1, "%s/%s", TRUST_SETTINGS_PATH, ADMIN_TRUST_SETTINGS);
			break;
		case kSecTrustSettingsDomainSystem:
			/* 
			 * The client really should just read this file themselves since it's
			 * immutable. But, we'll do it if asked. 
			 */
			strcpy(path, SYSTEM_TRUST_SETTINGS_PATH);
			break;
		default:
			break;
	}
}

static mach_port_t	gBootstrapPort=MACH_PORT_NULL;
static mach_port_t	gAuditSessionPort=MACH_PORT_NULL;

void switchToContext(mach_port_t clientBootstrapPort, audit_token_t auditToken)
{
	au_asid_t 		asid, tmp_asid;
	mach_port_t		clientAuditSessionPort=MACH_PORT_NULL;
	kern_return_t	kr;

	/* get the audit session identifier and port from the audit_token */
	audit_token_to_au32(auditToken, NULL, NULL, NULL, NULL, NULL, NULL, &asid, NULL);
	audit_session_port(asid, &clientAuditSessionPort);

	if (gBootstrapPort == MACH_PORT_NULL) {
		/* save our own bootstrap port the first time through (to restore later) */
		task_get_bootstrap_port(mach_task_self(), &gBootstrapPort);
	}
	kr = task_set_bootstrap_port(mach_task_self(), clientBootstrapPort);
	if (kr != KERN_SUCCESS)
		ocspdErrorLog("Unable to set client bootstrap port\n");

	if (gAuditSessionPort == MACH_PORT_NULL) {
		/* save our own audit session port the first time through (to restore later) */
		gAuditSessionPort = audit_session_self();
		mach_port_mod_refs(mach_task_self(), gAuditSessionPort, MACH_PORT_RIGHT_SEND, +1);
	}
	tmp_asid = audit_session_join(clientAuditSessionPort);
	if (tmp_asid == AU_DEFAUDITSID)
		ocspdErrorLog("Unable to join client security session\n");
}

void restoreContext()
{
	if (gBootstrapPort != MACH_PORT_NULL)
	{
		kern_return_t kr = task_set_bootstrap_port(mach_task_self(), gBootstrapPort);
		if (kr != KERN_SUCCESS)
			ocspdErrorLog("Unable to restore server bootstrap port");
	}

	if (gAuditSessionPort != MACH_PORT_NULL)
	{
		au_asid_t asid = audit_session_join(gAuditSessionPort);
		if (asid == AU_DEFAUDITSID)
			ocspdErrorLog("Unable to rejoin original security session");
	}
}

kern_return_t ocsp_server_trustSettingsRead(
	mach_port_t serverport,
	audit_token_t auditToken,
	uint32_t domain,
	Data *trustSettings,
	mach_msg_type_number_t *trustSettingsCnt,
	OSStatus *rcode)
{
	StLock<Mutex> _(gTrustSettingsLock());
	char path[MAXPATHLEN + 1];

	trustSettingsPath(auditToken, domain, path);
	unsigned char *fileData = NULL;
	unsigned fileDataLen;
	if(readFile(path, &fileData, &fileDataLen)) {
		ocspdTrustDebug("trustSettingsRead: no file at %s", path);
		*rcode = errSecNoTrustSettings;
		*trustSettings = NULL;
		*trustSettingsCnt = 0;
		return 0;
	}

	/* realloc using our server's allocator for later free */
	CSSM_DATA cdata;
	Allocator &alloc = OcspdServer::active().alloc();
	cdata.Data = (uint8 *)alloc.malloc(fileDataLen);
	cdata.Length = fileDataLen;
	memmove(cdata.Data, fileData, fileDataLen);
	free(fileData);
	passDataToCaller(cdata, trustSettings, trustSettingsCnt);
	ocspdTrustDebug("trustSettingsRead: read %lu bytes from %s",
			(unsigned long)cdata.Length, path);
	*rcode = noErr;
	return 0;
}

kern_return_t ocsp_server_trustSettingsWrite(
	mach_port_t serverport,
	audit_token_t auditToken,
	mach_port_t clientport,
	uint32_t domain,
	Data authBlob,
	mach_msg_type_number_t authBlobCnt,
	Data trustSettings,
	mach_msg_type_number_t trustSettingsCnt,
	OSStatus *rcode)
{
	StLock<Mutex> _(gTrustSettingsLock());

	const char *authRight = NULL;
	CFStringRef authRule = NULL;
	char path[MAXPATHLEN + 1];

	trustSettingsPath(auditToken, domain, path);

	switch(domain) {
		case kSecTrustSettingsDomainUser:
			authRight = TRUST_SETTINGS_RIGHT_USER;
			authRule  = TRUST_SETTINGS_RULE_USER;
			break;
		case kSecTrustSettingsDomainAdmin:
			authRight = TRUST_SETTINGS_RIGHT_ADMIN;
			authRule  = TRUST_SETTINGS_RULE_ADMIN;
			break;
		case kSecTrustSettingsDomainSystem:
			/* this TrustSetting is immutable */
			*rcode = errSecDataNotModifiable;
			return 0;
	}

	AuthorizationExternalForm extForm;
	if(authBlobCnt > sizeof(extForm)) {
		/* not sure how this could legitimately happen.... */
		ocspdErrorLog("trustSettingsWrite: authBlob too big\n");
		*rcode = paramErr;
		return 0;
	}

	/*
	 * Lazily create auth rights we (and we alone) use 
	 */
	AuthorizationRef authRef;
	OSStatus ortn;
	if(AuthorizationRightGet(authRight, NULL)) {
		ortn = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, 
					0, &authRef);
		if(ortn) {
			/* should never happen */
			ocspdErrorLog("trustSettingsWrite: AuthorizationCreate failure\n");
			*rcode = internalComponentErr;
			return 0;
		}
		ortn = AuthorizationRightSet(authRef, authRight, authRule,
			NULL, NULL, NULL);
		if(ortn) {
			ocspdErrorLog("trustSettingsWrite: AuthorizationRightSet failure\n");
			*rcode = internalComponentErr;
			return 0;
		}
		AuthorizationFree(authRef, 0);
	}

	/* 
 	 * Cook up an auth object from the client's blob 
	 */
	memmove(&extForm, authBlob, authBlobCnt);
	ortn = AuthorizationCreateFromExternalForm(&extForm, &authRef);
	if(ortn) {
		ocspdErrorLog("trustSettingsWrite: AuthorizationCreateFromExternalForm failure\n");
		*rcode = paramErr;
		return 0;
	}
	
	/* now, see if we're authorized to do this thing */
	AuthorizationItem authItem     = {authRight, 0, NULL, 0};
	AuthorizationRights authRights = { 1, &authItem };
	AuthorizationFlags authFlags   = kAuthorizationFlagInteractionAllowed | 
								     kAuthorizationFlagExtendRights;
	/* save and restore context around call which can put up UI */
	switchToContext(clientport, auditToken);
	ortn = AuthorizationCopyRights(authRef, &authRights, NULL,
		authFlags, NULL);
	restoreContext();
	if(ortn) {
		ocspdErrorLog("trustSettingsWrite: AuthorizationCopyRights failure\n");		
	}
	/* fixme - destroy rights? Really? */
	AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
	if(ortn) {
		*rcode = ortn;
		return 0;
	}

	/* 
 	 * Looks like we're good to go.
	 * First, handle easy case of deleting a Trust Settings file (indicated
	 * by an empty trustSettings).
	 */
	if(trustSettingsCnt == 0) {
		ocspdTrustDebug("trustSettingsWrite: DELETING %s", path);
		if(unlink(path)) {
			/* FIXME maybe we should log this to the console */
			*rcode = errno;	
			ocspdErrorLog("trustSettingsWrite: unlink error %d\n", errno);	
		}
		else {
			*rcode = noErr;
		}
		return 0;
	}

	/*
	 * Create TRUST_SETTINGS_PATH if necessary.
	 */
	struct stat sb;
	if(stat(TRUST_SETTINGS_PATH, &sb)) {
		ocspdTrustDebug("trustSettingsWrite: creating %s", TRUST_SETTINGS_PATH);
		if(mkdir(TRUST_SETTINGS_PATH, TRUST_SETTINGS_PATH_MODE)) {
			ocspdErrorLog("trustSettingsWrite: mkdir() returned %d\n", errno);
			*rcode = internalComponentErr;
			return 0;
		}

		/* override the probable umask that made this directory unreadable by others */
		chmod(TRUST_SETTINGS_PATH, TRUST_SETTINGS_PATH_MODE);
	}

	/* And, finally.... */
	if(writeFile(path, (const unsigned char *)trustSettings, trustSettingsCnt)) {
		ocspdErrorLog("trustSettingsWrite: writeFile() error\n");
		*rcode = internalComponentErr;
	}
	else {
		ocspdTrustDebug("trustSettingsWrite: wrote %lu bytes to %s",
			(unsigned long)trustSettingsCnt, path);
		chmod(path, TRUST_SETTINGS_FILES_MODE);
		*rcode = noErr;
	}
	return 0;
}
