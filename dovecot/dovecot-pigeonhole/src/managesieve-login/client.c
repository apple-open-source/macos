/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "login-common.h"
#include "buffer.h"
#include "ioloop.h"
#include "istream.h"
#include "ostream.h"
#include "safe-memset.h"
#include "str.h"
#include "strescape.h"
#include "master-service.h"
#include "master-auth.h"
#include "auth-client.h"
#include "ssl-proxy.h"

#include "managesieve-parser.h"
#include "managesieve-quote.h"

#include "client.h"
#include "client-authenticate.h"

#include "managesieve-login-settings.h"
#include "managesieve-proxy.h"

#include <stdlib.h>

/* Disconnect client when it sends too many bad commands */
#define CLIENT_MAX_BAD_COMMANDS 3

const struct login_binary login_binary = {
	.protocol = "sieve",
	.process_name = "managesieve-login",
	.default_port = 4190
};

void login_process_preinit(void)
{
	login_set_roots = managesieve_login_settings_set_roots;
}

/* Skip incoming data until newline is found,
   returns TRUE if newline was found. */
bool client_skip_line(struct managesieve_client *client)
{
	const unsigned char *data;
	size_t i, data_size;

	data = i_stream_get_data(client->common.input, &data_size);

	for (i = 0; i < data_size; i++) {
		if (data[i] == '\n') {
			i_stream_skip(client->common.input, i+1);
			return TRUE;
		}
	}

	return FALSE;
}

static void client_send_capabilities(struct client *client)
{
	struct managesieve_client *msieve_client = 
		(struct managesieve_client *) client;
	const char *saslcap;

	T_BEGIN {
		saslcap = client_authenticate_get_capabilities(client);

		/* Default capabilities */
		client_send_raw(client, t_strconcat("\"IMPLEMENTATION\" \"", 
			msieve_client->set->managesieve_implementation_string, "\"\r\n", NULL));
		client_send_raw(client, t_strconcat("\"SIEVE\" \"", 
			msieve_client->set->managesieve_sieve_capability, "\"\r\n", NULL));
		if ( msieve_client->set->managesieve_notify_capability != NULL )
			client_send_raw(client, t_strconcat("\"NOTIFY\" \"", 
				msieve_client->set->managesieve_notify_capability, "\"\r\n", NULL));
		client_send_raw
			(client, t_strconcat("\"SASL\" \"", saslcap, "\"\r\n", NULL));

		/* STARTTLS */
		if (ssl_initialized && !client->tls)
			client_send_raw(client, "\"STARTTLS\"\r\n" );

		/* Protocol version */
		client_send_raw(client, "\"VERSION\" \"1.0\"\r\n");
	} T_END;
}

static int cmd_capability(struct managesieve_client *client)
{
	o_stream_cork(client->common.output);

	client_send_capabilities(&client->common);
	client_send_ok(&client->common, "Capability completed.");

	o_stream_uncork(client->common.output);

	return 1;
}

static int cmd_starttls(struct managesieve_client *client)
{
	client_cmd_starttls(&client->common);
	return 1;
}

static int cmd_noop
(struct managesieve_client *client, struct managesieve_arg *args)
{
	const char *text;
	string_t *resp_code;

	if ( args[0].type == MANAGESIEVE_ARG_EOL ) {
		client_send_ok(&client->common, "NOOP Completed");
		return TRUE;
	}

	if ( args[1].type != MANAGESIEVE_ARG_EOL ) {
		client_send_no(&client->common, "Too many arguments");
		return TRUE;
	}

	if ( (text = managesieve_arg_string(&args[0])) == NULL ) {
		client_send_no(&client->common, "Invalid echo tag.");
		return TRUE;
	}

	resp_code = t_str_new(256);
	str_append(resp_code, "TAG ");
	managesieve_quote_append_string(resp_code, text, FALSE);

	client_send_okresp(&client->common, str_c(resp_code), "Done");
	return TRUE;
}

static int cmd_logout(struct managesieve_client *client)
{
	client_send_ok(&client->common, "Logout completed.");
	client_destroy(&client->common, "Aborted login");
	return 1;
}

