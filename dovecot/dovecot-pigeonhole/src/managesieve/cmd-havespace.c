/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"

#include "sieve.h"
#include "sieve-script.h"

#include "sieve-storage-quota.h"

#include "managesieve-client.h"
#include "managesieve-quota.h"

bool cmd_havespace(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct managesieve_arg *args;
	const char *scriptname;
	uoff_t size;

	/* <scriptname> <size> */
	if ( !client_read_args(cmd, 2, 0, TRUE, &args) )
	  return FALSE;

	if ( (scriptname = managesieve_arg_string(&args[0])) == NULL ) {
		client_send_no(client, "Invalid string for scriptname.");
		return TRUE;
	}

	if ( managesieve_arg_number(&args[1], &size) < 0 ) {
		client_send_no(client, "Invalid scriptsize argument.");
		return TRUE;
	}

	if ( !sieve_script_name_is_valid(scriptname) ) {
		client_send_no(client, "Invalid script name.");
		return TRUE;
	}

	if ( size == 0 ) {
		client_send_no(client, "Cannot upload empty script.");
		return TRUE;
	}

	if ( !managesieve_quota_check_all(client, scriptname, size) )
		return TRUE;

	client_send_ok(client, "Putscript would succeed.");
	return TRUE;
}
