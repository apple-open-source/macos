/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "ostream.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"

bool cmd_logout(struct client_command_context *cmd)
{
	struct client *client = cmd->client;

	/* no arguments */
	if ( !client_read_no_args(cmd) )
		return FALSE;

	client_send_line(client, "OK \"Logout completed.\"");
	client_disconnect(client, "Logged out");
	return TRUE;
}