/* APPLE - <rdar://problem/9119321> */
static int cmd_forbidden(struct managesieve_client *client)
{
	client_send_bye(&client->common, "Invalid MANAGESIEVE command.");
	client_destroy(&client->common, "Disconnected due to forbidden command");
	return 1;
}

static int client_command_execute
(struct managesieve_client *client, const char *cmd, 
	struct managesieve_arg *args)
{
	cmd = t_str_ucase(cmd);
	if (strcmp(cmd, "AUTHENTICATE") == 0)
		return cmd_authenticate(client, args);
	if (strcmp(cmd, "CAPABILITY") == 0)
		return cmd_capability(client);
	if (strcmp(cmd, "STARTTLS") == 0)
		return cmd_starttls(client);
	if (strcmp(cmd, "NOOP") == 0)
		return cmd_noop(client, args);
	if (strcmp(cmd, "LOGOUT") == 0)
		return cmd_logout(client);

	/* APPLE - <rdar://problem/9119321> */
	if (strcmp(cmd, "CONNECT") == 0 || strcmp(cmd, "GET") == 0 ||
	    strcmp(cmd, "POST") == 0)
		return cmd_forbidden(client);

	return -1;
}

static bool client_handle_input(struct managesieve_client *client)
{
	struct managesieve_arg *args;
	const char *msg;
	int ret;
	bool fatal;

	i_assert(!client->common.authenticating);

	if (client->cmd_finished) {
		/* clear the previous command from memory. don't do this
		   immediately after handling command since we need the
		   cmd_tag to stay some time after authentication commands. */
		client->cmd_name = NULL;
		managesieve_parser_reset(client->parser);

		/* remove \r\n */
		if (client->skip_line) {
			if (!client_skip_line(client))
				return FALSE;
			client->skip_line = FALSE;
		}

		client->cmd_finished = FALSE;
	}

	if (client->cmd_name == NULL) {
		client->cmd_name = managesieve_parser_read_word(client->parser);
		if (client->cmd_name == NULL)
			return FALSE; /* need more data */
	}

	switch (managesieve_parser_read_args(client->parser, 0, 0, &args)) {
	case -1:
		/* error */
		msg = managesieve_parser_get_error(client->parser, &fatal);
		if (fatal) {
			client_send_bye(&client->common, msg);
			client_destroy(&client->common, t_strconcat("Disconnected: ",
				msg, NULL));
			return FALSE;
		}

		client_send_no(&client->common, msg);
		client->cmd_finished = TRUE;
		client->skip_line = TRUE;
		return TRUE;
	case -2:
		/* not enough data */
		return FALSE;
	}
	/* we read the entire line - skip over the CRLF */
	if (!client_skip_line(client))
		i_unreached();

	ret = client_command_execute(client, client->cmd_name, args);

	client->cmd_finished = TRUE;
	if (ret < 0) {
		if (++client->common.bad_counter >= CLIENT_MAX_BAD_COMMANDS) {
			client_send_bye(&client->common,	
				"Too many invalid MANAGESIEVE commands.");
			client_destroy(&client->common, 
				"Disconnected: Too many invalid commands.");
			return FALSE;
		}  
		client_send_no(&client->common,
			"Error in MANAGESIEVE command received by server.");
	}

	return ret != 0 && !client->common.destroyed;
}

static void managesieve_client_input(struct client *client)
{
	struct managesieve_client *managesieve_client = 
		(struct managesieve_client *) client;

	if (!client_read(client))
		return;

	client_ref(client);

	o_stream_cork(managesieve_client->common.output);
	for (;;) {
		if (!auth_client_is_connected(auth_client)) {
			/* we're not currently connected to auth process -
			   don't allow any commands */
			/* FIXME: Can't do untagged responses with managesieve. Any other ways?
			client_send_ok(client, AUTH_SERVER_WAITING_MSG);
			*/
			if (client->to_auth_waiting != NULL)
				timeout_remove(&client->to_auth_waiting);

			client->input_blocked = TRUE;
			break;
		} else {
			if (!client_handle_input(managesieve_client))
				break;
		}
	}
	o_stream_uncork(managesieve_client->common.output);
	client_unref(&client);
}

static struct client *managesieve_client_alloc(pool_t pool)
{
	struct managesieve_client *msieve_client;

	msieve_client = p_new(pool, struct managesieve_client, 1);
	return &msieve_client->common;
}

