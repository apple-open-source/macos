/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"

#include "managesieve-quote.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"

#include <stdlib.h>

bool cmd_noop(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct managesieve_arg *args;
	const char *text;
	string_t *resp_code;

	/* [<echo string>] */
	if ( !client_read_args(cmd, 0, 0, FALSE, &args) )
		return FALSE;

	if ( args[0].type == MANAGESIEVE_ARG_EOL ) {
		client_send_ok(client, "NOOP Completed");
		return TRUE;
	}

	if ( (text = managesieve_arg_string(&args[0])) == NULL ) {
		client_send_no(client, "Invalid echo tag.");
		return TRUE;
	}

	if ( args[1].type != MANAGESIEVE_ARG_EOL ) {
		client_send_command_error(cmd, "Too many arguments.");
		return TRUE;
	}

	resp_code = t_str_new(256);
	str_append(resp_code, "TAG ");
	managesieve_quote_append_string(resp_code, text, FALSE);

	client_send_okresp(client, str_c(resp_code), "Done");
	return TRUE;
}

/* APPLE - <rdar://problem/9119321> */
bool cmd_forbidden(struct client_command_context *cmd)
{
	struct client *client = cmd->client;

	client_send_bye(client, "Invalid MANAGESIEVE command.");
	client_disconnect(client, "Disconnected due to forbidden command");
	return TRUE;
}
