/*
 * opendirectory.h
 *
 * Copyright (C) 2003-2007 Apple Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>

#include <DirectoryService/DirServices.h>

/* tdb.h #defines this, which collides with OSByteOrder.h in PPC builds. */
#undef u32

#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesUtils.h>

/* Definitions */
#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_DS_BUFFER_SIZE (1024 * 10)
#define SMALL_DS_BUFFER_SIZE (512)

#define DS_CLOSE_NODE(noderef) \
    do { \
	if ((noderef) != 0) { dsCloseDirNode(noderef); } \
	(noderef) = 0; \
    } while (0);

#define DS_DEFAULT_BUFFER(dref) \
	dsDataBufferAllocate((dref), DEFAULT_DS_BUFFER_SIZE)

/* Directory Services node references are not good across fork(2). We need to
 * associate the reference with the process it was created in so we can figure
 * out whether we might need to recreate it.
 */
struct opendirectory_session
{
	tDirReference   	ref; /* Service reference. */
	tDirNodeReference	search; /* Search node reference. */
	pid_t		    	pid; /* PID of owning process. */

	const char **		local_path_cache;
	CFMutableDictionaryRef  domain_sid_cache;
};

/* Session management APIs. */

tDirStatus opendirectory_connect(struct opendirectory_session *session);
tDirStatus opendirectory_reconnect(struct opendirectory_session *session);
void opendirectory_disconnect(struct opendirectory_session *session);
tDirStatus opendirectory_searchnode(struct opendirectory_session *session);

tDirStatus opendirectory_open_node(struct opendirectory_session *session,
                        const char* nodeName,
                        tDirNodeReference *nodeReference);

/* Memory management APIs. */

void opendirectory_free_list(struct opendirectory_session *session,
			    tDataListPtr list);
void opendirectory_free_node(struct opendirectory_session *session,
			    tDataNodePtr node);
void opendirectory_free_buffer(struct opendirectory_session *session,
			    tDataBufferPtr buffer);

/* Search APIs. */

tDirStatus opendirectory_sam_searchattr(
				struct opendirectory_session *session,
                            	CFMutableArrayRef *records,
				const char *type,
				const char *attr,
				const char *value);
tDirStatus opendirectory_sam_searchname(
				struct opendirectory_session *session,
                            	CFMutableArrayRef *records,
				const char *type,
				const char *name);

tDirStatus opendirectory_insert_search_results(tDirNodeReference node,
				    CFMutableArrayRef recordsArray,
				    const unsigned long recordCount,
				    tDataBufferPtr dataBuffer);

/* High-level SAM policy APIs. */

BOOL opendirectory_find_usersid_from_record(
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *sid);

BOOL opendirectory_find_groupsid_from_record(
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *sid);

CFDictionaryRef opendirectory_find_record_from_usersid(
				struct opendirectory_session *session,
				const DOM_SID *user_sid);

CFDictionaryRef opendirectory_find_record_from_groupsid(
				struct opendirectory_session *session,
				const DOM_SID *group_sid);

/* Miscellaneous helper APIs. */

tDataListPtr opendirectory_sam_attrlist(struct opendirectory_session *session);
const char ** opendirectory_local_paths(struct opendirectory_session *session);

char *opendirectory_get_record_attribute(void *talloc_ctx,
				    CFDictionaryRef record,
				    const char *attribute);
char * opendirectory_talloc_cfstr(void *talloc_ctx, CFStringRef cfstring);

/* Authentication APIs. */

tDirStatus opendirectory_authenticate_node(
				struct opendirectory_session *session,
				tDirNodeReference nodeRef);

tDirStatus opendirectory_user_auth_and_session_key(
				struct opendirectory_session *session,
				tDirNodeReference inUserNodeRef,
				const char *account_name,
				u_int8_t *challenge,
				u_int8_t *client_response,
				u_int8_t *session_key,
				u_int32_t *key_length);

tDirStatus opendirectory_user_session_key(
			struct opendirectory_session *session,
			tDirNodeReference inUserNodeRef,
			const char *account_name,
			u_int8_t *session_key);

tDirStatus opendirectory_ntlmv2user_session_key(const char *account_name,
				u_int32_t ntv2response_len,
				u_int8_t* ntv2response,
				const char* domain,
				u_int32_t *session_key_len,
				u_int8_t *session_key);

tDirStatus opendirectory_cred_session_key(const DOM_CHAL *client_challenge,
				const DOM_CHAL *server_challenge,
				const char *machine_acct,
				char *session_key);

tDirStatus opendirectory_set_workstation_nthash(const char *account_name,
				const char *nt_hash);

tDirStatus opendirectory_lmchap2changepasswd(const char *account_name,
				const char *passwordData,
				const char *passwordHash,
				u_int8_t passwordFormat,
				const char *slot_id);