static void managesieve_client_create
(struct client *client, void **other_sets)
{
	struct managesieve_client *msieve_client = 
		(struct managesieve_client *) client;

	msieve_client->set = other_sets[0];
	msieve_client->parser = managesieve_parser_create
		(msieve_client->common.input, msieve_client->common.output, 
		MAX_MANAGESIEVE_LINE);
	client->io = io_add(client->fd, IO_READ, client_input, client);
}

static void managesieve_client_destroy(struct client *client)
{
	struct managesieve_client *managesieve_client = 
		(struct managesieve_client *) client;

	managesieve_parser_destroy(&managesieve_client->parser);
}

static void managesieve_client_send_greeting(struct client *client)
{
	/* Cork the stream to send the capability data as a single tcp frame
	 *   Some naive clients break if we don't.
	 */
	o_stream_cork(client->output);

	/* Send initial capabilities */   
	client_send_capabilities(client);
	client_send_ok(client, client->set->login_greeting);

	o_stream_uncork(client->output);
}

static void managesieve_client_starttls(struct client *client)
{
	struct managesieve_client *msieve_client = 
		(struct managesieve_client *) client;

	managesieve_parser_destroy(&msieve_client->parser);
	msieve_client->parser =
		managesieve_parser_create(msieve_client->common.input,
				   msieve_client->common.output, MAX_MANAGESIEVE_LINE);

	/* CRLF is lost from buffer when streams are reopened. */
	msieve_client->skip_line = FALSE;

	/* Cork the stream to send the capability data as a single tcp frame
	 *   Some naive clients break if we don't.
	 */
	o_stream_cork(client->output);

	client_send_capabilities(client);
	client_send_ok(client, "TLS negotiation successful.");

	o_stream_uncork(client->output);	
}

void _client_send_response(struct client *client, 
	const char *oknobye, const char *resp_code, const char *msg)
{
	T_BEGIN {
		string_t *line = t_str_new(256);

		str_append(line, oknobye);
		
		if (resp_code != NULL) {
			str_append(line, " [");
			str_append(line, resp_code);
			str_append_c(line, ']');
		}
		
		if ( msg != NULL )	
		{
			str_append_c(line, ' ');
			managesieve_quote_append_string(line, msg, TRUE);
		}

		str_append(line, "\r\n");

		client_send_raw_data(client, str_data(line), str_len(line));
	} T_END;
}

static void managesieve_client_send_line
(struct client *client, enum client_cmd_reply reply, const char *text)
{
	const char *resp_code = NULL;
	const char *prefix = "NO";

	switch (reply) {
	case CLIENT_CMD_REPLY_OK:
		prefix = "OK";
		break;
	case CLIENT_CMD_REPLY_AUTH_FAILED:
		break;
	case CLIENT_CMD_REPLY_AUTHZ_FAILED:
		break;
	case CLIENT_CMD_REPLY_AUTH_FAIL_TEMP:
		resp_code = "TRYLATER";
		break;
	case CLIENT_CMD_REPLY_AUTH_FAIL_REASON:
		break;
	case CLIENT_CMD_REPLY_AUTH_FAIL_NOSSL:
		resp_code = "ENCRYPT-NEEDED";
		break;
	case CLIENT_CMD_REPLY_BAD:
		prefix = "NO";
		break;
	case CLIENT_CMD_REPLY_BYE:
		prefix = "BYE";
		break;
	case CLIENT_CMD_REPLY_STATUS:
		return;
	case CLIENT_CMD_REPLY_STATUS_BAD:
		prefix = "NO";
		break;
	}

	_client_send_response(client, prefix, resp_code, text);
}

void clients_init(void)
{
}

void clients_deinit(void)
{
	clients_destroy_all();
}

struct client_vfuncs client_vfuncs = {
	managesieve_client_alloc,
	managesieve_client_create,
	managesieve_client_destroy,
	managesieve_client_send_greeting,
	managesieve_client_starttls,
	managesieve_client_input,
	managesieve_client_send_line,
	managesieve_client_auth_handle_reply,
	managesieve_client_auth_send_challenge,
	managesieve_client_auth_parse_response,
	managesieve_proxy_reset,
	managesieve_proxy_parse_line
};


