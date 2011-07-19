/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
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

#include "imap-common.h"
#include "imap-id.h"

#include "str.h"
#include "home-expand.h"
#include "mail-user.h"
#include "imap-quote.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

#define	APS_VERSION		"1"

typedef struct msg_data_s {
	unsigned long msg;
	unsigned long pid;

	char d1[128];
	char d2[512];
	char d3[512];
	char d4[512];
} msg_data_t;

/*
   tag1 XAPPLEPUSHSERVICE "aps-version" "1"
	  "aps-account-id" "E8CD34AD-98D3-4489-A6BB-86B1D082FECE"
	  "aps-device-token" "a66216ad1683d48b9933cdcc3b98a833ee1a968143f41ea494187da54715da66"
	  "aps-subtopic" "com.apple.mobilemail"
*/

static void do_notify(const char *username, const char *aps_acct_id,
		      const char *aps_dev_token, const char *aps_sub_topic)
{
	const char *push_notify_path = "/var/dovecot/push_notify";

	msg_data_t msg_data;
	memset(&msg_data, 0, sizeof(struct msg_data_s));
	msg_data.msg = 2;

	strlcpy(msg_data.d1, username, sizeof(msg_data.d1));
	strlcpy(msg_data.d2, aps_acct_id, sizeof(msg_data.d2));
	strlcpy(msg_data.d3, aps_dev_token, sizeof(msg_data.d3));
	strlcpy(msg_data.d4, aps_sub_topic, sizeof(msg_data.d4));

	int soc = socket( AF_UNIX, SOCK_DGRAM, 0 );
	if ( soc < 0 ) {
		i_warning( "open notify socket failed(%d): %m", soc );
		return;
	}

	struct sockaddr_un sock_addr;
	memset( &sock_addr, 0, sizeof(struct sockaddr_un));
	sock_addr.sun_family = AF_UNIX;
	strlcpy( sock_addr.sun_path, push_notify_path, sizeof(sock_addr.sun_path) );
	socklen_t sock_len = sizeof(sock_addr.sun_family) + strlen(sock_addr.sun_path) + 1;
	int rc = connect(soc, (struct sockaddr *) &sock_addr, sock_len);
	if ( rc < 0 ) {
		i_warning("connect to notify socket %s failed: %m",
			  push_notify_path);
		close(soc);
		return;
	}

	rc = send(soc, (void *)&msg_data, sizeof(msg_data), 0);
	if ( rc < 0 )
		i_warning("send to notify socket %s failed: %m",
			  push_notify_path);

	close(soc);
}

static const char *aps_reply_generate (struct client_command_context *cmd,
				       const struct imap_arg *args)
{
	const char *aps_topic = cmd->client->set->aps_topic;
	const char *aps_ver=NULL;
	const char *aps_acct_id=NULL;
	const char *aps_dev_token=NULL;
	const char *aps_sub_topic=NULL;
	const char *key, *value;

	/* must have a topic */
	if (aps_topic == NULL || *aps_topic == '\0')
		return NULL;

	/* scarf off the aps keys/values */
	while (imap_arg_get_astring(&args[0], &key) &&
	       imap_arg_get_astring(&args[1], &value)) {
		if (strcasecmp(key, "aps-version") == 0)
			aps_ver = t_strdup(value);
		else if (strcasecmp(key, "aps-account-id") == 0)
			aps_acct_id = t_strdup(value);
		else if (strcasecmp(key, "aps-device-token") == 0)
			aps_dev_token = t_strdup(value);
		else if (strcasecmp(key, "aps-subtopic") == 0)
			aps_sub_topic = t_strdup(value);
		else 
			return NULL;
		args += 2;
	}

	/* save notification settings */
	if ( aps_ver && aps_acct_id && aps_dev_token && aps_sub_topic ) {
		/* subscribe to notification node */
		do_notify(cmd->client->user->username, aps_acct_id,
			  aps_dev_token, aps_sub_topic);

		/* generate aps response */
		string_t *str = t_str_new(256);
		imap_dquote_append( str, "aps-version" );
		str_append_c(str, ' ');
		imap_dquote_append( str, APS_VERSION );
		str_append_c(str, ' ');
		imap_dquote_append( str, "aps-topic" );
		str_append_c(str, ' ');
		imap_dquote_append( str, aps_topic );
		return str_c(str);
	}
	return NULL;
}

bool cmd_x_apple_push_service(struct client_command_context *cmd)
{
	const struct imap_arg *args;

	if (!client_read_args(cmd, 0, 0, &args))
		return FALSE;

	const char *reply = aps_reply_generate(cmd, args);
	if (reply != NULL)
		client_send_line(cmd->client,
				 t_strdup_printf("* XAPPLEPUSHSERVICE %s",
						 reply));
	client_send_tagline(cmd, "OK XAPPLEPUSHSERVICE completed.");

	return TRUE;
}
