/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"

#include "sieve-storage.h"
#include "sieve-storage-script.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"

bool cmd_renamescript(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct sieve_storage *storage = client->storage;
	const char *scriptname, *newname;
	struct sieve_script *script;

	/* <oldname> <newname> */
	if (!client_read_string_args(cmd, 2, TRUE, &scriptname, &newname))
		return FALSE;

	script = sieve_storage_script_init(storage, scriptname);

	if (script == NULL) {
		client_send_storage_error(client, storage);
		return TRUE;
	}

	if (sieve_storage_script_rename(script, newname) < 0)
		client_send_storage_error(client, storage);
	else    
		client_send_ok(client, "Renamescript completed.");

	return TRUE;
}

