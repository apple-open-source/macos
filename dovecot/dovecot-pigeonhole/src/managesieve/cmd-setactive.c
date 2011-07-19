/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-storage.h"
#include "sieve-storage-script.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"

bool cmd_setactive(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct sieve_storage *storage = client->storage;
	const char *scriptname;
	struct sieve_script *script;;
	int ret;

	/* <scriptname> */
	if ( !client_read_string_args(cmd, 1, TRUE, &scriptname) )
		return FALSE;

	if ( *scriptname != '\0' ) {
		script = sieve_storage_script_init(storage, scriptname);

		if ( script == NULL ) {
			client_send_storage_error(client, storage);
			return TRUE;
		}
	
		ret = sieve_storage_script_activate(script);
		if ( ret < 0 )
			client_send_storage_error(client, storage);
		else
			client_send_ok(client, ret ? 
				"Setactive completed." :
				"Script is already active.");

		sieve_script_unref(&script);
	} else {
		ret = sieve_storage_deactivate(storage);
		
		if ( ret < 0 )
			client_send_storage_error(client, storage);
		else
			client_send_ok(client, ret ?
 				"Active script is now deactivated." :
				"No scripts currently active.");	
	}

	return TRUE;
}
