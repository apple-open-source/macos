/* Copyright (c) 2008-2011 Apple, inc. */

#include "lib.h"
#include "mail-deliver.h"
#include "mail-namespace.h"
#include "message-address.h"
#include "push-notify-plugin.h"

#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/unistd.h>

const char *push_notify_plugin_version = DOVECOT_VERSION;

static deliver_hook_func_t *next_deliver_mail;
struct et_list *_et_list = NULL;

struct message_info {
	const char	*from;
	const char	*subj;
};

// -----------------------------------------------------------------
//	push_notification ()

static void
push_notification(struct mail_deliver_context *ctx, const char *mailbox)
{
	int					debug			= 0;
	int					notify_sock		= 0;
	const char		   *sock_path		= "/var/dovecot/push_notify";
	ssize_t				rc				= 0;
	socklen_t			sock_len		= 0;
	struct sockaddr_un	sock_addr;
	struct msg_data_s	msg_data;

	if (ctx->dest_user->mail_debug) {
		debug = 1;
		i_info( "push-notify: push notification enabled" );
	}

	if ( strcasecmp(mailbox, "INBOX") != 0) {
		i_info( "push-notify: message saved to mailbox: %s, no notification sent", mailbox );
		return;
	}

	notify_sock = socket( AF_UNIX, SOCK_DGRAM, 0 );
	if ( notify_sock < 0 ) {
		/* warn that connect failed but do not fail the plugin or message will not get delivered */
		i_warning( "push-notify: open socket: \"%s\" failed", sock_path );
		return;
	}

	sock_addr.sun_family = AF_UNIX;
	strlcpy( sock_addr.sun_path, "/var/dovecot/push_notify", sizeof(sock_addr.sun_path) );
	sock_len = sizeof(sock_addr.sun_family) + strlen(sock_addr.sun_path) + 1;
	rc = connect(notify_sock, (struct sockaddr *) &sock_addr,  sock_len);
	if ( rc < 0 ) {
		/* warn that connect failed but do not fail the plugin or message will not get delivered */
		i_warning( "push-notify: connect() to socket: \"%s\" failed: %m", sock_path );
		return;
	}

	memset( &msg_data, 0, sizeof( msg_data ) );
	msg_data.msg = 3;

	/* set user/account id */
	if ( ctx->dest_user->username != NULL ) {
		strlcpy( msg_data.d1, ctx->dest_user->username, sizeof(msg_data.d1) );
		if (debug)
			i_info( "push-notify: notify: %s", msg_data.d1 );
	}

	rc = send(notify_sock, (void *)&msg_data, sizeof(struct msg_data_s), 0);
	if ( rc < 0 )
		i_warning( "push-notify: send() to socket: \"%s\" failed: %m", sock_path );

	if (debug)
		i_info("push-notify: data sent: %lu", rc);

	close(notify_sock);
} // push_notification

static void push_notify_deliver(struct mail_deliver_context *ctx,
				const char *mailbox)
{
	push_notification(ctx, mailbox);
	if (next_deliver_mail != NULL)
		next_deliver_mail(ctx, mailbox);
}

void push_notify_plugin_init (struct module *module ATTR_UNUSED)
{
	next_deliver_mail = deliver_hook;
	deliver_hook = push_notify_deliver;
}

void push_notify_plugin_deinit (void)
{
	deliver_hook = next_deliver_mail;
}
