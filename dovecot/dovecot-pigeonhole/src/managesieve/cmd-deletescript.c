/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-storage.h"
#include "sieve-storage-script.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"

bool cmd_deletescript(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct sieve_storage *storage = client->storage;
	const char *scriptname;
	struct sieve_script *script;

	/* <scrip name>*/
	if ( !client_read_string_args(cmd, 1, TRUE, &scriptname) )
		return FALSE;

	script = sieve_storage_script_init(storage, scriptname);

	if (script == NULL) {
		client_send_storage_error(client, storage);
		return TRUE;
	}

	if (sieve_storage_script_delete(&script) < 0) {
		client_send_storage_error(client, storage);
	} else {
		client_send_ok(client, "Deletescript completed.");
	}

	/* Script object is deleted no matter what in 
	 * sieve_script_delete()
	 */

	return TRUE;
}