#ifndef kDS1AttrSMBRID
#define kDS1AttrSMBRID			"dsAttrTypeNative:smb_rid"
#define kDS1AttrSMBGroupRID		"dsAttrTypeNative:smb_group_rid"
#define kDS1AttrSMBPWDLastSet		"dsAttrTypeNative:smb_pwd_last_set"
#define kDS1AttrSMBLogonTime		"dsAttrTypeNative:smb_logon_time"
#define kDS1AttrSMBLogoffTime		"dsAttrTypeNative:smb_logoff_time"
#define kDS1AttrSMBKickoffTime		"dsAttrTypeNative:smb_kickoff_time"
#define kDS1AttrSMBHomeDrive		"dsAttrTypeNative:smb_home_drive"
#define kDS1AttrSMBHome			"dsAttrTypeNative:smb_home"
#define kDS1AttrSMBScriptPath		"dsAttrTypeNative:smb_script_path"
#define kDS1AttrSMBProfilePath		"dsAttrTypeNative:smb_profile_path"
#define kDS1AttrSMBUserWorkstations	"dsAttrTypeNative:smb_user_workstations"
#define kDS1AttrSMBAcctFlags		"dsAttrTypeNative:smb_acctFlags"
#endif

#define kDS1AttrSMBLMPassword		"dsAttrTypeNative:smb_lmPassword"
#define kDS1AttrSMBNTPassword		"dsAttrTypeNative:smb_ntPassword"
#define kDS1AttrSMBPWDCanChange   	"dsAttrTypeNative:smb_pwd_can_change"
#define kDS1AttrSMBPWDMustChange	"dsAttrTypeNative:smb_pwd_must_change"

#define kPlainTextPassword		"plaintextpassword"

/* Password Policy Attributes */
#define kPWSIsDisabled			"isDisabled"
#define kPWSIsAdmin			"isAdminUser"
#define kPWSNewPasswordRequired		"newPasswordRequired"
#define kPWSIsUsingHistory		"usingHistory"
#define kPWSCanChangePassword		"canModifyPasswordforSelf"
#define kPWSExpiryDateEnabled		"usingHardExpirationDate"
#define kPWSRequiresAlpha		"requiresAlpha"
#define kPWSExpiryDate			"expirationDateGMT"
#define kPWSHardExpiryDate		"hardExpireDateGMT"
#define kPWSMaxMinChgPwd		"maxMinutesUntilChangePassword"
#define kPWSMaxMinActive		"maxMinutesUntilDisabled"
#define kPWSMaxMinInactive		"maxMinutesOfNonUse"
#define kPWSMaxFailedLogins		"maxFailedLoginAttempts"
#define kPWSMinChars			"minChars"
#define kPWSMaxChars			"maxChars"
#define kPWSPWDCannotBeName		"passwordCannotBeName"

#define kPWSPWDLastSetTime		"passwordLastSetTime"
#define kPWSLastLoginTime		"lastLoginTime"
#define kPWSLogOffTime			"logOffTime"
#define kPWSKickOffTime			"kickOffTime"

enum ds_trace_level
{
	DS_TRACE_ALL,
	DS_TRACE_ERRORS
};

/* Squash a ds_trace_level to a DEBUG status, depending on whether the
 * API result was an error or not.
 */
#define LOG_DS_ERRLEVEL(which, status) \
	( ((status) == eDSNoErr && (which) != DS_TRACE_ALL) ? 10 : 0 )

/* We should never get this because we carefully obtain a new reference
 * across fork(2). If this does happen, we need to know where.
 */
#define LOG_DS_CHECK_BADREF(status) \
	{ if ((status) == eDSInvalidReference) { log_stack_trace(); }}

/* Log the results on an API call. */
#define LOG_DS_ERROR(which, status, funcname) \
	do { \
		char * dserr = dsCopyDirStatusName(status); \
		int level = LOG_DS_ERRLEVEL(which, status); \
		DEBUG(level, ("%s gave %ld [%s]\n", \
			funcname, (long)status, dserr ? dserr : "??")); \
		SAFE_FREE(dserr); \
		LOG_DS_CHECK_BADREF(status); \
	} while(0)

/* Log the results of an API call, and append an extra message. */
#define LOG_DS_ERROR_MSG(which, status, funcname, msgfmt) \
	do { \
		char * dserr = dsCopyDirStatusName(status); \
		int level = LOG_DS_ERRLEVEL(which, status); \
           	DEBUG(level, ("%s gave %ld [%s]: ", \
	       		funcname, (long)status, dserr ? dserr : "??")); \
           	DEBUGADD(level, msgfmt); \
           	SAFE_FREE(dserr); \
		LOG_DS_CHECK_BADREF(status); \
    	} while(0)

#ifdef __cplusplus
}
#endif
