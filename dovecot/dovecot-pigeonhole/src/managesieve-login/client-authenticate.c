/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "login-common.h"
#include "base64.h"
#include "buffer.h"
#include "ioloop.h"
#include "istream.h"
#include "ostream.h"
#include "safe-memset.h"
#include "str.h"
#include "str-sanitize.h"
#include "auth-client.h"

#include "managesieve-parser.h"
#include "managesieve-quote.h"
#include "client.h"

#include "client-authenticate.h"
#include "managesieve-proxy.h"

#include <stdlib.h>

const char *client_authenticate_get_capabilities
(struct client *client)
{
	const struct auth_mech_desc *mech;
	unsigned int i, count;
	bool first = TRUE;
	string_t *str;

	str = t_str_new(128);
	mech = sasl_server_get_advertised_mechs(client, &count);

	for (i = 0; i < count; i++) {
		/* Filter ANONYMOUS mechanism, ManageSieve has no use-case for it */
		if ( (mech[i].flags & MECH_SEC_ANONYMOUS) == 0 ) {
			if ( !first )
				str_append_c(str, ' ');
			else
				first = FALSE;

			str_append(str, mech[i].name);
		}
	}

	return str_c(str);
}

bool managesieve_client_auth_handle_reply
(struct client *client, const struct client_auth_reply *reply)
{
	struct managesieve_client *msieve_client =
		(struct managesieve_client *) client;

	if ( reply->host != NULL ) {
		string_t *resp_code;
		const char *reason;

		/* MANAGESIEVE referral

		   [nologin] referral host=.. [port=..] [destuser=..]
		   [reason=..]

		   NO [REFERRAL sieve://user;AUTH=mech@host:port/] "Can't login."
		   OK [...] "Logged in, but you should use this server instead."
		   .. [REFERRAL ..] Reason from auth server
		*/
		resp_code = t_str_new(128);
		str_printfa(resp_code, "REFERRAL sieve://%s;AUTH=%s@%s",
			    reply->destuser, client->auth_mech_name, reply->host);
		if ( reply->port != 4190 )
			str_printfa(resp_code, ":%u", reply->port);

		if ( reply->reason == NULL ) {
			if ( reply->nologin )
				reason = "Try this server instead.";
			else 
				reason = "Logged in, but you should use "
					"this server instead.";
		} else {
			reason = reply->reason;
		}

		if ( !reply->nologin ) {
			client_send_okresp(client, str_c(resp_code), reason);
			client_destroy_success(client, "Login with referral");
			return TRUE;
 		}
		client_send_noresp(client, str_c(resp_code), reason);
	} else if ( reply->nologin ) {
		/* Authentication went ok, but for some reason user isn't
		   allowed to log in. Shouldn't probably happen. */
		if (reply->reason != NULL) {
			client_send_line(client,
					 CLIENT_CMD_REPLY_AUTH_FAIL_REASON,
					 reply->reason);
		} else if (reply->temp) {
			client_send_line(client,
					 CLIENT_CMD_REPLY_AUTH_FAIL_TEMP,
					 AUTH_TEMP_FAILED_MSG);
		} else if (reply->authz_failure) {
			client_send_line(client, CLIENT_CMD_REPLY_AUTHZ_FAILED,
					 "Authorization failed");
		} else {
			client_send_line(client, CLIENT_CMD_REPLY_AUTH_FAILED,
					 AUTH_FAILED_MSG);
		}
	} else {
		/* normal login/failure */
		return FALSE;
	}

	i_assert(reply->nologin);

	managesieve_parser_reset(msieve_client->parser);

	if ( !client->destroyed ) 
		client_auth_failed(client);
	return TRUE;
}

void managesieve_client_auth_send_challenge
(struct client *client, const char *data)
{
	struct managesieve_client *msieve_client =
		(struct managesieve_client *) client;

	T_BEGIN {
		string_t *str = t_str_new(256);

		managesieve_quote_append_string(str, data, TRUE);
		str_append(str, "\r\n");

		client_send_raw_data(client, str_c(str), str_len(str));
	} T_END;

	managesieve_parser_reset(msieve_client->parser);
}

int managesieve_client_auth_parse_response(struct client *client)
{
	struct managesieve_client *msieve_client =
		(struct managesieve_client *) client;
	struct managesieve_arg *args;
	const char *msg;
	bool fatal;

	if ( i_stream_read(client->input) == -1 ) {	
		/* disconnected */
		client_destroy(client, "Disconnected");
		return -1;
	}

	if ( msieve_client->skip_line ) {
		if ( i_stream_next_line(client->input) == NULL )
			return 0;

		msieve_client->skip_line = FALSE;
	}

	switch ( managesieve_parser_read_args(msieve_client->parser, 0, 0, &args) ) {
	case -1:
		/* error */
		msg = managesieve_parser_get_error(msieve_client->parser, &fatal);
		if (fatal) {
			/* FIXME: What to do? */
		}

		if ( i_stream_next_line(client->input) == NULL )
			msieve_client->skip_line = TRUE;
		sasl_server_auth_failed(client, msg);
		return -1;
	case -2:
		/* not enough data */
		return 0;
	}
	
	if ( i_stream_next_line(client->input) == NULL )
		msieve_client->skip_line = TRUE;

	if ( args[0].type != MANAGESIEVE_ARG_STRING || 
		args[1].type != MANAGESIEVE_ARG_EOL ) {
		sasl_server_auth_failed(client, "Invalid AUTHENTICATE client response.");
		return -1;
	}

	str_append(client->auth_response, MANAGESIEVE_ARG_STR(&args[0]));

	if ( strcmp(str_c(client->auth_response), "*") == 0 ) {
		sasl_server_auth_abort(client);
		return -1;
	}

	return 1;
}

int cmd_authenticate
(struct managesieve_client *msieve_client, struct managesieve_arg *args)
{
	const char *mech_name, *init_resp = NULL;
	int ret;

	/* one mandatory argument: authentication mechanism name */
	if (args[0].type != MANAGESIEVE_ARG_STRING)
		return -1;
	if (args[1].type != MANAGESIEVE_ARG_EOL) {
		/* optional SASL initial response */
		if (args[1].type != MANAGESIEVE_ARG_STRING ||
		    args[2].type != MANAGESIEVE_ARG_EOL)
			return -1;
		init_resp = MANAGESIEVE_ARG_STR(&args[1]);
	}

	mech_name = MANAGESIEVE_ARG_STR(&args[0]);
	if (*mech_name == '\0') 
		return -1;

	/* Refuse the ANONYMOUS mechanism. */
	if ( strncasecmp(mech_name, "ANONYMOUS", 9) == 0 ) {
		client_send_no
			(&msieve_client->common, "ANONYMOUS login is not allowed.");
		return 0;
	}

	ret = client_auth_begin(&msieve_client->common, mech_name, init_resp);

	managesieve_parser_reset(msieve_client->parser);

	return ret;
}

