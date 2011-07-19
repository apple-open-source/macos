/*
 * Contains:   Routines to support Apple Push Notification service.
 * Written by: Michael Dasenbrock (for addtl writers check SVN comments).
 * Copyright:  © 2008-2011 Apple, Inc., all rights reserved.
 * Note:       When editing this file set Xcode to "Editor uses tabs/width=4".
 *             Any other editor or tab setting may not show this file nicely!
 */

#ifndef __push_notify_h__
#define __push_notify_h__
#endif

#define	BUFF_SIZE			8192
#define MAX_BUF_SIZE		5120000
#define	GUID_BUFF_SIZE		64
#define	AUTH_INFO_PATH		"/etc/dovecot/notify/notify.plist"

typedef struct msg_data_s {
	unsigned long msg;
	unsigned long pid;

	char d1[128];
	char d2[512];
	char d3[512];
	char d4[512];
} msg_data_t;

typedef struct server_info_s {
	int			port;
	char		*address;
	char		*username;
	char		*passwd;
	CFStringRef	cf_str_addr;
	CFStringRef	cf_str_pubsub;
	CFStringRef	cf_str_apn;
	CFStringRef	cf_str_user;
	CFStringRef	cf_str_pwd;
} server_info_t;

int		get_server_info			( server_info_t *in_out_info );
int		error_callback			( XMLNodeRef in_err_node, void *in_user_info );
void	session_callback		( XMPPSessionRef in_sess_ref, XMPPSessionEvent in_event );
void	create_node				( const char * );
void	publish					( msg_data_t *in_msg );
int		publish_callback		( void *in_user_info );
void	start_xmpp_session		();
void	reschedule_xmpp_connect	();
