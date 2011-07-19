/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "ostream.h"
#include "istream.h"

#include "sieve-storage-script.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"

struct cmd_getscript_context {
	struct client *client;
	struct client_command_context *cmd;
	struct sieve_storage *storage;	
	uoff_t script_size, script_offset;

	struct sieve_script *script;
	struct istream *script_stream;
	
	unsigned int failed:1;
};

static bool cmd_getscript_finish(struct cmd_getscript_context *ctx)
{
	struct client *client = ctx->client;

	if ( ctx->script != NULL )
		sieve_script_unref(&ctx->script);

	if ( ctx->failed ) {
		if ( client->output->closed ) {
			client_disconnect(client, "Disconnected");
			return TRUE;
		}

		client_send_storage_error(client, client->storage);
		return TRUE;
	}

	client_send_line(client, "");
	client_send_ok(client, "Getscript completed.");
	return TRUE;
}

static bool cmd_getscript_continue(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_getscript_context *ctx = cmd->context;
	off_t ret;

	ret = o_stream_send_istream(client->output, ctx->script_stream);

	if ( ret < 0 ) {
		sieve_storage_set_critical(ctx->storage,
			"o_stream_send_istream(%s) failed: %m", 
			sieve_script_filename(ctx->script));
		ctx->failed = TRUE;
		return cmd_getscript_finish(ctx);
	}

	ctx->script_offset += ret;

	if ( ctx->script_offset != ctx->script_size && !ctx->failed ) {
		/* unfinished */
		if ( !i_stream_have_bytes_left(ctx->script_stream) ) {
			/* Input stream gave less data than expected */
			sieve_storage_set_critical(ctx->storage,
				"GETSCRIPT for SCRIPT %s got too little data: "
				"%"PRIuUOFF_T" vs %"PRIuUOFF_T, sieve_script_name(ctx->script),
				ctx->script_offset, ctx->script_size);

			client_disconnect(ctx->client, "GETSCRIPT failed");
			ctx->failed = TRUE;
			return cmd_getscript_finish(ctx);
		}

		return FALSE;
	}

	return cmd_getscript_finish(ctx);
}

bool cmd_getscript(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_getscript_context *ctx;
	const char *scriptname;
	enum sieve_error error;

	/* <scriptname> */
	if ( !client_read_string_args(cmd, 1, TRUE, &scriptname) )
		return FALSE;

	ctx = p_new(cmd->pool, struct cmd_getscript_context, 1);
	ctx->cmd = cmd;
	ctx->client = client;
	ctx->storage = client->storage;
	ctx->failed = FALSE;
	ctx->script = sieve_storage_script_init(client->storage, scriptname);

	if (ctx->script == NULL) {
		ctx->failed = TRUE;
		return cmd_getscript_finish(ctx);
	}
			
	ctx->script_stream = sieve_script_open(ctx->script, &error);

	if ( ctx->script_stream == NULL ) {
		ctx->failed = TRUE;
		if ( error == SIEVE_ERROR_NOT_FOUND )
			sieve_storage_set_error(client->storage, error, "Script does not exist.");		
		return cmd_getscript_finish(ctx);
	}

	ctx->script_size = sieve_script_get_size(ctx->script);
	ctx->script_offset = 0;	

	client_send_line
		(client, t_strdup_printf("{%"PRIuUOFF_T"}", ctx->script_size));

	client->command_pending = TRUE;
	cmd->func = cmd_getscript_continue;
	cmd->context = ctx;

	return cmd_getscript_continue(cmd);
}
