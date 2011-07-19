/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "strfuncs.h"
#include "ostream.h"

#include "sieve.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"

bool cmd_capability(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	const char *sievecap, *notifycap;
	unsigned int max_redirects;

	/* no arguments */
	if ( !client_read_no_args(cmd) )
		return FALSE;

	o_stream_cork(client->output);

	T_BEGIN {
		/* Get capabilities */
		sievecap = sieve_get_capabilities(client->svinst, NULL);
		notifycap = sieve_get_capabilities(client->svinst, "notify");
		max_redirects = sieve_max_redirects(client->svinst);

		/* Default capabilities */
  		client_send_line(client, t_strconcat("\"IMPLEMENTATION\" \"", 
			client->set->managesieve_implementation_string, "\"", NULL));
		client_send_line(client, t_strconcat("\"SIEVE\" \"", 
			( sievecap == NULL ? "" : sievecap ), "\"", NULL));

		/* Maximum number of redirects (if limited) */
		if ( max_redirects > 0 )
			client_send_line(client, 
				t_strdup_printf("\"MAXREDIRECTS\" \"%u\"", max_redirects));

		/* Notify methods */
		if ( notifycap != NULL ) {
			client_send_line(client, t_strconcat("\"NOTIFY\" \"", 
				notifycap, "\"", NULL));
		}

		/* Protocol version */
		client_send_line(client, "\"VERSION\" \"1.0\"");

		/* Finish */
		client_send_line(client, "OK \"Capability completed.\"");
	} T_END;

	o_stream_uncork(client->output);

	return TRUE;

}

