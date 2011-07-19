/*
 * Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

#ifndef DB_OD_H
#define DB_OD_H

#include <uuid/uuid.h>

#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <DirectoryService/DirServicesConst.h>

#define OD_USER_CACHE_KEY	"%u"
#define OD_DEFAULT_USERNAME_FORMAT "%u"
#define OD_DEFAULT_PASS_SCHEME "CRYPT"

#define	kCRAM_MD5_AuthSuccess			"k-cram-md5-auth-success"
#define	kCRAM_MD5_AuthFailed			"k-cram-md5-auth-failed"
#define	kCRAM_APOP_AuthSuccess			"k-apop-auth-success"
#define	kCRAM_APOP_AuthFailed			"k-apop-auth-failed"

/* Mail user attribute version */
#define	kXMLKeyAttrVersion				"kAttributeVersion"
	#define	kXMLValueVersion				"Apple Mail 1.0"
	#define	kXMLValueVersion2				"Apple Mail 2.0"

/* Account state */
#define	kXMLKeyAcctState				"kMailAccountState"
	#define	kXMLValueAcctEnabled			"Enabled"
	#define	kXMLValueAcctDisabled			"Off"
	#define	kXMLValueAcctFwd				"Forward"

/* Auto forward key (has no specific value) */
#define	kXMLKeyAutoFwd					"kAutoForwardValue"

/* Migration key/values */
#define	kXMLKeyMigrationFlag			"kMigrationFlag"
	#define	kXMLValueAcctMigrated			"AcctMigrated"
	#define	kXMLValueAcctNotMigrated		"AcctNotMigrated"

/* IMAP login state */
#define	kXMLKeykIMAPLoginState			"kIMAPLoginState"
	#define	kXMLValueIMAPLoginOK			"IMAPAllowed"
	#define	kXMLValueIMAPLogInNotOK			"IMAPDeny"

/* POP3 login state */
#define	kXMLKeyPOP3LoginState			"kPOP3LoginState"
	#define	kXMLValuePOP3LoginOK			"POP3Allowed"
	#define	kXMLValuePOP3LoginNotOK			"POP3Deny"

/* Account location key (has no specific value) */
#define	kXMLKeyAcctLoc					"kMailAccountLocation"

/* Account location key (has no specific value) */
#define	kXMLKeyAltDataStoreLoc			"kAltMailStoreLoc"

/* Disk Quota  (has no specific value) */
#define	kXMLKeyDiskQuota				"kUserDiskQuota"

/* Push Notification enabled key */
#define	kPushNotifyEnabled				"notification_server_enabled"


extern int	od_pos_cache_ttl;
extern int	od_neg_cache_ttl;
extern bool	od_use_getpwnam_ext;

typedef enum {
	unknown_state			= 0x00000000,
	unknown_user			= 0x00000001,
	account_enabled			= 0x00000002,
	imap_enabled			= 0x00000004,
	pop_enabled				= 0x00000008,
	auto_fwd_enabled		= 0x00000010,
	sacl_enabled			= 0x00000020,
	sacl_not_member			= 0x00000040,
	acct_migrated			= 0x00000080
} od_acct_state;

typedef enum {
	OD_AUTH_SUCCESS,
	OD_AUTH_FAILURE
} od_auth_event_t;

struct od_user {
	int				refcount;
	time_t			create_time;
	uid_t			user_uid;
	gid_t			user_gid;
	od_acct_state	acct_state;
	int				mail_quota;
	char		   *user_guid;
	char		   *record_name;
	char		   *acct_loc;
	char		   *alt_data_loc;
};

struct db_od {
	int					refcount;
	ODSessionRef		od_session_ref;
	ODNodeRef			od_node_ref;
	unsigned int		userdb:1;
	int					pos_cache_ttl;
	int					neg_cache_ttl;
	bool				use_getpwnam_ext;
	struct hash_table  *users_table;
};

typedef struct msg_data_s {
	unsigned long msg;
	unsigned long pid;

	char d1[128];
	char d2[512];
	char d3[512];
	char d4[512];
} msg_data_t;

void			send_server_event	( struct auth_request *in_request, const od_auth_event_t in_event_code, const char *in_user, const char *in_addr );
void			close_server_event_port ( void );
struct od_user *db_od_user_lookup	( struct auth_request *in_request, struct db_od *in_db, const char *in_user_name, bool in_is_auth );
struct db_od   *db_od_init			( bool in_userdb );
void			db_od_user_unref	( struct od_user **in_user_info );
void			db_od_do_init		( struct db_od *in_od_info );
void			db_od_unref			( struct db_od **in_od_info_p );
void			db_od_sacl_check	( struct auth_request *in_request, struct od_user *in_od_user, const char *in_group );
const char	   *db_od_get_ms_path	( struct auth_request *in_request, const char *in_tag, const char *in_partition_map );
void			db_od_print_cf_error( struct auth_request *in_request, CFErrorRef in_cf_err_ref, const char *in_default_str );
bool			is_acct_enabled		( struct auth_request *in_request, od_acct_state in_acct_state, const char *in_user );
void			push_notify_init	( struct od_user *in_user_info );

#endif
